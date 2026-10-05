#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

namespace wxlens
{
namespace data
{

/**
 * Priority, bounded-concurrency scheduling for radar provider work (docs/ROADMAP.md performance
 * checklist, 2026-09-26 investigation).
 *
 * Three problems this exists to solve, all measured against `wxlens.log` for 2026-09-26:
 *
 *  - **Priority.** Every provider call went through `scwx::util::async`, which is a plain FIFO
 *    post to the shared io_context. A pane's site switch therefore queued behind whatever
 *    speculative history warming happened to be running, measured at 6.3 s median and 59.5 s
 *    worst per load. Foreground work now jumps the queue.
 *
 *  - **Concurrency.** `main()` starts the io_context on exactly one thread, so the whole
 *    application had one radar worker. Raising that alone is not safe (see below), which is why
 *    the bound lives here rather than being implied by the number of runners.
 *
 *  - **Serialization where the provider needs it.** `AwsNexradDataProvider`'s download path
 *    (`LoadObjectByKey`) is safe to call concurrently - no shared mutable state, and the AWS
 *    S3 client is documented thread-safe. Its *listing* path is not: `Impl::UpdateMetadata`
 *    takes a `std::shared_lock` on `objectsMutex_` and then writes `lastModified_` and
 *    `updatePeriod_` through it, which is a data race between two concurrent `ListObjects`
 *    calls on one provider. That defect is in `external/legacy-supercell-wx`, which this repo
 *    treats as read-only (AGENTS.md), so it cannot be fixed here. Giving every listing task for
 *    one provider the same `serialKey` means two of them never overlap, which makes the race
 *    unreachable without touching the submodule. The upstream fix remains worth making; this
 *    does not depend on it.
 *
 * Tasks with the same non-empty `serialKey` never run concurrently with each other. Tasks with
 * different keys, or with an empty key, are limited only by the global concurrency bound.
 *
 * Deliberately free of Qt and wxdata types: the queue schedules `std::function<void()>`, so its
 * ordering, bounding and serialization behaviour is testable without a network, a provider or a
 * running application.
 */
class RadarTaskQueue
{
public:
   enum class Priority
   {
      /// A user is waiting: the selected pane's volume, an explicit timeline seek, a site
      /// switch, a product catalog a dialog is about to show.
      Foreground,
      /// Speculative: history warming, adjacent-frame prefetch, periodic refresh. Always yields
      /// to Foreground, however long it has been queued - starvation is the correct behaviour
      /// here, because the work is by definition not being waited on.
      Background
   };

   RadarTaskQueue();

   /// Stops accepting work and blocks until every already-running task has finished, so no task
   /// can outlive the queue and touch state its owner has reclaimed.
   ~RadarTaskQueue();

   RadarTaskQueue(const RadarTaskQueue&)            = delete;
   RadarTaskQueue& operator=(const RadarTaskQueue&) = delete;
   RadarTaskQueue(RadarTaskQueue&&)                 = delete;
   RadarTaskQueue& operator=(RadarTaskQueue&&)      = delete;

   /// Process-wide queue used by RadarSiteDataService. Tests construct their own instance
   /// instead, so one test's bound or backlog cannot leak into another's.
   static RadarTaskQueue& Instance();

   /**
    * Schedules `task`. An empty `serialKey` means "no serialization constraint".
    *
    * `task` runs on an io_context worker. An exception escaping it is caught and logged; the
    * queue keeps running, matching main()'s own run-loop behaviour.
    */
   void Post(Priority priority, std::string serialKey, std::function<void()> task);

   /**
    * Drops queued-but-not-yet-started tasks whose `serialKey` starts with `serialKeyPrefix`.
    *
    * This is how a deactivated radar site stops costing anything: its already-running task will
    * observe its own cancellation flag and unwind, but anything still waiting should never start
    * at all. Returns how many were dropped, which the lifecycle tests assert on.
    *
    * Deliberately prefix-based rather than exact-match: one site posts under several keys
    * (`KEAX/list`, `KEAX/load`, ...), and dropping them individually would race with a task that
    * posts its own continuation.
    */
   std::size_t DropQueued(const std::string& serialKeyPrefix);

   /// Maximum tasks executing at once. Clamped to at least 1.
   void        SetMaxConcurrency(std::size_t maximum);
   [[nodiscard]] std::size_t max_concurrency() const;

   /// Diagnostics, and what the tests assert against.
   [[nodiscard]] std::size_t queued_count() const;
   [[nodiscard]] std::size_t running_count() const;

private:
   class Impl;
   /// shared, not unique: a dispatched task holds its own reference for the duration.
   std::shared_ptr<Impl> p;
};

} // namespace data
} // namespace wxlens
