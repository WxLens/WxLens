#pragma once

#include <cstddef>
#include <functional>
#include <future>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace wxlens
{
namespace data
{

/**
 * Bounded, deduplicating frame cache (docs/ROADMAP.md, 2026-09-09 user-feedback
 * checklist: "Add bounded shared frame caching and request deduplication").
 *
 * Two separate problems, one component:
 *
 *  - Retention. Radar frames were reloaded and re-decoded even when the
 *    selected object key had not changed. The measured cost of that repeat was
 *    ~3.6 s of download+decode for an identical Level 2 key
 *    (docs/performance-baseline.md). Entries are kept until a byte budget
 *    forces least-recently-used eviction, so retention cannot grow without
 *    bound on the roadmap's 8 GB low-end floor.
 *
 *  - Deduplication. Panes share one service per site (§4.6), so several panes
 *    selecting the same frame previously issued the same download concurrently.
 *    The first caller for a key runs the loader; every concurrent caller for
 *    that same key waits on that one result instead of starting its own.
 *
 * Deliberately generic and free of Qt, wxdata and provider dependencies: the
 * caller supplies the loader and the size estimator, which keeps the eviction
 * and deduplication logic testable without a network or a running application.
 *
 * All public members are safe to call concurrently. The loader runs outside the
 * internal lock, so a slow download never blocks cache lookups for other keys.
 */
template<typename T>
class FrameCache
{
public:
   /// Where a returned value came from - recorded so load logging can report
   /// cache behaviour honestly instead of asserting a fixed value.
   enum class Origin
   {
      /// Served from a retained entry; the loader did not run.
      Cache,
      /// This caller ran the loader.
      Loaded,
      /// Another caller was already loading this key; this caller waited.
      Deduplicated
   };

   struct LoadResult
   {
      std::shared_ptr<T> value;
      Origin             origin;

      [[nodiscard]] bool cache_hit() const { return origin == Origin::Cache; }
   };

   using Loader       = std::function<std::shared_ptr<T>()>;
   using SizeEstimate = std::function<std::size_t(const T&)>;

   explicit FrameCache(std::size_t capacityBytes) :
       capacityBytes_ {capacityBytes}
   {
   }

   FrameCache(const FrameCache&)            = delete;
   FrameCache& operator=(const FrameCache&) = delete;
   FrameCache(FrameCache&&)                 = delete;
   FrameCache& operator=(FrameCache&&)      = delete;

   /**
    * Returns the retained value for `key`, or nullptr. A hit promotes the entry
    * to most-recently-used.
    */
   std::shared_ptr<T> Find(const std::string& key)
   {
      std::lock_guard lock {mutex_};
      return FindLocked(key);
   }

   /**
    * Returns the retained value for `key`, otherwise loads it exactly once
    * across all concurrent callers.
    *
    * `sizeOf` is only consulted for a value this call actually loaded. A value
    * larger than the whole budget is returned to the caller but not retained,
    * so one oversized frame cannot evict an otherwise useful cache. A loader
    * returning nullptr is not retained either - a failed download must not
    * become a cached negative result - but concurrent callers still share that
    * one failed attempt rather than each retrying immediately.
    *
    * An exception from the loader propagates to every caller waiting on that
    * key and leaves nothing retained.
    */
   LoadResult Load(const std::string&  key,
                   const Loader&       loader,
                   const SizeEstimate& sizeOf)
   {
      std::shared_ptr<InFlight> inFlight;
      bool                      owner = false;

      {
         std::lock_guard lock {mutex_};

         if (auto cached = FindLocked(key); cached != nullptr)
         {
            return {std::move(cached), Origin::Cache};
         }

         auto [it, inserted] = inFlight_.try_emplace(key);
         if (inserted)
         {
            it->second = std::make_shared<InFlight>();
            owner      = true;
         }
         inFlight = it->second;
      }

      if (!owner)
      {
         // Blocks until the owning caller publishes a value or an exception.
         // Rethrows the owner's exception here, so a shared failure surfaces
         // identically to a failure this caller produced itself.
         return {inFlight->future.get(), Origin::Deduplicated};
      }

      std::shared_ptr<T> value;
      try
      {
         value = loader();
      }
      catch (...)
      {
         const auto failure = std::current_exception();
         {
            std::lock_guard lock {mutex_};
            inFlight_.erase(key);
         }
         inFlight->promise.set_exception(failure);
         throw;
      }

      const std::size_t bytes = (value != nullptr) ? sizeOf(*value) : 0U;

      {
         std::lock_guard lock {mutex_};
         inFlight_.erase(key);
         if (value != nullptr && bytes <= capacityBytes_)
         {
            InsertLocked(key, value, bytes);
         }
      }

      inFlight->promise.set_value(value);
      return {std::move(value), Origin::Loaded};
   }

   /// Drops every retained entry. In-flight loads are unaffected; their results
   /// simply land in an empty cache.
   void Clear()
   {
      std::lock_guard lock {mutex_};
      entries_.clear();
      order_.clear();
      usedBytes_ = 0U;
   }

   [[nodiscard]] std::size_t size_bytes() const
   {
      std::lock_guard lock {mutex_};
      return usedBytes_;
   }

   [[nodiscard]] std::size_t count() const
   {
      std::lock_guard lock {mutex_};
      return entries_.size();
   }

   [[nodiscard]] std::size_t capacity_bytes() const { return capacityBytes_; }

   /// Diagnostic only - does not promote the entry, so tests can assert
   /// eviction order without altering it.
   [[nodiscard]] bool Contains(const std::string& key) const
   {
      std::lock_guard lock {mutex_};
      return entries_.find(key) != entries_.end();
   }

private:
   struct InFlight
   {
      std::promise<std::shared_ptr<T>>       promise;
      std::shared_future<std::shared_ptr<T>> future {promise.get_future()};
   };

   struct Entry
   {
      std::shared_ptr<T>                        value;
      std::size_t                               bytes;
      typename std::list<std::string>::iterator position;
   };

   std::shared_ptr<T> FindLocked(const std::string& key)
   {
      auto it = entries_.find(key);
      if (it == entries_.end())
      {
         return nullptr;
      }
      order_.splice(order_.begin(), order_, it->second.position);
      it->second.position = order_.begin();
      return it->second.value;
   }

   void InsertLocked(const std::string&        key,
                     const std::shared_ptr<T>& value,
                     std::size_t               bytes)
   {
      // A concurrent loader for a different key may have inserted this one in
      // the window where the lock was released, so replace rather than assume.
      if (auto existing = entries_.find(key); existing != entries_.end())
      {
         usedBytes_ -= existing->second.bytes;
         order_.erase(existing->second.position);
         entries_.erase(existing);
      }

      order_.push_front(key);
      entries_.emplace(key, Entry {value, bytes, order_.begin()});
      usedBytes_ += bytes;

      while (usedBytes_ > capacityBytes_ && !order_.empty())
      {
         const std::string oldest = order_.back();
         if (auto it = entries_.find(oldest); it != entries_.end())
         {
            usedBytes_ -= it->second.bytes;
            entries_.erase(it);
         }
         order_.pop_back();
      }
   }

   const std::size_t                      capacityBytes_;
   mutable std::mutex                     mutex_;
   std::unordered_map<std::string, Entry> entries_;
   std::list<std::string>                 order_;
   std::size_t                            usedBytes_ {0U};
   std::unordered_map<std::string, std::shared_ptr<InFlight>> inFlight_;
};

} // namespace data
} // namespace wxlens
