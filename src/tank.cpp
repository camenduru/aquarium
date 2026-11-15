// ================================================================
// AQUARIUM - tank implementation
// ================================================================

#include "tank.h"

#include "composite.h"
#include "creature/sea/SeaSpecies.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>

namespace
{

// Species that read clearly at 64 px and between them cover every major
// shape the generator has: fish, a ray, a shark, a cephalopod, a puffer.
const char* const kSpecies[] = {
    "Clownfish",
    "Koi",
    "Yellow tang",
    "Pufferfish",
    "Manta ray",
    "Stingray",
    "Giant squid",
    "Octopus",
};
constexpr int kSpeciesCount = int(sizeof(kSpecies) / sizeof(kSpecies[0]));

constexpr int kCell       = 64;   // sprite cell
constexpr int kSmoothness = 2;    // 16 swim frames / 20 turn frames

// A startled fish flees for this long, easing from its escape speed back to
// cruise; the speed itself is water.ini's `escape_speed` (default 2.8x).
constexpr double kAlarmSeconds = 1.6;

// Room (window px) a fleeing fish wants on its chosen side; with less, it
// picks the other side rather than turning around a moment later.
constexpr double kEscapeRoom = 40.0;

// The kinds in SeaKind order, spelled for a save file. Keep this next to the
// enum's order; PickWeightedKind() indexes the weight table with the same order.
const char* const kKindNames[sea::kKindCount] = {
    "Fish", "Shark", "Whale", "Ray", "Squid", "Octopus", "Urchin"
};

/// The genome a (species, seed) pair names. A named preset pins its genes; a
/// "Random:<Kind>" fish takes a seed-drawn animal of that kind, and the bare
/// "Random" of an older save still picks its kind from the seed, exactly as
/// the spawn path used to do - so storing the pair is enough to rebuild the
/// same animal.
sea::SeaGenome GenomeFor(const std::string& species, sea::u64 seed)
{
    sea::SeaKind kind;
    if (ParseRandomSpecies(species, kind))
        return sea::MakeGenome(sea::kRandomSpecies, seed, nullptr, kind);
    if (species.empty() || species == sea::kRandomSpecies)
        return sea::RandomGenome(sea::SeaKind((unsigned(seed) >> 3) % 7u), seed);
    return sea::MakeGenome(species, seed);
}

struct ClipView
{
    int    row    = 0;
    int    frames = 1;
    double fps    = 10.0;
    bool   loops  = false;
};

ClipView LookupClip(const sea::SeaSheet& sheet, int clip)
{
    ClipView v;
    for (int i = 0; i < sea::ClipCount; ++i)
    {
        const sea::SeaSheetClip& c = sheet.clips[i];
        if (c.clip == clip && c.frames > 0)
        {
            v.row    = c.row;
            v.frames = c.frames;
            v.fps    = c.fps > 0.0 ? c.fps : 10.0;
            v.loops  = c.loops;
            return v;
        }
    }
    return v;
}

} // namespace

const char* KindName(sea::SeaKind kind)
{
    const int i = int(kind);
    return (i >= 0 && i < sea::kKindCount) ? kKindNames[i] : nullptr;
}

std::string RandomSpeciesName(sea::SeaKind kind)
{
    const char* name = KindName(kind);
    if (name == nullptr) return sea::kRandomSpecies;
    return std::string(sea::kRandomSpecies) + ":" + name;
}

bool ParseRandomSpecies(const std::string& species, sea::SeaKind& kind)
{
    if (species.rfind("Random:", 0) != 0) return false;
    const std::string tail = species.substr(7);
    for (int i = 0; i < sea::kKindCount; ++i)
    {
        if (tail == kKindNames[i])
        {
            kind = sea::SeaKind(i);
            return true;
        }
    }
    return false;
}

int PickWeightedKind(const double (&weights)[sea::kKindCount], double u01)
{
    double total = 0.0;
    for (double w : weights)
        total += std::max(0.0, w);

    // Every weight at zero would make a random fish impossible; fall back to
    // even odds instead of refusing, so an INI with all zeros is not a trap.
    if (total <= 0.0)
        return std::clamp(int(u01 * double(sea::kKindCount)), 0, sea::kKindCount - 1);

    double left = std::clamp(u01, 0.0, 0.999999999) * total;
    for (int i = 0; i < sea::kKindCount; ++i)
    {
        left -= std::max(0.0, weights[i]);
        if (left < 0.0) return i;
    }

    // Rounding tail: the last kind with any weight.
    for (int i = sea::kKindCount - 1; i >= 0; --i)
        if (weights[i] > 0.0) return i;
    return 0;
}

