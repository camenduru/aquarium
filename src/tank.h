#pragma once
// ================================================================
// AQUARIUM - the tank: fish agents playing the generator's clips
// ----------------------------------------------------------------
// Every fish is one sprite sheet from the sea generator (a row per clip,
// a column per frame). An agent only decides where it is and which clip
// it is in; the sheet already contains the right-facing swim, the
// left-facing swim and both turns, so nothing is mirrored at runtime.
//
// Sheets are built on worker threads: a 64 px sheet is seconds of
// supersampled rendering, and the window must appear immediately, so
// fish arrive one at a time instead of blocking startup. AddFish() takes
// the same route, so a newcomer is generated while the rest of the tank
// keeps swimming.
// ================================================================

#include "creature/sea/SeaSprite.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

/// Ceiling for AddFish(), and the limit Start() and --fish clamp to, so all
/// three agree. Every fish is one resident sheet, so this is a memory and
/// generation-time budget as much as a taste one.
constexpr int kMaxTankFish = 128;

struct TankFish
{
    sea::SeaSheet sheet;
    bool     ready  = false;
    std::string species;
    sea::u64 seed   = 0;     // the seed this fish's genome was built from

    double x = 0.0;          // centre, in window pixels
    double baseY = 0.0;      // bobbing centre
    double y = 0.0;          // drawn centre (baseY + bob)
    double speed = 40.0;     // pixels per second

    int    dir = 1;          // +1 faces right, -1 faces left
    int    clip = sea::ClipSwimRight;
    bool   turning = false;
    double clock = 0.0;      // seconds into the current clip
    int    frame = 0;

    double bobPhase = 0.0;
    double bobAmp = 3.0;
    double bobSpeed = 1.0;

    // Startled: a click on this fish sets the alarm and it darts away from the
    // pointer until it runs out. Not saved - a poke is an event, not the tank.
    double alarm = 0.0;

    int    scale = 2;        // nearest-neighbour upscale
    int    alpha = 235;      // depth
};

/// One fish as a save file sees it: species and genome seed rebuild the sheet
/// exactly, and the agent fields put it back where it was swimming.
struct TankCreature
{
    std::string species;
    sea::u64 seed  = 0;
    double x       = 0.0;
    double baseY   = 0.0;
    double speed   = 40.0;
    int    dir     = 1;
    double bobPhase = 0.0;
    double bobAmp   = 3.0;
    double bobSpeed = 1.0;
    int    alpha    = 235;
};

class Tank
{
public:
    ~Tank() { Stop(); }

    /// Rebuild the tank: spawn `fishCount` agents and start generating sheets.
    void Start(int width, int height, sea::u64 seed, int fishCount);
    void Stop();

    /// Append one randomly generated fish and build its sheet on a worker
    /// thread, leaving every other fish untouched. Draw/Update ignore it
    /// until it is ready, so it simply appears. False when the tank is full
    /// or not running.
    bool AddFish();

    /// Startle the fish drawn under a window pixel. It turns its back on the
    /// pointer and darts off, so clicking a fish scares it rather than being
    /// ignored. Returns the fish's index (the save's `fish_N`), or -1 when that
    /// pixel holds no fish - the click was on open water or empty sprite.
    int PokeAt(double x, double y);

    /// How fast a startled fish flees, as a multiple of its cruise speed
    /// (water.ini's `escape_speed`). Clamped to 1..6.
    void SetEscapeSpeed(double multiplier);

    void Resize(int width, int height);
    void Update(double dt);
    void Draw(uint8_t* premultipliedBgra, int width, int height);

    /// Every fish as data a save file can hold: species, genome seed and the
    /// agent state. A snapshot taken now restores to the same aquarium.
    std::vector<TankCreature> Snapshot() const;

    /// Rebuild the tank from a snapshot: sheets are regenerated from the saved
    /// species/seed on a worker, positions are set immediately. False when the
    /// list is empty or larger than the tank.
    bool Restore(int width, int height, const std::vector<TankCreature>& creatures);

    /// Draw into a straight-alpha logical canvas (half the window size) for
    /// the water composite. `hit` receives one byte per logical pixel and is
    /// set only on fish, so the window's hit test can ignore the water.
    void DrawLogical(uint8_t* straightRgba, int logicalW, int logicalH, uint8_t* hit) const;

    /// A turn emits a ripple. Window pixels; the caller converts to logical
    /// space. Rate-limited inside the tank so a crowded tank cannot flood
    /// the 16 ripple slots.
    std::function<void(double x, double y, double amplitude)> onRipple;

    int  FishCount() const { return int(fish_.size()); }
    int  ReadyCount();
    bool WaitAllReady(int timeoutMs);

private:
    struct CreatureJob
    {
        std::string species;
        sea::u64 seed = 0;
    };

    void InitFish(TankFish& f, std::mt19937& rng);
    void WorkerRange(int first, std::vector<CreatureJob> jobs);
    void AdoptPending();
    void EmitRipple(const TankFish& f);

    std::vector<TankFish> fish_;
    std::vector<std::unique_ptr<sea::SeaSheet>> pending_;
    mutable std::mutex mutex_;
    std::vector<std::thread> workers_;
    std::atomic<bool> stop_{ false };

    int       w_ = 0;
    int       h_ = 0;
    sea::u64  seed_ = 0;

    /// Per-click entropy for AddFish(), seeded from the OS once at startup so
    /// no two clicks within a process ever share a fish seed.
    std::mt19937_64 salty_{ std::random_device{}() };

    /// The preset pool shuffled once per launch, so a new tank opens with a
    /// random pick instead of the same list order every time.
    std::vector<const char*> startOrder_;

    /// Seconds until the next turn may emit a ripple.
    double rippleCooldown_ = 0.0;

    /// water.ini's escape_speed: a startled fish's speed, relative to cruise.
    double escapeSpeed_ = 2.8;
};
