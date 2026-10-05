#pragma once

#include <QString>

#include <string>
#include <vector>

namespace wxlens
{
namespace products
{

struct Level3ProductDescriptor
{
   QString categoryId;
   QString categoryDescription;
   QString productId;
   QString description;
   QString awipsId;

   /**
    * Product/tilt display preference fields (docs/ROADMAP.md Phase 1 slice 3F "near-term
    * product/tilt presentation follow-up"). Shape is final so QML can bind against it now;
    * family and recommendation values are derived from wxdata's canonical product order; angle
    * values remain empty until reported/nominal elevation metadata is available:
    *
    * - `family`: meteorological grouping a friendly picker should collapse tilt/source variants
    *   under (e.g. every CC AWIPS id beneath one "Correlation Coefficient" row).
    * - `recommended`: marks the one entry per family a friendly picker should show by default.
    *   Uses the first site-available entry in wxdata's canonical order, never provider order.
    * - `elevationAngleText`: the tilt angle to display, exact when the loaded product reports one
    *   or the code's nominal angle marked approximate otherwise. Empty until that data is wired
    *   through - do not read emptiness as "no tilt".
    * - `elevationIsExact`: true only once `elevationAngleText` holds a real reported elevation
    *   rather than a nominal code angle.
    */
   QString family {};
   bool    recommended {false};
   QString elevationAngleText {};
   bool    elevationIsExact {false};

   bool operator==(const Level3ProductDescriptor&) const = default;
};

/**
 * The product picker's section headings.
 *
 * Level 2 moments (PaneController::productCatalog) and Level 3 categories (BuildLevel3ProductCatalog)
 * both group under these, and products that belong under one heading come from both - Level 2 VEL/SW
 * sit beside the Level 3 velocity products. Two parallel tables of section *strings* would split
 * those into differently-labeled groups the moment either side was reworded, so the strings exist
 * once, here, and both sides name a section rather than spelling one.
 */
enum class ProductSectionId
{
   Reflectivity,
   Velocity,
   DualPolarization,
   PrecipitationAccumulation,
   Other
};

[[nodiscard]] QString ProductSectionName(ProductSectionId section);

/** Maps a provider's site-specific AWIPS availability through wxdata's product
 * catalog. */
std::vector<Level3ProductDescriptor>
BuildLevel3ProductCatalog(const std::vector<std::string>& availableAwipsIds);

} // namespace products
} // namespace wxlens