void Tank::SetKindWeights(const double (&weights)[sea::kKindCount])
{
    for (int i = 0; i < sea::kKindCount; ++i)
        kindWeight_[i] = std::max(0.0, weights[i]);
}

void Tank::InitFish(TankFish& f, std::mt19937& rng)
{
    std::uniform_real_distribution<double> ux(90.0, std::max(140.0, double(w_ - 90)));
    std::uniform_real_distribution<double> uy(double(h_) * 0.20, double(h_) * 0.86);
    std::uniform_real_distribution<double> us(26.0, 56.0);
    std::uniform_real_distribution<double> ub(1.5, 3.5);
    std::uniform_real_distribution<double> u01(0.0, 1.0);

    f.x        = ux(rng);
    f.baseY    = uy(rng);
    f.y        = f.baseY;
    f.speed    = us(rng);
    f.dir      = (rng() & 1u) ? 1 : -1;
    f.clip     = f.dir > 0 ? sea::ClipSwimRight : sea::ClipSwimLeft;
    f.turning  = false;
    f.clock    = u01(rng) * 1.2;
    f.frame    = 0;
    f.bobPhase = u01(rng) * 6.283185307179586;
    f.bobAmp   = ub(rng);
    f.bobSpeed = 0.55 + 0.75 * u01(rng);
    f.scale    = 2;
    f.alpha    = 230 + int(u01(rng) * 25.0);
    f.ready    = false;
}

void Tank::Start(int width, int height, sea::u64 seed, int fishCount)
{
    Stop();

    w_ = std::max(width, 64);
    h_ = std::max(height, 64);
    seed_ = seed;
    fishCount = std::clamp(fishCount, 1, kMaxTankFish);

    std::mt19937 rng(uint32_t(seed ^ 0x9E3779B97F4A7C15ull));

    // Shuffle the preset pool once per launch: which fish the tank opens
    // with is then random every start instead of always the same species.
    // Seeds from fresh launches are random too, so the genomes differ as well.
    startOrder_.assign(kSpecies, kSpecies + kSpeciesCount);
    std::shuffle(startOrder_.begin(), startOrder_.end(), rng);

    fish_.assign(size_t(fishCount), TankFish());
    std::vector<CreatureJob> jobs;
    jobs.reserve(size_t(fishCount));
    for (int i = 0; i < fishCount; ++i)
    {
        TankFish& f = fish_[size_t(i)];
        // The pool covers the first few; the rest are random animals, and
        // their kind follows water.ini's weights.
        f.species   = i < kSpeciesCount
                    ? startOrder_[size_t(i)]
                    : RandomSpeciesName(sea::SeaKind(
                          PickWeightedKind(kindWeight_, double(rng()) / 4294967296.0)));
        f.seed      = seed_ + sea::u64(i) * 37 + 1;
        InitFish(f, rng);
        jobs.push_back(CreatureJob{ f.species, f.seed });
    }

    pending_.clear();
    pending_.resize(size_t(fishCount));
    stop_.store(false);
    workers_.emplace_back(&Tank::WorkerRange, this, 0, std::move(jobs));
}

void Tank::Stop()
{
    stop_.store(true);
    for (std::thread& t : workers_)
        if (t.joinable())
            t.join();
    workers_.clear();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_.clear();
        fish_.clear();
    }
}

void Tank::Resize(int width, int height)
{
    w_ = std::max(width, 64);
    h_ = std::max(height, 64);
    for (TankFish& f : fish_)
    {
        f.x = std::clamp(f.x, 80.0, std::max(100.0, double(w_ - 80)));
        f.baseY = std::clamp(f.baseY, double(h_) * 0.15, double(h_) * 0.9);
    }
}

