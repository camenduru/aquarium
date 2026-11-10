#pragma once
// ================================================================
// AQUARIUM - the tank: fish agents playing the generator's clips
// ----------------------------------------------------------------
// Every fish is one sprite sheet from the sea generator (a row per clip,
// a column per frame). An agent only decides where it is and which clip
// it is in; the sheet already contains the right-facing swim, the
// left-facing swim and both turns, so nothing is mirrored at runtime.
//
// Sheets are built on a worker thread: a 64 px sheet is seconds of
// supersampled rendering, and the window must appear immediately, so
// fish arrive one at a time instead of blocking startup.
// ================================================================

#include "creature/sea/SeaSprite.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct TankFish
{
    sea::SeaSheet sheet;
    bool     ready  = false;
    std::string species;

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

    int    scale = 2;        // nearest-neighbour upscale
    int    alpha = 235;      // depth
};

class Tank
{
public:
    ~Tank() { Stop(); }

    /// Rebuild the tank: spawn `fishCount` agents and start generating sheets.
    void Start(int width, int height, sea::u64 seed, int fishCount);
    void Stop();

    void Resize(int width, int height);
    void Update(double dt);
    void Draw(uint8_t* premultipliedBgra, int width, int height);

    int  FishCount() const { return int(fish_.size()); }
    int  ReadyCount();
    bool WaitAllReady(int timeoutMs);

private:
    void Worker();
    void AdoptPending();

    std::vector<TankFish> fish_;
    std::vector<std::unique_ptr<sea::SeaSheet>> pending_;
    mutable std::mutex mutex_;
    std::thread worker_;
    std::atomic<bool> stop_{ false };

    int       w_ = 0;
    int       h_ = 0;
    sea::u64  seed_ = 0;
};
