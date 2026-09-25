#include <wxlens/products/sweep_disk_cache.hpp>
#include <wxlens/log/logger.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <system_error>
#include <vector>

namespace wxlens
{
namespace products
{

namespace
{
static const std::string logPrefix_ = "products.sweep_disk_cache";
static const auto        logger_    = wxlens::log::Create(logPrefix_);

// Identifies the format, not just the file type: bump on any layout change below so an old build's
// cache is a clean miss rather than a misparsed one.
constexpr char kMagic[4] = {'W', 'X', 'R', '1'};

constexpr char kTempSuffix[] = ".tmp";
constexpr char kEntrySuffix[] = ".wxrd";

std::uint64_t Fnv1a64(const std::string& data)
{
   constexpr std::uint64_t kOffsetBasis = 14695981039346656037ULL;
   constexpr std::uint64_t kPrime       = 1099511628211ULL;
   std::uint64_t           hash         = kOffsetBasis;
   for (const unsigned char byte : data)
   {
      hash ^= byte;
      hash *= kPrime;
   }
   return hash;
}

// Same algorithm, over an arbitrary byte payload - used for the corruption-detection checksum
// rather than key hashing, but there is no reason to carry two implementations of FNV-1a.
std::uint32_t Fnv1a32(const std::uint8_t* data, std::size_t length)
{
   constexpr std::uint32_t kOffsetBasis = 2166136261U;
   constexpr std::uint32_t kPrime       = 16777619U;
   std::uint32_t           hash         = kOffsetBasis;
   for (std::size_t i = 0; i < length; ++i)
   {
      hash ^= data[i];
      hash *= kPrime;
   }
   return hash;
}

/// Minimal little-endian byte-buffer writer. Deliberately not a fixed struct layout (alignment
/// and padding would have to be reasoned about byte-for-byte for no benefit - this file is never
/// read by anything but this class).
class ByteWriter
{
public:
   void U32(std::uint32_t value)
   {
      for (int i = 0; i < 4; ++i) buffer_.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
   }
   void I64(std::int64_t value)
   {
      auto unsigned_ = static_cast<std::uint64_t>(value);
      for (int i = 0; i < 8; ++i) buffer_.push_back(static_cast<std::uint8_t>(unsigned_ >> (8 * i)));
   }
   void F32(float value)
   {
      std::uint32_t bits;
      std::memcpy(&bits, &value, sizeof(bits));
      U32(bits);
   }
   void Bytes(const void* data, std::size_t length)
   {
      const auto* bytes = static_cast<const std::uint8_t*>(data);
      buffer_.insert(buffer_.end(), bytes, bytes + length);
   }
   void String(const std::string& value)
   {
      U32(static_cast<std::uint32_t>(value.size()));
      Bytes(value.data(), value.size());
   }
   [[nodiscard]] const std::vector<std::uint8_t>& buffer() const { return buffer_; }

private:
   std::vector<std::uint8_t> buffer_;
};

/// Bounds-checked little-endian byte-buffer reader. Every read fails closed (throws) on
/// underflow, which Find() below turns into "delete the file, report a miss" rather than a crash
/// or garbage geometry - the one property that actually matters for a cache that a corrupted disk
/// or an interrupted write can hand back anything to.
class ByteReader
{
public:
   ByteReader(const std::uint8_t* data, std::size_t length) : data_ {data}, length_ {length} {}

