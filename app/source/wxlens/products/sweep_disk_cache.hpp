#pragma once

#include <wxlens/products/radar_sweep_product.hpp>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace wxlens
{
namespace products
{

/**
 * A disk-cache hit, read back. Carries the tilt the sweep was actually built from alongside the
 * geometry, not just SweepData: RadarSweepProduct pairs a sweep with an elevation angle under one
 * lock precisely because an altitude computed from one sweep's range and a different sweep's
 * angle describes nothing (see its dataMutex_ comment) - a disk hit that resolved to sweep
 * geometry but guessed at the angle would violate that same invariant.
 */
struct CachedSweep
{
   std::shared_ptr<SweepData> sweep;
   float                      elevationAngleDegrees {0.0f};

   /// The site's full list of available tilts (the tilt picker's dropdown), captured alongside
   /// the sweep so a disk hit does not silently leave that picker looking like only one tilt
   /// exists - this is the one other piece of state OnLevelTwoDataLoaded derives from the volume
   /// besides the sweep and angle above.
   std::vector<float> elevationCuts;
};

/**
 * Disk-backed companion to GeometryCache (radar_sweep_product.cpp): persists computed SweepData
 * across restarts, keyed the same way (see BuildDiskCacheKey in the .cpp), so revisiting a
 * previously-viewed archive frame after a restart can skip the network fetch and decode entirely,
 * not just the geometry rebuild GeometryCache already saves within one running process. See
 * docs/ROADMAP.md's "Add bounded on-device disk persistence" checklist item.
 *
 * Deliberately caches computed SweepData rather than the decoded Ar2vFile or raw download bytes:
 * ADR 0002 forbids patching wxdata to expose either, and SweepData is WxLens's own plain struct -
 * fully serializable without touching wxdata at all. The tradeoff is scope, not correctness: a
 * (site, product, elevation, time) combination never viewed before still requires the normal
 * network fetch the first time, in this run or any other; only revisits are ever free.
 *
 * One flat directory of self-describing files. A corrupt or half-written file must never be
 * mistaken for a different, valid entry, so the original key is stored inside the file and
 * checked on read rather than trusted from the filename alone - the filename is only a hash used
 * to keep the directory listable in O(1) per lookup.
 *
 * Every public member serializes through one mutex: disk cache calls are already off the render
 * and GUI threads (see OnLevelTwoDataLoaded's worker), so correctness is preferred over the
 * concurrency FrameCache needs for in-memory lookups on a hot path.
 */
class SweepDiskCache
{
public:
   /// `root` is created if missing. `capacityBytes` bounds total size on disk; eviction is by
   /// least-recently-read file, matching FrameCache's in-memory LRU policy.
   SweepDiskCache(std::filesystem::path root, std::size_t capacityBytes);

   SweepDiskCache(const SweepDiskCache&)            = delete;
   SweepDiskCache& operator=(const SweepDiskCache&) = delete;
   SweepDiskCache(SweepDiskCache&&)                 = delete;
   SweepDiskCache& operator=(SweepDiskCache&&)      = delete;

   /**
    * Returns the cached sweep for `key`, or a null CachedSweep::sweep on a miss - including a
    * miss caused by a corrupt, truncated, foreign or hash-colliding file, all of which this
    * deletes on the way out rather than ever handing bad geometry to the renderer. Promotes the
    * entry to most-recently-read on a hit.
    */
   [[nodiscard]] CachedSweep Find(const std::string& key) const;

   /**
    * Persists `sweep`/`elevationAngleDegrees` under `key`/`observationTime`. Written to a
    * temporary file and renamed into place, so a crash or kill mid-write leaves either the
    * previous entry or nothing behind - never a truncated one. Enforces the capacity budget
    * afterward, evicting least-recently-read entries (this one included, if it is itself the
    * single oversized outlier).
    */
   void Store(const std::string&                     key,
             std::chrono::system_clock::time_point   observationTime,
             const SweepData&                        sweep,
             float                                    elevationAngleDegrees,
             const std::vector<float>&                elevationCuts);

   /// Deletes every entry, including stray temporary files left by an interrupted Store. Safe to
   /// call while the process is running - no file here is ever held open outside one Find/Store.
   void Clear();

   [[nodiscard]] std::size_t size_bytes() const;
   [[nodiscard]] std::size_t count() const;
   [[nodiscard]] std::size_t capacity_bytes() const;

private:
   [[nodiscard]] std::filesystem::path PathFor(const std::string& key) const;
   void EnforceCapacityLocked() const;

   std::filesystem::path root_;
   std::size_t           capacityBytes_;
   mutable std::mutex    mutex_;
};

} // namespace products
} // namespace wxlens
