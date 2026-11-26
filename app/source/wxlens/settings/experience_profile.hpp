#pragma once

#include <map>
#include <span>

#include <QString>

namespace wxlens
{
namespace settings
{

/**
 * The three built-in experience levels (ADR 0005 §1). Ordinals are persisted, so append only.
 *
 * Nothing outside ExperienceProfile may compare levels (ADR 0005 §2): features ask whether their
 * reveal key is revealed, and the one table in experience_profile.cpp decides which level first
 * shows each key. Moving a feature to another tier is then a one-line edit there.
 */
enum class ExperienceLevel
{
   Basic = 0,
   Advanced,
   Professional
};

/// One gated capability (ADR 0005 §2). `lowestLevel` is the whole level-to-key table: a key is
/// revealed by default at that level and every level above it.
struct RevealKey
{
   const char*     id;          ///< stable, persisted and checked by QML, e.g. "panes.multi"
   const char*     label;       ///< settings-UI text
   const char*     description; ///< settings-UI explanation of what turning it on shows
   ExperienceLevel lowestLevel;
};

/**
 * The chosen experience level plus the user's per-key overrides on top of it (ADR 0005 §1-§3).
 *
 * Pure state with no storage or Qt object machinery, so the rules are testable on their own;
 * AppSettings persists it and exposes it to QML.
 *
 * Overrides are held only while they differ from the level's default. Setting a key back to the
 * level default removes its override rather than storing a redundant one, which is what makes
 * HasOverrides() mean "the user customized this level" and not "the user once touched a toggle".
 */
class ExperienceProfile
{
public:
   static constexpr ExperienceLevel kDefaultLevel = ExperienceLevel::Basic;

   /// Every reveal key this build knows about, in settings-UI order.
   [[nodiscard]] static std::span<const RevealKey> Keys();
   [[nodiscard]] static const RevealKey*           FindKey(const QString& id);
   [[nodiscard]] static bool LevelDefault(ExperienceLevel level, const QString& id);
   [[nodiscard]] static QString LevelName(ExperienceLevel level);

   /// Validates a persisted or QML-supplied ordinal.
   [[nodiscard]] static bool IsValidLevel(int level);

   [[nodiscard]] ExperienceLevel Level() const { return level_; }

   /**
    * Switches level and clears every override - picking a tier from the dropdown is the "one
    * action that brings back that view" ADR 0005 exists for, so leftover overrides from the
    * previous tier must not leak into it. Returns whether anything changed.
    */
   bool SetLevel(ExperienceLevel level);

   /**
    * Whether a capability is shown. A key this build does not know is revealed: ADR 0005 says
    * features without a key are visible at every level, and hiding something because of a typo
    * or a newer key is the wrong failure direction.
    */
   [[nodiscard]] bool IsRevealed(const QString& id) const;

   /// Records (or, when `revealed` equals the level default, removes) an override. Unknown keys
   /// are refused. Returns whether the effective value changed.
   bool SetRevealed(const QString& id, bool revealed);

   [[nodiscard]] bool HasOverrides() const { return !overrides_.empty(); }
   [[nodiscard]] const std::map<QString, bool>& Overrides() const { return overrides_; }

   /// Drops every override, keeping the level. Returns whether anything changed.
   bool ClearOverrides();

private:
   ExperienceLevel         level_ {kDefaultLevel};
   std::map<QString, bool> overrides_ {};
};

} // namespace settings
} // namespace wxlens