   std::uint32_t U32()
   {
      Require(4);
      std::uint32_t value = 0;
      for (int i = 0; i < 4; ++i)
         value |= static_cast<std::uint32_t>(data_[offset_ + i]) << (8 * i);
      offset_ += 4;
      return value;
   }
   std::int64_t I64()
   {
      Require(8);
      std::uint64_t value = 0;
      for (int i = 0; i < 8; ++i)
         value |= static_cast<std::uint64_t>(data_[offset_ + i]) << (8 * i);
      offset_ += 8;
      return static_cast<std::int64_t>(value);
   }
   float F32()
   {
      const std::uint32_t bits = U32();
      float                value;
      std::memcpy(&value, &bits, sizeof(value));
      return value;
   }
   void Bytes(void* out, std::size_t length)
   {
      Require(length);
      std::memcpy(out, data_ + offset_, length);
      offset_ += length;
   }
   std::string String()
   {
      const auto  length = U32();
      std::string value(length, '\0');
      Bytes(value.data(), length);
      return value;
   }
   [[nodiscard]] std::size_t remaining() const { return length_ - offset_; }
   [[nodiscard]] const std::uint8_t* cursor() const { return data_ + offset_; }

private:
   void Require(std::size_t count) const
   {
      if (count > remaining())
      {
         throw std::out_of_range {"sweep disk cache entry truncated"};
      }
   }

