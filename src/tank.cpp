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
    for (int i = 0; i < fishCount; ++i)
    {
        TankFish& f = fish_[size_t(i)];
        f.species   = i < kSpeciesCount ? startOrder_[size_t(i)] : std::string("Random");
        InitFish(f, rng);
    }

    pending_.clear();
    pending_.resize(size_t(fishCount));
    stop_.store(false);
    workers_.emplace_back(&Tank::WorkerRange, this, 0, fishCount, false, sea::u64(0), &startOrder_);
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

bool Tank::AddFish()
{
    if (fish_.empty() || int(fish_.size()) >= kMaxTankFish)
        return false;   // not running, or the tank is already full

    const int index = int(fish_.size());

    // Fresh entropy per click: the newcomer is not tied to the tank seed or
    // to how many fish already exist, so no run of clicks ever repeats.
    const sea::u64 salt = salty_();

    std::mt19937 rng(uint32_t(salt >> 32) ^ uint32_t(salt));

    TankFish added;
    added.species = "Random";
    InitFish(added, rng);

    // Unready fish are skipped by Update and Draw, so the newcomer is invisible
    // until its sheet lands - and the tank never pauses in the meantime.
    fish_.push_back(std::move(added));
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_.resize(fish_.size());
    }

    workers_.emplace_back(&Tank::WorkerRange, this, index, 1, true, salt,
                          static_cast<const std::vector<const char*>*>(nullptr));
    return true;
}

void Tank::WorkerRange(int first, int count, bool randomSpecies, sea::u64 randomSeed,
                       const std::vector<const char*>* presets)
{
    for (int n = 0; n < count && !stop_.load(); ++n)
    {
        const int index = first + n;

        // Random fish take the seed AddFish() drew from its entropy source,
        // so two clicks can never agree; preset fish stay keyed to the tank
        // seed, where the index spread keeps same-species twins apart.
        const sea::u64 fishSeed = randomSpecies
            ? randomSeed
            : seed_ + sea::u64(index) * 37 + 1;

        sea::SeaGenome genome;
        if (presets != nullptr && index < int(presets->size()))
            genome = sea::MakeGenome((*presets)[size_t(index)], fishSeed);
        else
        {
            genome = sea::RandomGenome(sea::SeaKind((unsigned(fishSeed) >> 3) % 7u), fishSeed);
        }

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

    for (TankFish& f : fish_)
    {
        if (!f.ready) continue;

        const ClipView c = LookupClip(f.sheet, f.clip);
        f.clock += dt;
        const int raw = int(f.clock * c.fps);
        f.frame = c.loops ? (raw % std::max(c.frames, 1))
                          : std::min(raw, std::max(c.frames - 1, 0));

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
            f.x += f.dir * f.speed * dt;
            const double margin = 78.0;
            if (f.dir > 0 && f.x > double(w_) - margin)
            {
                f.clip = sea::ClipTurnRightToLeft;   // faces left after the sweep
                f.turning = true;
                f.clock = 0.0;
                f.frame = 0;
            }
            else if (f.dir < 0 && f.x < margin)
            {
                f.clip = sea::ClipTurnLeftToRight;
                f.turning = true;
                f.clock = 0.0;
                f.frame = 0;
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
