// Level 2 (Archive II) decode benchmark.
//
// Times scwx::wsr88d::Ar2vFile::LoadData on real volume files read into memory first, so the
// number is pure decompress + parse + index cost: no network, no disk. Also times a bzip2-only
// pass over the same LDM records (sequential, the way the unpatched loader does it), which shows
// how much of the load is decompression versus message parsing.
//
// Prints a fingerprint of everything the loader produced (every radial's identity and the raw
// bytes of every moment block), so a change to the loader can be checked for identical output,
// not just speed.
//
// Usage: wxlens-level2-decode-bench [--runs N] <Archive II file>...
//
// Not a test and not registered with ctest - it measures, it doesn't assert.

#include <scwx/wsr88d/ar2v_file.hpp>
#include <scwx/wsr88d/rda/generic_radar_data.hpp>
#include <scwx/wsr88d/rda/rda_types.hpp>

#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/device/array.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{

using Clock = std::chrono::steady_clock;

double Ms(Clock::duration d)
{
   return std::chrono::duration<double, std::milli>(d).count();
}

struct Fnv
{
   std::uint64_t h = 1469598103934665603ULL;
   void          Add(const void* data, std::size_t n)
   {
      const auto* b = static_cast<const unsigned char*>(data);
      for (std::size_t i = 0; i < n; ++i) { h = (h ^ b[i]) * 1099511628211ULL; }
   }
   template<typename T> void Add(const T& v) { Add(&v, sizeof(v)); }
};

std::uint64_t Fingerprint(const scwx::wsr88d::Ar2vFile& file, std::size_t& radials)
{
   using namespace scwx::wsr88d::rda;
   Fnv f;
   f.Add(file.message_count());
   radials = 0;
   for (const auto& [elevationIndex, scan] : file.radar_data())
   {
      f.Add(elevationIndex);
      if (!scan) continue;
      for (const auto& [azimuthIndex, radial] : *scan)
      {
         f.Add(azimuthIndex);
         if (!radial) continue;
         ++radials;
         f.Add(radial->collection_time());
         f.Add(radial->modified_julian_date());
         f.Add(radial->azimuth_angle().value());
         for (DataBlockType type : MomentDataBlockTypeIterator())
         {
            const auto block = radial->moment_data_block(type);
            if (!block) continue;
            f.Add(type);
            const std::size_t gates = block->number_of_data_moment_gates();
            const std::size_t width = block->data_word_size() > 8 ? 2 : 1;
            f.Add(gates);
            f.Add(block->data_word_size());
            if (block->data_moments()) f.Add(block->data_moments(), gates * width);
         }
      }
   }
   // The index GetElevationScan answers from, for the products the app shows most.
   for (DataBlockType type : {DataBlockType::MomentRef, DataBlockType::MomentVel})
   {
      const auto [scan, cut, cuts] = file.GetElevationScan(type, 0.5f, {});
      f.Add(cut);
      for (float c : cuts) f.Add(c);
   }
   return f.h;
}

/// Sequential bzip2 over every LDM record, mirroring the unpatched DecompressLDMRecords.
double TimeDecompressOnly(const std::string& bytes, std::size_t& records)
{
   constexpr std::size_t kVolumeHeaderSize = 24;
   const auto            start             = Clock::now();
   records                                 = 0;
   std::size_t pos                         = kVolumeHeaderSize;
   while (pos + 4 <= bytes.size())
   {
      std::int32_t control = 0;
      std::memcpy(&control, bytes.data() + pos, 4);
      control                = static_cast<std::int32_t>(
         (static_cast<std::uint32_t>(control) >> 24) |
         ((static_cast<std::uint32_t>(control) >> 8) & 0xFF00u) |
         ((static_cast<std::uint32_t>(control) << 8) & 0xFF0000u) |
         (static_cast<std::uint32_t>(control) << 24));
      const std::size_t size = static_cast<std::size_t>(std::abs(control));
      if (size == 0) break;
      pos += 4;
      const std::size_t take = std::min(size, bytes.size() - pos);
      boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
      in.push(boost::iostreams::bzip2_decompressor());
      in.push(boost::iostreams::array_source(bytes.data() + pos, take));
      std::stringstream ss;
      try { boost::iostreams::copy(in, ss); } catch (const std::exception&) {}
      pos += take;
      ++records;
   }
   return Ms(Clock::now() - start);
}

double Median(std::vector<double> v)
{
   std::sort(v.begin(), v.end());
   return v.size() % 2 ? v[v.size() / 2] : (v[v.size() / 2 - 1] + v[v.size() / 2]) / 2;
}

} // namespace

int main(int argc, char** argv)
{
   int                      runs = 7;
   std::vector<std::string> files;
   for (int i = 1; i < argc; ++i)
   {
      if (std::strcmp(argv[i], "--runs") == 0 && i + 1 < argc) runs = std::atoi(argv[++i]);
      else files.emplace_back(argv[i]);
   }
   if (files.empty() || runs < 1)
   {
      std::fprintf(stderr, "usage: %s [--runs N] <Archive II file>...\n", argv[0]);
      return 2;
   }

   std::printf("hardware threads: %u, runs per file: %d\n", std::thread::hardware_concurrency(),
               runs);
   std::printf("%-28s %9s %7s %8s %11s %11s %11s  %s\n", "file", "bytes", "records", "radials",
               "load med ms", "load min ms", "bz2-only ms", "fingerprint");

   int failures = 0;
   for (const auto& path : files)
   {
      std::ifstream in(path, std::ios::binary);
      if (!in)
      {
         std::fprintf(stderr, "cannot open %s\n", path.c_str());
         ++failures;
         continue;
      }
      const std::string bytes {std::istreambuf_iterator<char>(in), {}};

      std::vector<double> loads;
      std::uint64_t       print   = 0;
      std::size_t         radials = 0;
      bool                ok      = true;
      for (int r = 0; r < runs; ++r)
      {
         std::istringstream      is(bytes);
         scwx::wsr88d::Ar2vFile  file;
         const auto              start = Clock::now();
         ok                            = file.LoadData(is) && ok;
         loads.push_back(Ms(Clock::now() - start));
         std::size_t         n = 0;
         const std::uint64_t h = Fingerprint(file, n);
         if (r > 0 && (h != print || n != radials))
         {
            std::fprintf(stderr, "%s: fingerprint changed between runs\n", path.c_str());
            ok = false;
         }
         print   = h;
         radials = n;
      }

      std::vector<double> bz;
      std::size_t         records = 0;
      for (int r = 0; r < std::min(runs, 3); ++r) bz.push_back(TimeDecompressOnly(bytes, records));

      const auto slash = path.find_last_of("/\\");
      std::printf("%-28s %9zu %7zu %8zu %11.1f %11.1f %11.1f  %016llx%s\n",
                  path.substr(slash == std::string::npos ? 0 : slash + 1).c_str(), bytes.size(),
                  records, radials, Median(loads), *std::min_element(loads.begin(), loads.end()),
                  Median(bz), static_cast<unsigned long long>(print), ok ? "" : "  FAILED");
      if (!ok) ++failures;
   }
   return failures == 0 ? 0 : 1;
}