   const std::uint8_t* data_;
   std::size_t          length_;
   std::size_t          offset_ {0};
};

std::vector<std::uint8_t> SerializePayload(const SweepData&           sweep,
                                          float                      elevationAngleDegrees,
                                          const std::vector<float>& elevationCuts)
{
   ByteWriter writer;
   writer.U32(static_cast<std::uint32_t>(sweep.vertices.size()));
   writer.Bytes(sweep.vertices.data(), sweep.vertices.size() * sizeof(float));
   writer.U32(static_cast<std::uint32_t>(sweep.dataMoments8.size()));
   writer.Bytes(sweep.dataMoments8.data(), sweep.dataMoments8.size());
   writer.U32(static_cast<std::uint32_t>(sweep.dataMoments16.size()));
   writer.Bytes(sweep.dataMoments16.data(), sweep.dataMoments16.size() * sizeof(std::uint16_t));
   writer.F32(sweep.dataMomentOffset);
   writer.F32(sweep.dataMomentScale);
   writer.String(sweep.dataMomentUnits);
   writer.F32(elevationAngleDegrees);
   writer.U32(static_cast<std::uint32_t>(elevationCuts.size()));
   writer.Bytes(elevationCuts.data(), elevationCuts.size() * sizeof(float));
   return writer.buffer();
}

/// Throws on any structural problem (truncation, an absurd count that would overrun the buffer);
/// Find() below is the only caller and treats every exception here as a plain cache miss.
CachedSweep DeserializePayload(const std::uint8_t* data, std::size_t length)
{
   ByteReader reader {data, length};
   auto       sweep = std::make_shared<SweepData>();

   const auto verticesCount = reader.U32();
   sweep->vertices.resize(verticesCount);
   if (verticesCount > 0)
   {
      reader.Bytes(sweep->vertices.data(), verticesCount * sizeof(float));
   }

   const auto moments8Count = reader.U32();
   sweep->dataMoments8.resize(moments8Count);
   if (moments8Count > 0)
   {
      reader.Bytes(sweep->dataMoments8.data(), moments8Count);
   }

   const auto moments16Count = reader.U32();
   sweep->dataMoments16.resize(moments16Count);
   if (moments16Count > 0)
   {
      reader.Bytes(sweep->dataMoments16.data(), moments16Count * sizeof(std::uint16_t));
   }

   sweep->dataMomentOffset = reader.F32();
   sweep->dataMomentScale  = reader.F32();
   sweep->dataMomentUnits  = reader.String();
   const auto elevationAngleDegrees = reader.F32();

   const auto elevationCutsCount = reader.U32();
   std::vector<float> elevationCuts(elevationCutsCount);
   if (elevationCutsCount > 0)
   {
      reader.Bytes(elevationCuts.data(), elevationCutsCount * sizeof(float));
   }

   return CachedSweep {sweep, elevationAngleDegrees, std::move(elevationCuts)};
}

} // namespace

SweepDiskCache::SweepDiskCache(std::filesystem::path root, std::size_t capacityBytes) :
    root_ {std::move(root)}, capacityBytes_ {capacityBytes}
{
   std::error_code error;
   std::filesystem::create_directories(root_, error);
   if (error)
   {
      logger_->error("Cannot create sweep disk cache directory {}: {}",
                     root_.string(),
                     error.message());
   }
}

std::filesystem::path SweepDiskCache::PathFor(const std::string& key) const
{
   std::ostringstream name;
   name << std::hex << Fnv1a64(key);
   return root_ / (name.str() + kEntrySuffix);
}

CachedSweep SweepDiskCache::Find(const std::string& key) const
{
   std::lock_guard lock {mutex_};

   const auto path = PathFor(key);
   std::error_code exists_error;
   if (!std::filesystem::exists(path, exists_error))
   {
      return {};
   }

   std::ifstream file(path, std::ios::binary);
   if (!file.is_open())
   {
      return {};
   }
   const std::vector<std::uint8_t> raw {std::istreambuf_iterator<char> {file},
                                        std::istreambuf_iterator<char> {}};
   file.close();

   const auto reject = [&](const char* reason) -> CachedSweep
   {
      logger_->warn("Sweep disk cache entry for {} rejected ({}); removing {}",
                    key,
                    reason,
                    path.string());
      std::error_code remove_error;
      std::filesystem::remove(path, remove_error);
      return {};
   };

   try
   {
      ByteReader header {raw.data(), raw.size()};
      std::array<std::uint8_t, 4> magic {};
      header.Bytes(magic.data(), magic.size());
      if (std::memcmp(magic.data(), kMagic, sizeof(kMagic)) != 0)
      {
         return reject("bad magic/version");
      }
      const auto storedKey          = header.String();
      const auto observationTimeMs  = header.I64();
      (void) observationTimeMs; // Not needed to serve a hit; kept for future diagnostics/tools.
      const auto checksum           = header.U32();
      const auto payloadLength      = header.U32();

      if (storedKey != key)
      {
         // A hash collision between two different keys, or (far more likely) a leftover file from
         // an older cache layout that reused this hash space differently. Either way, this is not
         // the entry the caller asked for.
         return reject("stored key does not match");
      }
      if (payloadLength != header.remaining())
      {
         return reject("payload length mismatch");
      }
      if (Fnv1a32(header.cursor(), payloadLength) != checksum)
      {
         return reject("checksum mismatch");
      }

      auto result = DeserializePayload(header.cursor(), payloadLength);

      std::error_code touch_error;
      std::filesystem::last_write_time(
         path, std::filesystem::file_time_type::clock::now(), touch_error);

      return result;
   }
   catch (const std::exception&)
   {
      return reject("truncated or malformed entry");
   }
}

void SweepDiskCache::Store(const std::string&                   key,
                           std::chrono::system_clock::time_point observationTime,
                           const SweepData&                      sweep,
                           float                                  elevationAngleDegrees,
                           const std::vector<float>&              elevationCuts)
{
   std::lock_guard lock {mutex_};

   const auto payload = SerializePayload(sweep, elevationAngleDegrees, elevationCuts);

   ByteWriter header;
   header.Bytes(kMagic, sizeof(kMagic));
   header.String(key);
   header.I64(std::chrono::duration_cast<std::chrono::milliseconds>(
                 observationTime.time_since_epoch())
                 .count());
   header.U32(Fnv1a32(payload.data(), payload.size()));
   header.U32(static_cast<std::uint32_t>(payload.size()));

   const auto finalPath = PathFor(key);
   auto       tempPathWithSuffix = finalPath;
   tempPathWithSuffix += kTempSuffix;

   {
      std::ofstream file(tempPathWithSuffix, std::ios::binary | std::ios::trunc);
      if (!file.is_open())
      {
         logger_->warn("Cannot open {} for sweep disk cache write", tempPathWithSuffix.string());
         return;
      }
      file.write(reinterpret_cast<const char*>(header.buffer().data()),
                static_cast<std::streamsize>(header.buffer().size()));
      file.write(reinterpret_cast<const char*>(payload.data()),
                static_cast<std::streamsize>(payload.size()));
      if (!file.good())
      {
         logger_->warn("Write failed for {}; leaving previous entry (if any) untouched",
                       tempPathWithSuffix.string());
         file.close();
         std::error_code remove_error;
         std::filesystem::remove(tempPathWithSuffix, remove_error);
         return;
      }
   }

   std::error_code rename_error;
   std::filesystem::rename(tempPathWithSuffix, finalPath, rename_error);
   if (rename_error)
   {
      logger_->warn("Cannot commit sweep disk cache entry {}: {}",
                    finalPath.string(),
                    rename_error.message());
      std::error_code remove_error;
      std::filesystem::remove(tempPathWithSuffix, remove_error);
      return;
   }

   EnforceCapacityLocked();
}

void SweepDiskCache::EnforceCapacityLocked() const
{
   struct Entry
   {
      std::filesystem::path                path;
      std::uintmax_t                       bytes;
      std::filesystem::file_time_type      lastWrite;
   };

   std::vector<Entry> entries;
   std::error_code     iterate_error;
   std::uintmax_t      total = 0;

   for (const auto& dirEntry :
       std::filesystem::directory_iterator {root_, iterate_error})
   {
      if (iterate_error) break;
      if (!dirEntry.is_regular_file()) continue;

      // A .tmp file surviving here means a previous Store was interrupted mid-write (crash, kill,
      // disk full) before the rename that commits it - garbage, not a candidate entry.
      if (dirEntry.path().extension() == kTempSuffix)
      {
         std::error_code remove_error;
         std::filesystem::remove(dirEntry.path(), remove_error);
         continue;
      }
      if (dirEntry.path().extension() != kEntrySuffix) continue;

      std::error_code size_error;
      std::error_code time_error;
      const auto      bytes = dirEntry.file_size(size_error);
      const auto      lastWrite = dirEntry.last_write_time(time_error);
      if (size_error || time_error) continue;

      entries.push_back({dirEntry.path(), bytes, lastWrite});
      total += bytes;
   }

   if (total <= capacityBytes_) return;

   std::sort(entries.begin(),
            entries.end(),
            [](const Entry& a, const Entry& b) { return a.lastWrite < b.lastWrite; });

   for (const auto& entry : entries)
   {
      if (total <= capacityBytes_) break;
      std::error_code remove_error;
      if (std::filesystem::remove(entry.path, remove_error))
      {
         total -= entry.bytes;
      }
   }
}

void SweepDiskCache::Clear()
{
   std::lock_guard lock {mutex_};

   std::error_code iterate_error;
   for (const auto& dirEntry : std::filesystem::directory_iterator {root_, iterate_error})
   {
      if (iterate_error) break;
      if (!dirEntry.is_regular_file()) continue;
      const auto extension = dirEntry.path().extension();
      if (extension != kEntrySuffix && extension != kTempSuffix) continue;
      std::error_code remove_error;
      std::filesystem::remove(dirEntry.path(), remove_error);
   }
}

std::size_t SweepDiskCache::size_bytes() const
{
   std::lock_guard lock {mutex_};
   std::uintmax_t   total = 0;
   std::error_code   iterate_error;
   for (const auto& dirEntry : std::filesystem::directory_iterator {root_, iterate_error})
   {
      if (iterate_error) break;
      if (!dirEntry.is_regular_file() || dirEntry.path().extension() != kEntrySuffix) continue;
      std::error_code size_error;
      total += dirEntry.file_size(size_error);
   }
   return static_cast<std::size_t>(total);
}

std::size_t SweepDiskCache::count() const
{
   std::lock_guard lock {mutex_};
   std::size_t      total = 0;
   std::error_code   iterate_error;
   for (const auto& dirEntry : std::filesystem::directory_iterator {root_, iterate_error})
   {
      if (iterate_error) break;
      if (dirEntry.is_regular_file() && dirEntry.path().extension() == kEntrySuffix) ++total;
   }
   return total;
}

std::size_t SweepDiskCache::capacity_bytes() const
{
   std::lock_guard lock {mutex_};
   return capacityBytes_;
}

} // namespace products
} // namespace wxlens
