#pragma once

#include <map>

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

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
 * Three distinct notions live here and must not be confused
 * (docs/phase1-ux-feedback-2026-08-31.md, "Product-aware palette ownership and defaults"):
 *
 * - The **active** palette is what the editor is showing. It is editor state only: selecting a
 *   palette to look at or edit never recolours a pane.
 * - A **family** is one meteorological field's group of interchangeable ramps. Bundled palettes
 *   are their own family except for the declared variants (`SRV` belongs to `DV`, `KDP2` to
 *   `KDP`). An imported palette starts with no family and is matched to candidates by its
 *   `Units:` header - the one physical property a GRLevelX `.pal` reliably declares - then linked
 *   to one explicitly by the user.
 * - A **family default** is which palette that field's panes use when they carry no explicit
 *   override of their own. "Apply to product" both publishes the edited palette text and makes
 *   that palette its family's default.
 *
 * Family defaults persist (category `palettes`) once bindSettings() has been called; an unbound
 * manager, as in most tests, keeps them in memory only. Palette *text* edits are deliberately
 * never persisted - factory palettes are never overwritten - so a restart always brings back the
 * bundled originals, and resetFamilyDefaults() is the in-session equivalent.
 */
class PaletteManager : public QObject
{
   Q_OBJECT
   Q_PROPERTY(QStringList paletteNames READ paletteNames NOTIFY paletteNamesChanged)
   Q_PROPERTY(QString activeName READ activeName NOTIFY activePaletteChanged)
   Q_PROPERTY(bool activeIsFactoryPalette READ activeIsFactoryPalette NOTIFY activePaletteChanged)
   Q_PROPERTY(bool confirmationRequired READ confirmationRequired NOTIFY confirmationRequiredChanged)
   Q_PROPERTY(PaletteModel* editor READ editor CONSTANT)

   /// The palette each known family currently renders with (its bundled default until the user
   /// applies another member). A list, not a map, so QML can test membership with indexOf.
   Q_PROPERTY(QStringList familyDefaultNames READ familyDefaultNames NOTIFY familyDefaultsChanged)

   /// Every family id, so the editor can offer a "show only this field's palettes" filter rather
   /// than one ever-growing horizontal strip.
   Q_PROPERTY(QVariantList families READ families NOTIFY paletteNamesChanged)

   /**
    * Whether the editor's current changes have already been published by applyActive(). True
    * means panes are rendering them but nothing on disk holds them, which is a different warning
    * from "these changes have not been applied anywhere yet" - see the close confirmation.
    */
   Q_PROPERTY(bool activeDraftApplied READ activeDraftApplied NOTIFY activeDraftAppliedChanged)

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
   bool activeDraftApplied() const;
   PaletteModel* editor();
   QString activeText() const;

   /// The last *applied* text of a palette - what panes render. Edits in progress are not visible
   /// here until applyActive().
   QString paletteText(const QString& name) const;

   /**
    * The family a bundled palette belongs to, or an empty string for one with no family yet
    * (every import until it is linked). The family id is the name of its primary palette, e.g.
    * FamilyOf("SRV") == "DV".
    */
   [[nodiscard]] static QString FamilyOf(const QString& paletteName);

   /// Every palette that may stand in for `familyId`'s field, primary first. Empty for no family.
   [[nodiscard]] static QStringList FamilyMembers(const QString& familyId);

   /**
    * `Units:` as declared by a `.pal`, normalised for comparison (upper-case, and `DEG/KM` folded
    * onto `DEG` so KDP's two bundled ramps compare equal). Empty when the file declares none,
    * which is not an error - `HC` and `Default16` ship that way - but does mean units cannot
    * suggest a family for it.
    */
   [[nodiscard]] static QString CanonicalUnits(const QString& declaredUnits);

   /// Parses the `Units:` header out of raw `.pal` text, already canonicalised.
   [[nodiscard]] static QString UnitsOfText(const QString& paletteText);

   Q_INVOKABLE QString familyOf(const QString& paletteName) const;
   Q_INVOKABLE QString unitsOf(const QString& paletteName) const;

   /// Families whose field is measured in the same units as `paletteName` - the candidates a
   /// freshly imported palette can sensibly be linked to. A bundled palette answers its own
   /// family plus any other family sharing its units.
   Q_INVOKABLE QStringList compatibleFamilies(const QString& paletteName) const;

   /// Every family id known to this catalog, primary palette first.
   [[nodiscard]] QVariantList families() const;

   /**
    * The palettes worth showing while working on `familyId`: its bundled members, plus every
    * import linked to it, plus every unlinked import whose units match. An empty `familyId`
    * answers the whole catalog. This is what keeps the editor from becoming a strip of every
    * palette ever imported.
    */
   Q_INVOKABLE QStringList palettesForFamily(const QString& familyId) const;

   /// The user-chosen default for a family, or empty when the family still uses each product's
   /// bundled default (which is the family id for most products, but e.g. KDP2 for PHI2).
   Q_INVOKABLE QString familyDefault(const QString& familyId) const;
   Q_INVOKABLE bool isFamilyDefault(const QString& paletteName) const;
   QStringList familyDefaultNames() const;

   /// Makes `paletteName` the default for `familyId`; rejected unless it is a member or an import
   /// linked to it. An empty name clears the choice back to the bundled default.
   Q_INVOKABLE void setFamilyDefault(const QString& familyId, const QString& paletteName);

   /// Clears every user-chosen family default, so each field returns to its bundled palette.
   /// Wired to the Settings dialog's "Reset to defaults" as well as the editor's "Reset all",
   /// since a global reset that silently kept a palette override would be a lie.
   Q_INVOKABLE void resetFamilyDefaults();

   /**
    * Links an imported palette to a product family, so it appears while working on that field and
    * becomes eligible to be its default. Bundled palettes cannot be relinked - their family is
    * the meteorological fact that names them.
    */
   Q_INVOKABLE bool setPaletteFamily(const QString& paletteName, const QString& familyId);

   /**
    * Reads a `.pal` without importing it, for the import preview: `{valid, error, name, units,
    * unitsDeclared, stopCount, compatibleFamilies}`. Lets the user see what a file is and what it
    * can colour *before* it joins the catalog.
    */
   Q_INVOKABLE QVariantMap inspectFile(const QUrl& source) const;

   /// Activates a palette immediately, bypassing the unsaved-changes confirmation flow. Not
   /// Q_INVOKABLE on purpose - QML must go through requestSelect() so dirty edits are never
   /// silently discarded. Exists for callers (tests, other C++) that already know there is nothing
   /// to lose.
   bool select(const QString& name);

   /// Imports a `.pal` and, when `familyId` is non-empty, links it to that family in one step.
   Q_INVOKABLE bool openFile(const QUrl& source, const QString& familyId = {});
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
   void activeDraftAppliedChanged();
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
      QString units;   ///< canonicalised `Units:`; empty when the file declares none
      QString family;  ///< bundled: its own family; import: empty until the user links it
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
   void NotifyDraftAppliedChanged();

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
   bool                         draftApplied_ {false};
};

} // namespace palettes
} // namespace wxlens
