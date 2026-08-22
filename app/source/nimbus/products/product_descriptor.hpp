#pragma once

#include <QString>

namespace nimbus
{
namespace products
{

/**
 * What a pane is currently displaying, described generically rather than as radar-specific
 * fields on the pane itself (docs/ROADMAP.md §4.6: "Do not give PaneController radar-specific
 * fields directly" - a pane is a visualization workspace, not a fixed radar viewport).
 *
 * Radar is the only `kind` implemented today, and this deliberately stays a plain value type
 * rather than a class hierarchy: the point is that PaneController never names a radar site, not
 * that speculative satellite/model product types get modelled up front (§0's "no premature
 * later-phase tech" rule). When a second kind arrives, the dispatch that reads `kind` -
 * PaneController::attachLayers - is the one place that grows a branch.
 */
struct ProductDescriptor
{
   QString kind {QStringLiteral("radar")};        ///< data domain, e.g. "radar"
   QString sourceKey {};                          ///< source id within the domain, e.g. "KEAX"
   QString product {QStringLiteral("Reflectivity")}; ///< product within the source

   bool operator==(const ProductDescriptor& o) const
   {
      return kind == o.kind && sourceKey == o.sourceKey && product == o.product;
   }
};

} // namespace products
} // namespace nimbus
