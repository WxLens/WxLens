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

   bool operator==(const Level3ProductDescriptor&) const = default;
};

/** Maps a provider's site-specific AWIPS availability through wxdata's product
 * catalog. */
std::vector<Level3ProductDescriptor>
BuildLevel3ProductCatalog(const std::vector<std::string>& availableAwipsIds);

} // namespace products
} // namespace wxlens
