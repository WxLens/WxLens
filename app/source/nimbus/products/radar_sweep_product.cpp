#include <nimbus/products/radar_sweep_product.hpp>
#include <nimbus/data/radar_site_data_service.hpp>
#include <nimbus/data/radar_site_database.hpp>
#include <nimbus/log/logger.hpp>
#include <nimbus/util/geodesic.hpp>

#include <scwx/common/color_table.hpp>
#include <scwx/common/constants.hpp>
#include <scwx/common/geographic.hpp>
#include <scwx/wsr88d/rda/generic_radar_data.hpp>

#include <algorithm>
#include <chrono>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <sstream>

#include <units/angle.h>
#include <units/length.h>
#include <QFile>

namespace nimbus
{
namespace products
{

static const std::string logPrefix_ = "products.radar_sweep_product";
static const auto        logger_    = nimbus::log::Create(logPrefix_);

namespace
{
using scwx::wsr88d::rda::DataBlockType;
using scwx::wsr88d::rda::ElevationScan;

// Ported from Level2ProductView (level2_product_view.cpp) - see this project's
// docs/adr/0004-maplibre-qml-integration.md and docs/ROADMAP.md §7 Phase 1 slice 3 for context on
// why only the non-smoothing, non-CFP path is ported this slice.
constexpr std::uint16_t kRangeFolded      = 1u;
constexpr std::uint32_t kVerticesPerBin   = 6u;
constexpr std::uint32_t kValuesPerVertex  = 2u;
constexpr std::size_t   kVerticesPerGate       = 6u;
constexpr std::size_t   kVerticesPerOriginGate = 3u;

constexpr std::uint32_t kMaxRadialGates_ =
   scwx::common::MAX_0_5_DEGREE_RADIALS * scwx::common::MAX_DATA_MOMENT_GATES;
constexpr std::uint32_t kMaxCoordinates_ = kMaxRadialGates_ * 2u;

units::degrees<float> NormalizeAngle(units::degrees<float> angle)
{
   constexpr auto angleLimit = units::degrees<float> {180.0f};
   constexpr auto fullAngle  = units::degrees<float> {360.0f};

   while (angle < -angleLimit)
   {
      angle += fullAngle;
   }
   while (angle >= angleLimit)
   {
      angle -= fullAngle;
   }

   return angle;
}

bool IsRadarDataIncomplete(const ElevationScan& radarData)
{
   constexpr units::degrees<float> kIncompleteDataAngleThreshold {2.5f};

   if (radarData.empty() || radarData.cbegin()->second == nullptr ||
       radarData.crbegin()->second == nullptr)
   {
      return false;
   }

   const units::degrees<float> firstAngle = radarData.cbegin()->second->azimuth_angle();
   const units::degrees<float> lastAngle  = radarData.crbegin()->second->azimuth_angle();
   const units::degrees<float> angleDelta =
      scwx::common::GetAngleDelta(firstAngle, lastAngle);

   return angleDelta > kIncompleteDataAngleThreshold;
}

// Non-smoothing path only, ported from Level2ProductView::Impl::ComputeCoordinates.
std::vector<float> ComputeCoordinates(const ElevationScan& radarData,
                                      DataBlockType         dataBlockType,
                                      double                 radarLatitude,
                                      double                 radarLongitude)
{
   std::vector<float> coordinates(kMaxCoordinates_, 0.0f);

   const auto radarData0It = radarData.find(0);
   if (radarData0It == radarData.cend() || radarData0It->second == nullptr)
   {
      logger_->warn("Empty radial data");
      return coordinates;
   }

   const auto& radarData0  = radarData0It->second;
   const auto  momentData0 = radarData0->moment_data_block(dataBlockType);
   if (momentData0 == nullptr)
   {
      logger_->warn("No moment data");
      return coordinates;
   }

   const auto gateSize = momentData0->data_moment_range_sample_interval();

   std::uint16_t numRadials =
      static_cast<std::uint16_t>(radarData.crbegin()->first + 1);
   const std::uint16_t numRangeBins = std::max(
      momentData0->number_of_data_moment_gates() + 1u, scwx::common::MAX_DATA_MOMENT_GATES);

   if (IsRadarDataIncomplete(radarData))
   {
      ++numRadials;
   }

   numRadials = std::min<std::uint16_t>(numRadials, scwx::common::MAX_0_5_DEGREE_RADIALS);

   // Far end of the first gate is the gate size distance from the radar site (non-smoothing).
   constexpr float gateRangeOffset = 1.0f;

   for (std::uint32_t radial = 0; radial < numRadials; ++radial)
   {
      units::degrees<float> angle {};

      const auto radialData    = radarData.find(radial);
      auto       prevRadial1It = radarData.find((radial >= 1) ? radial - 1 : numRadials - (1 - radial));
      auto       prevRadial2It = radarData.find((radial >= 2) ? radial - 2 : numRadials - (2 - radial));

      if (radialData != radarData.cend() && radialData->second != nullptr &&
          prevRadial1It != radarData.cend() && prevRadial1It->second != nullptr)
      {
         const units::degrees<float> currentAngle = radialData->second->azimuth_angle();
         const units::degrees<float> prevAngle     = prevRadial1It->second->azimuth_angle();
         const units::degrees<float> deltaAngle = NormalizeAngle(currentAngle - prevAngle);

         constexpr float deltaScale = 0.5f;
         angle                      = currentAngle - deltaAngle * deltaScale;
      }
      else if (radialData != radarData.cend() && radialData->second != nullptr)
      {
         const units::degrees<float> currentAngle = radialData->second->azimuth_angle();
         constexpr units::degrees<float> deltaAngle {0.5f};
         constexpr float                 deltaScale = 0.5f;

         angle = currentAngle - deltaAngle * deltaScale;
      }
      else if (prevRadial1It != radarData.cend() && prevRadial1It->second != nullptr &&
               prevRadial2It != radarData.cend() && prevRadial2It->second != nullptr)
      {
         const units::degrees<float> prevAngle1 = prevRadial1It->second->azimuth_angle();
         const units::degrees<float> prevAngle2 = prevRadial2It->second->azimuth_angle();
         const units::degrees<float> deltaAngle  = NormalizeAngle(prevAngle1 - prevAngle2);

         constexpr float deltaScale = 0.5f;
         angle                      = prevAngle1 + deltaAngle * deltaScale;
      }
      else if (prevRadial1It != radarData.cend() && prevRadial1It->second != nullptr)
      {
         const units::degrees<float> prevAngle1 = prevRadial1It->second->azimuth_angle();
         constexpr units::degrees<float> deltaAngle {0.5f};
         constexpr float                 deltaScale = 0.5f;

         angle = prevAngle1 + deltaAngle * deltaScale;
      }
      else
      {
         // Not enough angles present to determine an angle for this radial - leave it zeroed.
         continue;
      }

      for (std::uint32_t gate = 0; gate < numRangeBins; ++gate)
      {
         const std::uint32_t radialGate = radial * scwx::common::MAX_DATA_MOMENT_GATES + gate;
         const units::length::meters<float> range =
            (static_cast<float>(gate) + gateRangeOffset) * gateSize;
         const std::size_t offset = static_cast<std::size_t>(radialGate) * 2;

         const auto [latitude, longitude] = nimbus::util::GeodesicDirect(
            radarLatitude, radarLongitude, angle.value(), range.value());

         coordinates[offset]     = static_cast<float>(latitude);
         coordinates[offset + 1] = static_cast<float>(longitude);
      }
   }

   return coordinates;
}

std::shared_ptr<SweepData> ComputeSweep(const ElevationScan& radarData,
                                        DataBlockType         dataBlockType,
                                        double                 radarLatitude,
                                        double                 radarLongitude,
                                        std::shared_ptr<scwx::common::ColorTable> colorTable)
{
   const auto radarData0It = radarData.find(0);
   if (radarData0It == radarData.cend() || radarData0It->second == nullptr)
   {
      logger_->warn("Empty radial data for sweep");
      return nullptr;
   }

   const auto& radarData0  = radarData0It->second;
   const auto  momentData0 = radarData0->moment_data_block(dataBlockType);
   if (momentData0 == nullptr)
   {
      logger_->warn("No moment data for sweep");
      return nullptr;
   }

   std::size_t radials       = radarData.crbegin()->first + 1;
   std::size_t vertexRadials = radials;

   if (IsRadarDataIncomplete(radarData))
   {
      ++vertexRadials;
   }

   radials       = std::min<std::size_t>(radials, scwx::common::MAX_0_5_DEGREE_RADIALS);
   vertexRadials = std::min<std::size_t>(vertexRadials, scwx::common::MAX_0_5_DEGREE_RADIALS);

   const std::vector<float> coordinates =
      ComputeCoordinates(radarData, dataBlockType, radarLatitude, radarLongitude);

   const std::uint32_t gates = momentData0->number_of_data_moment_gates();

   auto result = std::make_shared<SweepData>();

   std::vector<float>& vertices = result->vertices;
   vertices.resize(vertexRadials * gates * kVerticesPerBin * kValuesPerVertex);
   std::size_t vIndex = 0;

   std::vector<std::uint8_t>&  dataMoments8  = result->dataMoments8;
   std::vector<std::uint16_t>& dataMoments16 = result->dataMoments16;
   std::size_t                 mIndex        = 0;

   const bool is8Bit = (momentData0->data_word_size() == 8);
   if (is8Bit)
   {
      dataMoments8.resize(radials * gates * kVerticesPerBin);
   }
   else
   {
      dataMoments16.resize(radials * gates * kVerticesPerBin);
   }

   const std::uint16_t snrThreshold =
      static_cast<std::uint16_t>(std::max<std::int16_t>(2, momentData0->snr_threshold_raw()));

   constexpr std::uint16_t startRadial = 0u;

   for (const auto& radialPair : radarData)
   {
      const std::uint16_t radial     = radialPair.first;
      const auto&          radialData = radialPair.second;
      if (radialData == nullptr)
      {
         continue;
      }

      const auto momentData = radialData->moment_data_block(dataBlockType);
      if (momentData == nullptr || momentData0->data_word_size() != momentData->data_word_size())
      {
         continue;
      }

      const std::int32_t dataMomentInterval  = momentData->data_moment_range_sample_interval_raw();
      const std::int32_t dataMomentIntervalH = dataMomentInterval / 2;
      const std::int32_t dataMomentRange =
         std::max<std::int32_t>(momentData->data_moment_range_raw(), dataMomentIntervalH);

      const units::length::meters<float> gateSize = momentData->data_moment_range_sample_interval();
      const auto gateSizeMeters = static_cast<std::int32_t>(gateSize.value());

      constexpr std::int32_t gatesPerBin = 1;

      const std::int32_t startGate =
         (gateSizeMeters > 0) ? (dataMomentRange - dataMomentIntervalH) / gateSizeMeters : 0;
      const std::int32_t numberOfDataMomentGates = std::min<std::int32_t>(
         momentData->number_of_data_moment_gates(), static_cast<std::int32_t>(gates));
      const std::int32_t endGate = std::min<std::int32_t>(
         startGate + numberOfDataMomentGates * gatesPerBin,
         static_cast<std::int32_t>(scwx::common::MAX_DATA_MOMENT_GATES));

      const std::uint8_t*  dataMomentsArray8  = nullptr;
      const std::uint16_t* dataMomentsArray16 = nullptr;

      if (is8Bit)
      {
         dataMomentsArray8 = reinterpret_cast<const std::uint8_t*>(momentData->data_moments());
      }
      else
      {
         dataMomentsArray16 = reinterpret_cast<const std::uint16_t*>(momentData->data_moments());
      }

      for (std::int32_t gate = startGate, i = 0; gate + gatesPerBin <= endGate;
          gate += gatesPerBin, ++i)
      {
         if (gate < 0)
         {
            continue;
         }

         const std::size_t vertexCount = (gate > 0) ? kVerticesPerGate : kVerticesPerOriginGate;

         if (is8Bit)
         {
            const std::uint8_t& dataValue = dataMomentsArray8[i];
            if (dataValue < snrThreshold && dataValue != kRangeFolded)
            {
               continue;
            }
            for (std::size_t m = 0; m < vertexCount; ++m)
            {
               dataMoments8[mIndex++] = dataValue;
            }
         }
         else
         {
            const std::uint16_t& dataValue = dataMomentsArray16[i];
            if (dataValue < snrThreshold && dataValue != kRangeFolded)
            {
               continue;
            }
            for (std::size_t m = 0; m < vertexCount; ++m)
            {
               dataMoments16[mIndex++] = dataValue;
            }
         }

         if (gate > 0)
         {
            // Draw two triangles per gate:
            // 2 +---+ 4
            //   |  /|
            //   | / |
            //   |/  |
            // 1 +---+ 3
            const std::uint16_t baseCoord = static_cast<std::uint16_t>(gate - 1);

            const std::size_t offset1 =
               ((startRadial + radial) % vertexRadials * scwx::common::MAX_DATA_MOMENT_GATES +
                baseCoord) *
               2;
            const std::size_t offset2 = offset1 + static_cast<std::size_t>(gatesPerBin) * 2;
            const std::size_t offset3 =
               (((startRadial + radial + 1) % vertexRadials) *
                   scwx::common::MAX_DATA_MOMENT_GATES +
                baseCoord) *
               2;
            const std::size_t offset4 = offset3 + static_cast<std::size_t>(gatesPerBin) * 2;

            vertices[vIndex++] = coordinates[offset1];
            vertices[vIndex++] = coordinates[offset1 + 1];
            vertices[vIndex++] = coordinates[offset2];
            vertices[vIndex++] = coordinates[offset2 + 1];
            vertices[vIndex++] = coordinates[offset4];
            vertices[vIndex++] = coordinates[offset4 + 1];
            vertices[vIndex++] = coordinates[offset1];
            vertices[vIndex++] = coordinates[offset1 + 1];
            vertices[vIndex++] = coordinates[offset3];
            vertices[vIndex++] = coordinates[offset3 + 1];
            vertices[vIndex++] = coordinates[offset4];
            vertices[vIndex++] = coordinates[offset4 + 1];
         }
         else
         {
            const std::uint16_t baseCoord = static_cast<std::uint16_t>(gate);

            const std::size_t offset1 =
               ((startRadial + radial) % vertexRadials * scwx::common::MAX_DATA_MOMENT_GATES +
                baseCoord) *
               2;
            const std::size_t offset2 =
               (((startRadial + radial + 1) % vertexRadials) *
                   scwx::common::MAX_DATA_MOMENT_GATES +
                baseCoord) *
               2;

            vertices[vIndex++] = static_cast<float>(radarLatitude);
            vertices[vIndex++] = static_cast<float>(radarLongitude);
            vertices[vIndex++] = coordinates[offset1];
            vertices[vIndex++] = coordinates[offset1 + 1];
            vertices[vIndex++] = coordinates[offset2];
            vertices[vIndex++] = coordinates[offset2 + 1];
         }
      }
   }

   vertices.resize(vIndex);
   vertices.shrink_to_fit();

   if (is8Bit)
   {
      dataMoments8.resize(mIndex);
      dataMoments8.shrink_to_fit();
   }
   else
   {
      dataMoments16.resize(mIndex);
      dataMoments16.shrink_to_fit();
   }

   // Color table LUT - ported from Level2ProductView::UpdateColorTableLut, Reflectivity's
   // rangeMin/rangeMax case only (the switch's default branch upstream), no threshold masking
   // (that comes from a settings-driven "color_table_threshold()" this slice doesn't have yet).
   if (colorTable != nullptr && colorTable->IsValid())
   {
      const float offset = momentData0->offset();
      const float scale   = momentData0->scale();

      constexpr std::uint16_t rangeMin = 1;
      constexpr std::uint16_t rangeMax = 255;

      auto& lut = result->colorTableLut;
      lut.resize(rangeMax - rangeMin + 1);

      for (std::uint16_t i = rangeMin; i <= rangeMax; ++i)
      {
         if (i == kRangeFolded)
         {
            lut[i - rangeMin] = colorTable->rf_color();
         }
         else
         {
            const float f = (static_cast<float>(i) - offset) / scale;
            lut[i - rangeMin] = colorTable->Color(f);
         }
      }

      result->colorTableMin = rangeMin;
      result->colorTableMax = rangeMax;
   }

   return result;
}

std::shared_ptr<scwx::common::ColorTable> LoadBundledColorTable()
{
   // The default reflectivity palette ("BR"/"DR" in the legacy app's PaletteSettings), sourced
   // from NOAA's Weather and Climate Toolkit (see ACKNOWLEDGEMENTS.md). ColorTable::Load(filename)
   // uses a plain std::ifstream (wxdata is Qt-free), so a Qt-resource-bundled ":/..." path has to
   // go through QFile + the Load(std::istream&) overload instead - matches how the legacy Qt app
   // itself bridges its QRC-bundled palettes into this same Qt-free loader.
   QFile file(":/qt/qml/Nimbus/App/res/palettes/wct/DR.pal");
   if (!file.open(QIODevice::ReadOnly))
   {
      logger_->error("Failed to open bundled default color table");
      return nullptr;
   }

   const QByteArray  data = file.readAll();
   std::istringstream iss(std::string(data.constData(), static_cast<std::size_t>(data.size())));

   auto colorTable = scwx::common::ColorTable::Load(iss);
   if (colorTable == nullptr || !colorTable->IsValid())
   {
      logger_->error("Bundled default color table failed to parse");
      return nullptr;
   }

   return colorTable;
}

} // namespace

class RadarSweepProduct::Impl
{
public:
   explicit Impl(RadarSweepProduct* self,
                 const std::string& radarSite,
                 double             siteLatitude,
                 double             siteLongitude,
                 double             siteAltitudeMslMeters) :
       self_ {self},
       radarSite_ {radarSite},
       siteLatitude_ {siteLatitude},
       siteLongitude_ {siteLongitude},
       siteAltitudeMslMeters_ {siteAltitudeMslMeters},
       colorTable_ {LoadBundledColorTable()}
   {
   }

