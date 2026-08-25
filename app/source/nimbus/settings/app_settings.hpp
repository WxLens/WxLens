#pragma once

#include <memory>

#include <QObject>
#include <QString>
#include <QVariantList>

namespace nimbus
{
namespace settings
{

class SettingsStore;

/**
 * Typed, validated, QML-facing settings (docs/ROADMAP.md §3.2's `settings_variable` pattern:
 * validated defaults plus Qt-signal change notification, over the TOML store rather than
 * QSettings).
 *
 * **Every section carries a stable id from the first commit** (§4.5). That section is explicit
 * that a quick control must be able to open Settings "already scrolled to and highlighting the
 * specific section that sets its default", and that retrofitting addressability onto a tree built
 * without it is the expensive way to get there. So ids are part of the data model here, not a
 * detail of the dialog - the dialog renders `sections()`, and a deep-link is just an id.
 *
 * Writes persist immediately. A settings change that is only in memory looks identical to one
 * that stuck until the next launch, which is precisely the bug users cannot diagnose.
 */
class AppSettings : public QObject
{
   Q_OBJECT

   /// Which gesture starts a measurement (§4.4). See MeasurementGesture for why this is a
   /// preference rather than a fixed behaviour.
   Q_PROPERTY(int measurementGesture READ measurementGesture WRITE setMeasurementGesture NOTIFY
                 measurementGestureChanged)

   /**
    * Default scope for newly placed objects (§4.3). Explicitly a setting, not a constant: that
    * section records that RadarOmega and RadarScope disagree here and both are right for their
    * users, so the shipped default is `CurrentPaneOnly` but the value is read from config.
    */
   Q_PROPERTY(int defaultObjectScope READ defaultObjectScope WRITE setDefaultObjectScope NOTIFY
                 defaultObjectScopeChanged)

   /// Distance/altitude unit display (§4.4's deferred unit preference).
   Q_PROPERTY(
      int distanceUnits READ distanceUnits WRITE setDistanceUnits NOTIFY distanceUnitsChanged)

   /// Basemap appearance: follow the chrome by default, or force dark/light independently.
   Q_PROPERTY(int mapTheme READ mapTheme WRITE setMapTheme NOTIFY mapThemeChanged)

   /**
    * The §4.7 radar-geometry rows, as {id, label, visible, locked, note}. A list rather than one
    * property per row so the settings UI and the readout iterate the same catalogue and cannot
    * drift apart.
    */
   Q_PROPERTY(QVariantList geometryRows READ geometryRows NOTIFY geometryRowsChanged)

public:
   /**
    * Slice 7 shipped press-drag-release and click-then-click-again together, so neither habit is
    * punished. That is a good default and a bad mandate: with both live, a click that does not
    * move leaves a measurement half-started and waiting, so a stray click arms something the user
    * did not intend.
    */
   enum class MeasurementGesture
   {
      Both = 0,  ///< drag or click-click, whichever the user does
      DragOnly,  ///< a click that does not move is ignored
      ClickOnly  ///< click to start, click to finish; a drag is treated as the first click
   };
   Q_ENUM(MeasurementGesture)

   enum class DistanceUnits
   {
      Both = 0, ///< metric and customary together - the shipped default, and what slice 7 hardcoded
      Metric,
      Imperial
   };
   Q_ENUM(DistanceUnits)

   enum class MapTheme
   {
      FollowChrome = 0,
      Dark,
      Light
   };
   Q_ENUM(MapTheme)

   explicit AppSettings(SettingsStore& store, QObject* parent = nullptr);
   ~AppSettings() override;

   AppSettings(const AppSettings&)            = delete;
   AppSettings& operator=(const AppSettings&) = delete;
   AppSettings(AppSettings&&)                 = delete;
   AppSettings& operator=(AppSettings&&)      = delete;

   [[nodiscard]] int measurementGesture() const;
   [[nodiscard]] int defaultObjectScope() const;
   [[nodiscard]] int distanceUnits() const;
   [[nodiscard]] int mapTheme() const;

   void setMeasurementGesture(int gesture);
   void setDefaultObjectScope(int scopeKind);
   void setDistanceUnits(int units);
   void setMapTheme(int theme);

   [[nodiscard]] QVariantList geometryRows() const;

   /// Whether one §4.7 row is shown. Unknown ids answer true - a row the settings model has never
   /// heard of is more likely a newer row than a mistake, and hiding data by default is the wrong
   /// failure direction for this particular readout.
   [[nodiscard]] Q_INVOKABLE bool geometryRowVisible(const QString& rowId) const;

   Q_INVOKABLE void setGeometryRowVisible(const QString& rowId, bool visible);

   /**
    * The addressable settings sections (§4.5), as {id, title, summary}. Ids are stable and are
    * what a deep-linking quick control passes; titles and summaries are display text.
    */
   [[nodiscard]] Q_INVOKABLE QVariantList sections() const;

   /// Whether `sectionId` is one this build knows about - so a stale deep-link opens Settings at
   /// the top rather than at a blank panel.
   [[nodiscard]] Q_INVOKABLE bool hasSection(const QString& sectionId) const;

   /// Restores every setting to its shipped default and persists that.
   Q_INVOKABLE void resetToDefaults();

   /// Where the config files live, so the settings UI can show the user the directory it is
   /// writing - the file being hand-editable is only useful if it is findable.
   [[nodiscard]] Q_INVOKABLE QString configDirectory() const;

signals:
   void measurementGestureChanged();
   void defaultObjectScopeChanged();
   void distanceUnitsChanged();
   void mapThemeChanged();
   void geometryRowsChanged();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace settings
} // namespace nimbus
