#include <wxlens/settings/experience_profile.hpp>

#include <array>

namespace wxlens
{
namespace settings
{

namespace
{

using L = ExperienceLevel;

/**
 * THE level-to-key table (ADR 0005 §2). Only keys whose feature is actually gated belong here: a
 * key listed in Settings that hides nothing would be a dead toggle. ADR 0005 names more keys
 * (radar.site_picker, radar.product_browser, weather.forecast_card, ...); each is added in the
 * slice that gates its feature. radar.site_picker in particular must wait for automatic site
 * selection (ADR 0005 §6) - hiding the picker before that would strand Basic on one site.
 *
 * Ids are persisted and referenced from QML, so treat them as a contract.
 */
constexpr std::array<RevealKey, 5> kRevealKeys {{
   {"panes.multi",
    "Multiple panes",
    "Show the layout control for splitting the map into several panes.",
    L::Advanced},
   {"panes.sync_controls",
    "Pane sync controls",
    "Show each pane's linking menu, for independent views and custom links. When hidden, all "
    "maps move together.",
    L::Professional},
   {"tools.measurement",
    "Measurement",
    "Show the distance and bearing measurement tool.",
    L::Advanced},
   {"tools.markers",
    "Place markers",
    "Show the marker tool for pinning points on the map.",
    L::Advanced},
   {"tools.drawing",
    "Drawing and range rings",
    "Show the line-drawing and range-ring analysis tools.",
    L::Professional},
}};

} // namespace

std::span<const RevealKey> ExperienceProfile::Keys()
{
   return kRevealKeys;
}

const RevealKey* ExperienceProfile::FindKey(const QString& id)
{
   for (const RevealKey& key : kRevealKeys)
   {
      if (id == QLatin1String(key.id))
      {
         return &key;
      }
   }
   return nullptr;
}

bool ExperienceProfile::LevelDefault(ExperienceLevel level, const QString& id)
{
   const RevealKey* key = FindKey(id);
   // The single place a level comparison is allowed (ADR 0005 §2).
   return key == nullptr || static_cast<int>(level) >= static_cast<int>(key->lowestLevel);
}

QString ExperienceProfile::LevelName(ExperienceLevel level)
{
   switch (level)
   {
   case ExperienceLevel::Advanced: return QStringLiteral("Advanced");
   case ExperienceLevel::Professional: return QStringLiteral("Professional");
   case ExperienceLevel::Basic:
   default: return QStringLiteral("Basic");
   }
}

bool ExperienceProfile::IsValidLevel(int level)
{
   return level >= static_cast<int>(ExperienceLevel::Basic) &&
          level <= static_cast<int>(ExperienceLevel::Professional);
}

bool ExperienceProfile::SetLevel(ExperienceLevel level)
{
   if (level == level_ && overrides_.empty())
   {
      return false;
   }
   level_ = level;
   overrides_.clear();
   return true;
}

bool ExperienceProfile::IsRevealed(const QString& id) const
{
   const auto it = overrides_.find(id);
   return it != overrides_.end() ? it->second : LevelDefault(level_, id);
}

bool ExperienceProfile::SetRevealed(const QString& id, bool revealed)
{
   if (FindKey(id) == nullptr)
   {
      return false;
   }
   const bool before = IsRevealed(id);
   if (revealed == LevelDefault(level_, id))
   {
      overrides_.erase(id);
   }
   else
   {
      overrides_[id] = revealed;
   }
   return before != revealed;
}

bool ExperienceProfile::ClearOverrides()
{
   if (overrides_.empty())
   {
      return false;
   }
   overrides_.clear();
   return true;
}

} // namespace settings
} // namespace wxlens