   void OnLevelTwoDataLoaded(const std::shared_ptr<scwx::wsr88d::Ar2vFile>& file);

   RadarSweepProduct* self_;

   std::string radarSite_;
   double      siteLatitude_;
   double      siteLongitude_;
   double      siteAltitudeMslMeters_;

   std::shared_ptr<scwx::common::ColorTable> colorTable_;

   mutable std::mutex               dataMutex_;
   std::shared_ptr<const SweepData> data_;

   /// The tilt of the cut `data_` was built from. Guarded by dataMutex_ alongside data_, because
   /// the two must never be read out of step: an altitude computed from one sweep's range and a
   /// different sweep's elevation angle is a number that describes nothing.
   std::optional<double> elevationAngleDegrees_ {};
};

void RadarSweepProduct::Impl::OnLevelTwoDataLoaded(
   const std::shared_ptr<scwx::wsr88d::Ar2vFile>& file)
{
   logger_->debug("Computing sweep for {}", radarSite_);

   // Lowest elevation cut, latest available scan in this volume (an empty time_point means "no
   // constraint, take the newest" - see Ar2vFile::GetElevationScan). Which cut is *selected* is
   // still fixed here; the volume's full list of cuts is what elevation selection (slice 4+) will
   // need. `elevationCut` is the tilt this volume actually answered with, and slice 8's beam
   // geometry reports it rather than assuming the nominal 0.5°: a VCP's lowest cut is not always
   // 0.5°, and §4.7 forbids presenting a guessed angle as the radar's own.
   [[maybe_unused]] auto [elevationScan, elevationCut, elevationCuts] = file->GetElevationScan(
      DataBlockType::MomentRef, 0.0f, std::chrono::system_clock::time_point {});

   if (elevationScan == nullptr)
   {
      logger_->warn("No reflectivity elevation scan available for {}", radarSite_);
      return;
   }

   std::shared_ptr<SweepData> sweepData = ComputeSweep(
      *elevationScan, DataBlockType::MomentRef, siteLatitude_, siteLongitude_, colorTable_);

   if (sweepData == nullptr)
   {
      return;
   }

   logger_->info("Computed sweep for {}: {} vertices", radarSite_, sweepData->vertices.size() / 2);

   {
      std::scoped_lock lock {dataMutex_};
      data_                  = std::move(sweepData);
      elevationAngleDegrees_ = elevationCut;
   }

   Q_EMIT self_->SweepUpdated();
}

RadarSweepProduct::RadarSweepProduct(const std::string& radarSite,
                                     double             siteLatitude,
                                     double             siteLongitude,
                                     double             siteAltitudeMslMeters,
                                     QObject*           parent) :
    QObject(parent),
    p {std::make_unique<Impl>(
       this, radarSite, siteLatitude, siteLongitude, siteAltitudeMslMeters)}
{
   auto service = nimbus::data::RadarSiteDataService::Instance(radarSite);

   connect(service.get(),
           &nimbus::data::RadarSiteDataService::LevelTwoDataLoaded,
           this,
           [this](std::shared_ptr<scwx::wsr88d::Ar2vFile> file) { p->OnLevelTwoDataLoaded(file); });

   // This product owns requesting its own data, rather than depending on some other object having
   // done so first (which is what an earlier slice relied on, and which stops being true as soon
   // as more than one site can be on screen).
   service->LoadLatestLevel2Data();
}

RadarSweepProduct::~RadarSweepProduct() = default;

std::shared_ptr<RadarSweepProduct> RadarSweepProduct::Instance(const std::string& radarSite)
{
   static std::shared_mutex                                          instanceMutex;
   static std::map<std::string, std::shared_ptr<RadarSweepProduct>>  instances;

   std::shared_lock readLock {instanceMutex};
   if (auto it = instances.find(radarSite); it != instances.end())
   {
      return it->second;
   }
   readLock.unlock();

   const auto siteInfo = nimbus::data::FindRadarSite(radarSite);
   if (!siteInfo.has_value())
   {
      logger_->error("No site metadata found for {}", radarSite);
      return nullptr;
   }

   std::unique_lock writeLock {instanceMutex};
   auto [it, inserted] = instances.try_emplace(radarSite, nullptr);
   if (inserted)
   {
      it->second = std::make_shared<RadarSweepProduct>(
         radarSite, siteInfo->latitude, siteInfo->longitude, siteInfo->altitudeMslMeters);
   }
   return it->second;
}

const std::string& RadarSweepProduct::radar_site() const
{
   return p->radarSite_;
}

double RadarSweepProduct::site_latitude() const
{
   return p->siteLatitude_;
}

double RadarSweepProduct::site_longitude() const
{
   return p->siteLongitude_;
}

double RadarSweepProduct::site_altitude_msl_meters() const
{
   return p->siteAltitudeMslMeters_;
}

std::optional<double> RadarSweepProduct::elevation_angle_degrees() const
{
   std::scoped_lock lock {p->dataMutex_};
   return p->elevationAngleDegrees_;
}

std::shared_ptr<const SweepData> RadarSweepProduct::sweep_data() const
{
   std::scoped_lock lock {p->dataMutex_};
   return p->data_;
}

} // namespace products
} // namespace nimbus