std::vector<TankCreature> Tank::Snapshot() const
{
    std::vector<TankCreature> out;
    out.reserve(fish_.size());
    for (const TankFish& f : fish_)
    {
        TankCreature c;
        c.species  = f.species;
        c.seed     = f.seed;
        c.x        = f.x;
        c.baseY    = f.baseY;
        c.speed    = f.speed;
        c.dir      = f.dir;
        c.bobPhase = f.bobPhase;
        c.bobAmp   = f.bobAmp;
        c.bobSpeed = f.bobSpeed;
        c.alpha    = f.alpha;
        out.push_back(std::move(c));
    }
    return out;
}

bool Tank::Restore(int width, int height, const std::vector<TankCreature>& creatures)
{
    if (int(creatures.size()) > kMaxTankFish)
        return false;

    Stop();

    w_ = std::max(width, 64);
    h_ = std::max(height, 64);

    fish_.assign(creatures.size(), TankFish());
    std::vector<CreatureJob> jobs;
    jobs.reserve(creatures.size());
    for (size_t i = 0; i < creatures.size(); ++i)
    {
        const TankCreature& c = creatures[i];
        TankFish& f = fish_[i];
        f.species  = c.species.empty() ? std::string(sea::kRandomSpecies) : c.species;
        f.seed     = c.seed;
        // Same bounds Resize() applies, so a save from a larger window still
        // lands inside this one instead of swimming off-screen.
        f.x        = std::clamp(c.x, 80.0, std::max(100.0, double(w_ - 80)));
        f.baseY    = std::clamp(c.baseY, double(h_) * 0.15, double(h_) * 0.9);
        f.y        = c.baseY;
        f.speed    = c.speed;
        f.dir      = c.dir >= 0 ? 1 : -1;
        f.clip     = f.dir > 0 ? sea::ClipSwimRight : sea::ClipSwimLeft;
        f.turning  = false;
        f.clock    = 0.0;
        f.frame    = 0;
        f.bobPhase = c.bobPhase;
        f.bobAmp   = c.bobAmp;
        f.bobSpeed = c.bobSpeed;
        f.scale    = 2;
        f.alpha    = std::clamp(c.alpha, 0, 255);
        f.ready    = false;
        jobs.push_back(CreatureJob{ f.species, f.seed });
    }

    pending_.clear();
    pending_.resize(fish_.size());
    stop_.store(false);
    // An emptied tank restores as an emptied tank: no worker, nothing to
    // build, and AddFish() can refill it.
    if (!jobs.empty())
        workers_.emplace_back(&Tank::WorkerRange, this, 0, std::move(jobs));
    return true;
}

bool Tank::AddFish()
{
    // An emptied tank still accepts fish: removing the last one leaves the
    // reef and the water, and the school can be rebuilt from here.
    if (w_ < 1 || stop_.load() || int(fish_.size()) >= kMaxTankFish)
        return false;   // not running, or the tank is already full

    const int index = int(fish_.size());

    // Fresh entropy per click: the newcomer is not tied to the tank seed or
    // to how many fish already exist, so no run of clicks ever repeats.
    const sea::u64 salt = salty_();

    std::mt19937 rng(uint32_t(salt >> 32) ^ uint32_t(salt));

    TankFish added;
    added.species = RandomSpeciesName(sea::SeaKind(PickWeightedKind(
        kindWeight_, double(salt >> 11) * (1.0 / 9007199254740992.0))));
    added.seed    = salt;
    InitFish(added, rng);

    // Unready fish are skipped by Update and Draw, so the newcomer is invisible
    // until its sheet lands - and the tank never pauses in the meantime.
    fish_.push_back(std::move(added));
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_.resize(fish_.size());
    }

    workers_.emplace_back(&Tank::WorkerRange, this, index,
                          std::vector<CreatureJob>{ CreatureJob{ "Random", salt } });
    return true;
}

void Tank::WorkerRange(int first, std::vector<CreatureJob> jobs)
{
    for (int n = 0; n < int(jobs.size()) && !stop_.load(); ++n)
    {
        const int index = first + n;

        // The job carries the species and the seed the fish was spawned with,
        // so a restored tank reproduces the same sheet without re-deriving
        // either from the tank seed.
        const sea::SeaGenome genome = GenomeFor(jobs[size_t(n)].species, jobs[size_t(n)].seed);

        sea::SeaSpriteOptions options;
        options.size       = kCell;
        options.smoothness = kSmoothness;
        options.outline    = true;

        auto sheet = std::make_unique<sea::SeaSheet>();
        if (!sea::BuildSeaSheet(genome, options, *sheet))
            continue;   // leave the slot empty: the fish simply never shows

        std::lock_guard<std::mutex> lock(mutex_);
        if (size_t(index) < pending_.size())
            pending_[size_t(index)] = std::move(sheet);
    }
}

