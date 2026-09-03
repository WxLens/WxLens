#pragma once

#include <map>

#include <QObject>
#include <QStringList>

#include <wxlens/palettes/palette_model.hpp>

namespace wxlens
{
namespace settings
{
class SettingsStore;
}

namespace palettes
{

/**
 * Owns the palette catalog, the editor session's state-changing workflow, and the per-family
 * product defaults.
 *
 * Two distinct notions live here and must not be confused (docs/phase1-ux-feedback-2026-08-31.md,
 * "Product-aware palette ownership and defaults"):
 *
 * - The **active** palette is what the editor is showing. It is editor state only: selecting a
 *   palette to look at or edit never recolours a pane.
 * - A **family default** is which palette panes of one meteorological field use when they carry
 *   no explicit override of their own. Families group presentation variants of the same field
 *   (`SRV` is an alternate ramp for the `DV` velocity family; `KDP2` for `KDP`). "Apply to
 *   product" both publishes the edited palette text and makes that palette its family's default,
 *   so every velocity pane without an override follows it and no reflectivity pane can.
 *
 * Family defaults persist (category `palettes`) once bindSettings() has been called; an unbound
 * manager, as in most tests, keeps them in memory only.
 */
class PaletteManager : public QObject
{
   Q_OBJECT
   Q_PROPERTY(QStringList paletteNames READ paletteNames NOTIFY paletteNamesChanged)
   Q_PROPERTY(QString activeName READ activeName NOTIFY activePaletteChanged)
   Q_PROPERTY(bool activeIsFactoryPalette READ activeIsFactoryPalette NOTIFY activePaletteChanged)
   Q_PROPERTY(bool confirmationRequired READ confirmationRequired NOTIFY confirmationRequiredChanged)
   Q_PROPERTY(PaletteModel* editor READ editor CONSTANT)

   /// The palette each known family currently defaults to (its bundled default until the user
   /// applies another member). A list, not a map, so QML can test membership with indexOf.
   Q_PROPERTY(QStringList familyDefaultNames READ familyDefaultNames NOTIFY familyDefaultsChanged)

public:
   enum class UnsavedDecision
   {
      SaveCopy = 0,
      Discard,
      KeepEditing
   };
   Q_ENUM(UnsavedDecision)

   explicit PaletteManager(QObject* parent = nullptr);
   static PaletteManager& Instance();

   /**
    * Loads persisted family defaults from `store` and persists every later change to it. Values
    * that name a palette outside the family they are stored under are ignored, not repaired: a
    * hand-edited file is a supported workflow (SettingsStore), and a reflectivity ramp can never
    * be a velocity default no matter what the file says.
    */
   void bindSettings(settings::SettingsStore& store);

   QStringList paletteNames() const;
   QString activeName() const;
   bool activeIsFactoryPalette() const;
   bool confirmationRequired() const;
   PaletteModel* editor();
   QString activeText() const;

   /// The last *applied* text of a palette - what panes render. Edits in progress are not visible
   /// here until applyActive().
   QString paletteText(const QString& name) const;

   /**
    * The family a bundled palette belongs to, or an empty string for palettes with no family
    * (imported ones stay editor-only until they carry field metadata). The family id is the name
    * of its primary palette, e.g. FamilyOf("SRV") == "DV".
    */
   [[nodiscard]] static QString FamilyOf(const QString& paletteName);

   /// Every palette that may stand in for `familyId`'s field, primary first. Empty for no family.
   [[nodiscard]] static QStringList FamilyMembers(const QString& familyId);

   Q_INVOKABLE QString familyOf(const QString& paletteName) const;

   /// The user-chosen default for a family, or empty when the family still uses each product's
   /// bundled default (which is the family id for most products, but e.g. KDP2 for PHI2).
   Q_INVOKABLE QString familyDefault(const QString& familyId) const;
   Q_INVOKABLE bool isFamilyDefault(const QString& paletteName) const;
   QStringList familyDefaultNames() const;

   /// Makes `paletteName` the default for `familyId`; rejected unless it is a member. An empty
   /// name clears the choice back to the bundled defaults.
   Q_INVOKABLE void setFamilyDefault(const QString& familyId, const QString& paletteName);

   /// Activates a palette immediately, bypassing the unsaved-changes confirmation flow. Not
   /// Q_INVOKABLE on purpose - QML must go through requestSelect() so dirty edits are never
   /// silently discarded. Exists for callers (tests, other C++) that already know there is nothing
   /// to lose.
   bool select(const QString& name);
   Q_INVOKABLE bool openFile(const QUrl& source);
   Q_INVOKABLE bool saveAs(const QUrl& destination);

   Q_INVOKABLE void requestClose();
   Q_INVOKABLE void requestSelect(const QString& name);
   Q_INVOKABLE void requestImport();
   Q_INVOKABLE void requestResetActive();
   Q_INVOKABLE void requestResetAll();

   /**
    * Publishes the active palette's working text as its applied text and makes it the default
    * for its family. Panes react through paletteApplied (text) and familyDefaultsChanged.
    */
   Q_INVOKABLE void applyActive();
   Q_INVOKABLE void resolveUnsavedChanges(UnsavedDecision decision);
   Q_INVOKABLE void completePendingSave(const QUrl& destination);

signals:
   void paletteNamesChanged();
   void activePaletteChanged();
   void confirmationRequiredChanged();
   void paletteTextChanged(const QString& text);
   void paletteApplied(const QString& name);
   void familyDefaultsChanged();
   void closeRequested();
   void importFileRequested();
   void saveFileRequested();

private:
   struct Entry
   {
      QString workingText;
      QString appliedText;
      QString factoryText;
      QUrl    source;
      bool    factory {false};
   };

   enum class PendingAction
   {
      None,
      Close,
      Select,
      Import,
      ResetActive,
      ResetAll
   };

   bool Activate(const QString& name);
   void BeginAction(PendingAction action, const QString& target = {});
   void ExecutePendingAction();
   void ClearPendingAction();
   QString UniqueImportedName(const QString& baseName) const;
   [[nodiscard]] QStringList KnownFamilies() const;
   void PersistFamilyDefault(const QString& familyId);

   QStringList                  names_;
   std::map<QString, Entry>     entries_;
   std::map<QString, QString>   familyDefaults_;
   settings::SettingsStore*     store_ {nullptr};
   QString                      activeName_;
   QString                      activeText_;
   PaletteModel                 editor_;
   PendingAction                pendingAction_ {PendingAction::None};
   QString                      pendingTarget_;
   bool                         confirmationRequired_ {false};
};

} // namespace palettes
} // namespace wxlens
