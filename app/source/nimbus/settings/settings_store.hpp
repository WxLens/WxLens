#pragma once

#include <memory>

#include <QString>

namespace nimbus
{
namespace settings
{

/**
 * The structured config store (docs/ROADMAP.md §3.2, docs/adr/0003-config-storage-format.md).
 *
 * TOML files under QStandardPaths::AppConfigLocation, **one file per category** so the on-disk
 * layout mirrors the typed-accessor split rather than becoming one monolithic blob. Deliberately
 * not QSettings: §3.2 rules it out because it is registry-coupled on Windows, which precludes the
 * portable, hand-editable, shareable config the roadmap wants (and which the Phase 5 multi-user
 * stretch goal would need).
 *
 * **Hand-editing is a supported workflow, not an accident**, which drives the whole error policy
 * here: a malformed file, a missing key, a value of the wrong type, or a number outside its valid
 * range must never crash, never silently corrupt neighbouring values, and never propagate garbage
 * into the app. Every read validates and falls back to the caller's default, logging what it
 * rejected so a user who typo'd a file can find out why their change did nothing. A file that
 * fails to parse at all is left untouched on disk rather than being overwritten with defaults -
 * silently destroying someone's hand-written config is worse than ignoring it for one session.
 *
 * This is the storage layer only. Typed, validated, QML-facing accessors live in AppSettings; the
 * roadmap's `settings_variable`/`settings_interface` pattern (validated defaults + Qt-signal
 * change notification) is that class's job, not this one's.
 */
class SettingsStore
{
public:
   SettingsStore();
   ~SettingsStore();

   SettingsStore(const SettingsStore&)            = delete;
   SettingsStore& operator=(const SettingsStore&) = delete;
   SettingsStore(SettingsStore&&)                 = delete;
   SettingsStore& operator=(SettingsStore&&)      = delete;

   /// Process-wide store over the real config directory.
   static SettingsStore& Instance();

   /**
    * Points this store at a different config directory. Exists so tests can run against a
    * temporary directory instead of the developer's real settings - without it, running the suite
    * would read and rewrite the config of whoever is sitting at the machine.
    *
    * Discards anything already loaded; the next read re-reads from the new location.
    */
   void SetConfigDirectory(const QString& path);

   [[nodiscard]] QString ConfigDirectory() const;

   /// Absolute path of one category's file, e.g. ".../measurement.toml".
   [[nodiscard]] QString FilePath(const QString& category) const;

   /**
    * Typed reads. `defaultValue` is returned whenever the key is absent, holds the wrong type, or
    * (for numbers) falls outside [minimum, maximum] - the validated-defaults half of §3.2's
    * pattern.
    */
   [[nodiscard]] bool GetBool(const QString& category, const QString& key, bool defaultValue);
   [[nodiscard]] int
   GetInt(const QString& category, const QString& key, int defaultValue, int minimum, int maximum);
   [[nodiscard]] double GetDouble(const QString& category,
                                  const QString& key,
                                  double         defaultValue,
                                  double         minimum,
                                  double         maximum);
   [[nodiscard]] QString
   GetString(const QString& category, const QString& key, const QString& defaultValue);

   void SetBool(const QString& category, const QString& key, bool value);
   void SetInt(const QString& category, const QString& key, int value);
   void SetDouble(const QString& category, const QString& key, double value);
   void SetString(const QString& category, const QString& key, const QString& value);

   /**
    * Writes every category modified since the last save. Returns false if any file could not be
    * written - callers should surface that rather than assume a silent success, since a settings
    * change that did not persist looks identical to one that did until the next launch.
    */
   bool Save();

   /// Drops all loaded state, so the next read comes from disk again. Used by tests and by a
   /// future "reload config" action.
   void Reload();

   /// Whether the category's file failed to parse. Its values fall back to defaults, and Save()
   /// refuses to overwrite it (see the class comment).
   [[nodiscard]] bool CategoryFailedToParse(const QString& category);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace settings
} // namespace nimbus