void Tank::AdoptPending()
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t i = 0; i < fish_.size() && i < pending_.size(); ++i)
    {
        if (!pending_[i]) continue;
        fish_[i].sheet = std::move(*pending_[i]);
        pending_[i].reset();
        fish_[i].ready = true;
        fish_[i].clock = 0.0;
        fish_[i].frame = 0;
    }
}

int Tank::ReadyCount()
{
    AdoptPending();
    int n = 0;
    for (const TankFish& f : fish_)
        if (f.ready) ++n;
    return n;
}

bool Tank::WaitAllReady(int timeoutMs)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    for (;;)
    {
        if (ReadyCount() == FishCount()) return true;
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

void Tank::Update(double dt)
{
    if (dt <= 0.0) return;
    dt = std::min(dt, 0.1);   // a stalled frame must not teleport a fish

    AdoptPending();

    rippleCooldown_ = std::max(0.0, rippleCooldown_ - dt);

    for (TankFish& f : fish_)
    {
        if (!f.ready) continue;

        const ClipView c = LookupClip(f.sheet, f.clip);
        f.clock += dt;
        const int raw = int(f.clock * c.fps);
        f.frame = c.loops ? (raw % std::max(c.frames, 1))
                          : std::min(raw, std::max(c.frames - 1, 0));

        f.alarm = std::max(0.0, f.alarm - dt);

        if (f.turning)
        {
            const double span = double(c.frames) / c.fps;
            if (f.clock >= span)
            {
                f.dir     = -f.dir;
                f.clip    = f.dir > 0 ? sea::ClipSwimRight : sea::ClipSwimLeft;
                f.turning = false;
                f.clock   = 0.0;
                f.frame   = 0;
            }
        }
        else
        {
            // A startled fish leaves in a hurry: the click's alarm multiplies
            // the cruise speed, and eases back to it as the alarm runs out.
            const double rush =
                1.0 + (escapeSpeed_ - 1.0) * (f.alarm / kAlarmSeconds);
            f.x += f.dir * f.speed * rush * dt;
            const double margin = 78.0;
            if (f.dir > 0 && f.x > double(w_) - margin)
            {
                f.clip = sea::ClipTurnRightToLeft;   // faces left after the sweep
                f.turning = true;
                f.clock = 0.0;
                f.frame = 0;
                EmitRipple(f);
            }
            else if (f.dir < 0 && f.x < margin)
            {
                f.clip = sea::ClipTurnLeftToRight;
                f.turning = true;
                f.clock = 0.0;
                f.frame = 0;
                EmitRipple(f);
            }
        }

        f.y = f.baseY + f.bobAmp * std::sin(f.clock * f.bobSpeed * 2.0 + f.bobPhase);
    }
}

void Tank::Draw(uint8_t* canvas, int width, int height)
{
    if (canvas == nullptr || width < 1 || height < 1) return;

    for (const TankFish& f : fish_)
    {
        if (!f.ready || f.sheet.rgba.empty() || f.sheet.size < 1) continue;

        const ClipView c    = LookupClip(f.sheet, f.clip);
        const int      cell = f.sheet.size;
        const int      cols = f.sheet.width / cell;
        if (cols < 1) continue;

        const int col = std::clamp(f.frame, 0, cols - 1);
        const int sx  = col * cell;
        const int sy  = c.row * cell;
        if (sx + cell > f.sheet.width || sy + cell > f.sheet.height) continue;

        const int size   = cell * f.scale;
        const int dstX   = int(std::lround(f.x)) - size / 2;
        const int dstY   = int(std::lround(f.y)) - size / 2;

        BlendSprite(canvas, width, height,
                    f.sheet.rgba.data(), f.sheet.width, f.sheet.height,
                    sx, sy, cell, cell,
                    dstX, dstY, f.scale, f.alpha);
    }
}

void Tank::SetEscapeSpeed(double multiplier)
{
    escapeSpeed_ = std::clamp(multiplier, 1.0, 6.0);
}

int Tank::FishAt(double x, double y) const
{
    // Topmost first: the last drawn fish is the one under the pointer.
    for (int i = int(fish_.size()) - 1; i >= 0; --i)
    {
        const TankFish& f = fish_[size_t(i)];
        if (!f.ready || f.sheet.rgba.empty() || f.sheet.size < 1) continue;

        const ClipView c    = LookupClip(f.sheet, f.clip);
        const int      cell = f.sheet.size;
        const int      cols = f.sheet.width / cell;
        if (cols < 1) continue;

        const int size = cell * f.scale;
        const int dstX = int(std::lround(f.x)) - size / 2;
        const int dstY = int(std::lround(f.y)) - size / 2;

        const int px = int(std::floor(x)) - dstX;
        const int py = int(std::floor(y)) - dstY;
        if (px < 0 || py < 0 || px >= size || py >= size) continue;

        // The same nearest-neighbour mapping the blit uses, so the alpha test
        // answers for exactly the pixels that were drawn.
        const int col = std::clamp(f.frame, 0, cols - 1);
        const int sx  = col * cell + px / f.scale;
        const int sy  = c.row * cell + py / f.scale;
        if (sx >= f.sheet.width || sy >= f.sheet.height) continue;

        const size_t o = (size_t(sy) * size_t(f.sheet.width) + size_t(sx)) * 4;
        if (f.sheet.rgba[o + 3] < 24) continue;   // empty corner of the cell

        return i;
    }
    return -1;
}

bool Tank::RemoveFish(int index)
{
    if (index < 0 || index >= int(fish_.size())) return false;

    // pending_ is positional: a worker still building a sheet for slot k must
    // not find slot k pointing at a different fish when it lands.
    if (ReadyCount() != FishCount()) return false;

    std::lock_guard<std::mutex> lock(mutex_);
    fish_.erase(fish_.begin() + index);
    pending_.clear();
    pending_.resize(fish_.size());
    return true;
}

int Tank::PokeAt(double x, double y)
{
    const int i = FishAt(x, y);
    if (i < 0) return -1;

    TankFish& f = fish_[size_t(i)];

    // Away from the pointer; if that side is a wall away, leave the other
    // way. A fish cornered by the hand still gets out.
    int away = x < f.x ? 1 : -1;
    const double room = away > 0 ? double(w_) - 78.0 - f.x : f.x - 78.0;
    if (room < kEscapeRoom) away = -away;

    f.dir     = away;
    f.clip    = away > 0 ? sea::ClipSwimRight : sea::ClipSwimLeft;
    f.turning = false;
    f.clock   = 0.0;
    f.frame   = 0;
    f.alarm   = kAlarmSeconds;
    return i;
}

void Tank::EmitRipple(const TankFish& f)
{
    if (!onRipple || rippleCooldown_ > 0.0) return;
    rippleCooldown_ = 0.18;
    onRipple(f.x, f.y, 0.55);
}

void Tank::DrawLogical(uint8_t* canvas, int logicalW, int logicalH, uint8_t* hit) const
{
    if (canvas == nullptr || logicalW < 1 || logicalH < 1) return;

    // Fish coordinates stay in window pixels: update, seeding and the turn
    // margins are all expressed there. Only the blit is converted, so a fish
    // is drawn at exactly the same place it always was - one logical pixel
    // is two window pixels.
    for (const TankFish& f : fish_)
    {
        if (!f.ready || f.sheet.rgba.empty() || f.sheet.size < 1) continue;

        const ClipView c    = LookupClip(f.sheet, f.clip);
        const int      cell = f.sheet.size;
        const int      cols = f.sheet.width / cell;
        if (cols < 1) continue;

        const int col = std::clamp(f.frame, 0, cols - 1);
        const int sx  = col * cell;
        const int sy  = c.row * cell;
        if (sx + cell > f.sheet.width || sy + cell > f.sheet.height) continue;

        const int dstX = int(std::lround(f.x * 0.5)) - cell / 2;
        const int dstY = int(std::lround(f.y * 0.5)) - cell / 2;

        BlendSpriteLogical(canvas, logicalW, logicalH,
                           f.sheet.rgba.data(), f.sheet.width, f.sheet.height,
                           sx, sy, cell, cell,
                           dstX, dstY, f.alpha, hit);
    }
}
