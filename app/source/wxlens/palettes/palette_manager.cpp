#include <wxlens/palettes/palette_manager.hpp>
#include <wxlens/log/logger.hpp>
#include <wxlens/settings/settings_store.hpp>

#include <scwx/common/color_table.hpp>

#include <sstream>

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace wxlens
{
namespace palettes
{

namespace
{
const auto logger_ = wxlens::log::Create("palettes.palette_manager");
const QStringList kFactoryNames {"DR", "DV", "SRV", "SW", "ZDR", "CC", "KDP", "KDP2",
                                 "HC", "ET", "VIL", "OHP", "STP", "DOD_DSD", "Default16"};
const QString kPalettesCategory = QStringLiteral("palettes");
const QString kFamilyDefaultKeyPrefix = QStringLiteral("family_default_");

/// Display names for the field each family colours. Only used for UI text; the family id remains
/// the primary palette's name so nothing depends on these strings.
QString FamilyLabel(const QString& familyId)
{
   static const std::map<QString, QString> kLabels {
      {QStringLiteral("DR"), QStringLiteral("Reflectivity")},
      {QStringLiteral("DV"), QStringLiteral("Velocity")},
      {QStringLiteral("SW"), QStringLiteral("Spectrum width")},
      {QStringLiteral("ZDR"), QStringLiteral("Differential reflectivity")},
      {QStringLiteral("CC"), QStringLiteral("Correlation coefficient")},
      {QStringLiteral("KDP"), QStringLiteral("Specific differential phase")},
      {QStringLiteral("HC"), QStringLiteral("Hydrometeor classification")},
      {QStringLiteral("ET"), QStringLiteral("Echo tops")},
      {QStringLiteral("VIL"), QStringLiteral("Vertically integrated liquid")},
      {QStringLiteral("OHP"), QStringLiteral("One-hour precipitation")},
      {QStringLiteral("STP"), QStringLiteral("Storm-total precipitation")},
      {QStringLiteral("DOD_DSD"), QStringLiteral("Precipitation difference")},
      {QStringLiteral("Default16"), QStringLiteral("Generic 16-colour")}};
   const auto it = kLabels.find(familyId);
   return it == kLabels.end() ? familyId : it->second;
}
} // namespace

PaletteManager::PaletteManager(QObject* parent) : QObject(parent), editor_(this)
{
   for (const QString& name : kFactoryNames)
   {
      QFile file(":/qt/qml/WxLens/App/res/palettes/wct/" + name + ".pal");
      if (!file.open(QIODevice::ReadOnly)) continue;
      const QString text = QString::fromUtf8(file.readAll());
      names_.append(name);
      entries_.emplace(name,
                       Entry {text, text, text, {}, true, UnitsOfText(text), FamilyOf(name)});
   }

   connect(&editor_,
           &PaletteModel::contentChanged,
           this,
           [this](const QString& text)
           {
              activeText_ = text;
              if (auto it = entries_.find(activeName_); it != entries_.end())
                 it->second.workingText = text;
              NotifyDraftAppliedChanged();
           });

   if (entries_.contains("DR")) Activate("DR");
}

PaletteManager& PaletteManager::Instance()
{
   static PaletteManager instance;
   return instance;
}

QString PaletteManager::FamilyOf(const QString& paletteName)
{
   // Presentation variants of one meteorological field. Palettes describe a field, not just a
   // visual style, so this is the only place a palette may stand in for another - reflectivity
   // can never be made to look like velocity (docs/ROADMAP.md §4.5 palette-ownership rules).
   if (paletteName == QStringLiteral("SRV")) return QStringLiteral("DV");
   if (paletteName == QStringLiteral("KDP2")) return QStringLiteral("KDP");
   if (kFactoryNames.contains(paletteName)) return paletteName;
   return {};
}

QStringList PaletteManager::FamilyMembers(const QString& familyId)
{
   if (familyId == QStringLiteral("DV")) return {QStringLiteral("DV"), QStringLiteral("SRV")};
   if (familyId == QStringLiteral("KDP")) return {QStringLiteral("KDP"), QStringLiteral("KDP2")};
   if (familyId.isEmpty() || FamilyOf(familyId) != familyId) return {};
   return {familyId};
}

QString PaletteManager::CanonicalUnits(const QString& declaredUnits)
{
   QString units = declaredUnits.trimmed().toUpper();
   if (units.isEmpty()) return {};
   if (units == QStringLiteral("KTS") || units == QStringLiteral("KNOTS"))
      return QStringLiteral("KT");
   return units;
}

QString PaletteManager::UnitsQuantity(const QString& canonicalUnits)
{
   const QString units = CanonicalUnits(canonicalUnits);
   if (units.isEmpty()) return {};
   // Speed: WxLens's own DV ramp is MPH, the WCT ramps are KT, and Level 2 decodes to m/s. All
   // three describe the same field, and BuildColorTableLut already rescales to whatever the
   // chosen table declares, so they are interchangeable for the user's purposes.
   if (units == QStringLiteral("KT") || units == QStringLiteral("MPH") ||
       units == QStringLiteral("M/S") || units == QStringLiteral("MS") ||
       units == QStringLiteral("MPS") || units == QStringLiteral("KM/H") ||
       units == QStringLiteral("KPH"))
      return QStringLiteral("SPEED");
   // KDP declares DEG/KM and KDP2 declares DEG for the one field.
   if (units == QStringLiteral("DEG") || units == QStringLiteral("DEG/KM"))
      return QStringLiteral("ANGLE_RATE");
   if (units == QStringLiteral("DBZ")) return QStringLiteral("REFLECTIVITY");
   if (units == QStringLiteral("DB")) return QStringLiteral("DIFFERENTIAL");
   if (units == QStringLiteral("IN") || units == QStringLiteral("MM") ||
       units == QStringLiteral("CM"))
      return QStringLiteral("DEPTH");
   if (units == QStringLiteral("KFT") || units == QStringLiteral("FT") ||
       units == QStringLiteral("KM") || units == QStringLiteral("M"))
      return QStringLiteral("HEIGHT");
   return units;
}

QString PaletteManager::UnitsOfText(const QString& paletteText)
{
   // Deliberately parsed here rather than read off scwx::common::ColorTable: that class keeps its
   // Units: value private and external/ is read-only (docs/adr/0002). One line, same tokenisation
   // rule as the vendored parser (first whitespace-separated token after the directive).
   const QStringList lines = paletteText.split(QLatin1Char('\n'));
   for (const QString& line : lines)
   {
      const QString trimmed = line.trimmed();
      if (!trimmed.startsWith(QStringLiteral("Units:"), Qt::CaseInsensitive)) continue;
      const QString value = trimmed.mid(QStringLiteral("Units:").size()).trimmed();
      const QStringList tokens = value.split(QRegularExpression(QStringLiteral("\\s+")),
                                             Qt::SkipEmptyParts);
      return tokens.isEmpty() ? QString {} : CanonicalUnits(tokens.first());
   }
   return {};
}

QString PaletteManager::familyOf(const QString& paletteName) const
{
   const auto it = entries_.find(paletteName);
   return it == entries_.end() ? QString {} : it->second.family;
}

QString PaletteManager::unitsOf(const QString& paletteName) const
{
   const auto it = entries_.find(paletteName);
   return it == entries_.end() ? QString {} : it->second.units;
}

QStringList PaletteManager::compatibleFamilies(const QString& paletteName) const
{
   const auto it = entries_.find(paletteName);
   if (it == entries_.end()) return {};
   QStringList result;
   if (!it->second.family.isEmpty()) result.append(it->second.family);
   if (it->second.units.isEmpty()) return result;
   const QString quantity = UnitsQuantity(it->second.units);
   for (const QString& family : KnownFamilies())
   {
      if (result.contains(family)) continue;
      const auto primary = entries_.find(family);
      if (primary != entries_.end() && UnitsQuantity(primary->second.units) == quantity)
         result.append(family);
   }
   return result;
}

QVariantList PaletteManager::families() const
{
   QVariantList result;
   for (const QString& family : KnownFamilies())
   {
      const auto primary = entries_.find(family);
      result.append(QVariantMap {
         {QStringLiteral("id"), family},
         {QStringLiteral("label"), FamilyLabel(family)},
         {QStringLiteral("units"), primary == entries_.end() ? QString {} : primary->second.units},
         {QStringLiteral("defaultName"),
          familyDefault(family).isEmpty() ? family : familyDefault(family)},
         {QStringLiteral("usesBundledDefault"), familyDefault(family).isEmpty()}});
   }
   return result;
}

QStringList PaletteManager::palettesForFamily(const QString& familyId) const
{
   if (familyId.isEmpty()) return names_;
   const QStringList members = FamilyMembers(familyId);
   if (members.isEmpty()) return names_;
   const auto primary = entries_.find(familyId);
   const QString familyUnits = primary == entries_.end() ? QString {} : primary->second.units;

   QStringList result;
   for (const QString& name : names_)
   {
      const auto it = entries_.find(name);
      if (it == entries_.end()) continue;
      if (members.contains(name))
      {
         result.append(name);
         continue;
      }
      if (it->second.factory) continue; // another field's bundled ramp: never relevant here
      if (it->second.family == familyId)
      {
         result.append(name); // an import the user linked to this field
         continue;
      }
      // An unlinked import measuring this field's quantity is worth offering - that is exactly
      // the "does test01 work with velocity?" question the import flow answers.
      if (it->second.family.isEmpty() && !familyUnits.isEmpty() &&
          UnitsQuantity(it->second.units) == UnitsQuantity(familyUnits))
         result.append(name);
   }
   return result;
}

QStringList PaletteManager::KnownFamilies() const
{
   QStringList families;
   for (const QString& name : names_)
   {
      const QString family = FamilyOf(name);
      if (!family.isEmpty() && !families.contains(family)) families.append(family);
   }
   return families;
}

void PaletteManager::bindSettings(settings::SettingsStore& store)
{
   store_ = &store;
   familyDefaults_.clear();
   for (const QString& family : KnownFamilies())
   {
      const QString stored =
         store_->GetString(kPalettesCategory, kFamilyDefaultKeyPrefix + family, QString {});
      if (stored.isEmpty()) continue;
      if (!FamilyMembers(family).contains(stored) || !entries_.contains(stored))
      {
         logger_->warn("Ignoring {} default \"{}\": not a member of that palette family",
                       family.toStdString(), stored.toStdString());
         continue;
      }
      familyDefaults_[family] = stored;
   }
   Q_EMIT familyDefaultsChanged();
}

void PaletteManager::PersistFamilyDefault(const QString& familyId)
{
   if (store_ == nullptr) return;
   const auto it = familyDefaults_.find(familyId);
   store_->SetString(kPalettesCategory,
                     kFamilyDefaultKeyPrefix + familyId,
                     it == familyDefaults_.end() ? QString {} : it->second);
   if (!store_->Save())
      logger_->error("Could not persist the {} palette default", familyId.toStdString());
}

QString PaletteManager::familyDefault(const QString& familyId) const
{
   const auto it = familyDefaults_.find(familyId);
   return it == familyDefaults_.end() ? QString {} : it->second;
}

bool PaletteManager::isFamilyDefault(const QString& paletteName) const
{
   const QString family = familyOf(paletteName);
   if (family.isEmpty()) return false;
   const QString chosen = familyDefault(family);
   return chosen.isEmpty() ? paletteName == family : chosen == paletteName;
}

QStringList PaletteManager::familyDefaultNames() const
{
   QStringList result;
   for (const QString& family : KnownFamilies())
   {
      const QString chosen = familyDefault(family);
      result.append(chosen.isEmpty() ? family : chosen);
   }
   return result;
}

void PaletteManager::setFamilyDefault(const QString& familyId, const QString& paletteName)
{
   if (FamilyMembers(familyId).isEmpty()) return;
   if (!paletteName.isEmpty())
   {
      const auto it = entries_.find(paletteName);
      if (it == entries_.end()) return;
      const bool member = FamilyMembers(familyId).contains(paletteName);
      const bool linkedImport = !it->second.factory && it->second.family == familyId;
      if (!member && !linkedImport) return;
   }
   if (familyDefault(familyId) == paletteName) return;
   if (paletteName.isEmpty())
      familyDefaults_.erase(familyId);
   else
      familyDefaults_[familyId] = paletteName;
   PersistFamilyDefault(familyId);
   Q_EMIT familyDefaultsChanged();
}

void PaletteManager::resetFamilyDefaults()
{
   if (familyDefaults_.empty()) return;
   const QStringList families = KnownFamilies();
   familyDefaults_.clear();
   for (const QString& family : families) PersistFamilyDefault(family);
   Q_EMIT familyDefaultsChanged();
}

bool PaletteManager::setPaletteFamily(const QString& paletteName, const QString& familyId)
{
   auto it = entries_.find(paletteName);
   if (it == entries_.end() || it->second.factory) return false;
   if (!familyId.isEmpty() && FamilyMembers(familyId).isEmpty()) return false;
   if (it->second.family == familyId) return true;

   // Dropping a link must not leave the palette as a family default it no longer belongs to.
   if (const QString previous = it->second.family;
       !previous.isEmpty() && familyDefault(previous) == paletteName)
   {
      familyDefaults_.erase(previous);
      PersistFamilyDefault(previous);
   }
   it->second.family = familyId;
   Q_EMIT paletteNamesChanged();
   Q_EMIT familyDefaultsChanged();
   return true;
}

QVariantMap PaletteManager::inspectFile(const QUrl& source) const
{
   QVariantMap result {{QStringLiteral("valid"), false},
                       {QStringLiteral("error"), QString {}},
                       {QStringLiteral("name"), QString {}},
                       {QStringLiteral("units"), QString {}},
                       {QStringLiteral("stopCount"), 0},
                       {QStringLiteral("compatibleFamilies"), QVariantList {}}};

   const QString path = source.isLocalFile() ? source.toLocalFile() : source.toString();
   QFile file(path);
   if (!file.open(QIODevice::ReadOnly))
   {
      result[QStringLiteral("error")] = QStringLiteral("Could not open %1").arg(path);
      return result;
   }
   const QString text = QString::fromUtf8(file.readAll());
   result[QStringLiteral("name")] = QFileInfo(file).completeBaseName();

   std::istringstream stream(text.toStdString());
   auto table = scwx::common::ColorTable::Load(stream);
   if (table == nullptr || !table->IsValid())
   {
      result[QStringLiteral("error")] =
         QStringLiteral("Not a readable GRLevelX palette - no valid colour stops found");
      return result;
   }

   const QString units = UnitsOfText(text);
   int stopCount = 0;
   for (const QString& line : text.split(QLatin1Char('\n')))
      if (line.trimmed().startsWith(QStringLiteral("Color"), Qt::CaseInsensitive)) ++stopCount;

   QVariantList candidates;
   const QString quantity = UnitsQuantity(units);
   for (const QString& family : KnownFamilies())
   {
      const auto primary = entries_.find(family);
      if (primary == entries_.end()) continue;
      if (quantity.isEmpty() || UnitsQuantity(primary->second.units) != quantity) continue;
      candidates.append(QVariantMap {{QStringLiteral("id"), family},
                                     {QStringLiteral("label"), FamilyLabel(family)}});
   }

   result[QStringLiteral("valid")]              = true;
   result[QStringLiteral("units")]              = units;
   result[QStringLiteral("stopCount")]          = stopCount;
   result[QStringLiteral("compatibleFamilies")] = candidates;
   return result;
}

bool PaletteManager::activeDraftApplied() const { return draftApplied_; }

void PaletteManager::NotifyDraftAppliedChanged()
{
   const auto it = entries_.find(activeName_);
   const bool applied = it != entries_.end() && editor_.dirty() &&
                        it->second.workingText == it->second.appliedText;
   if (applied == draftApplied_) return;
   draftApplied_ = applied;
   Q_EMIT activeDraftAppliedChanged();
}

QStringList PaletteManager::paletteNames() const { return names_; }
QString PaletteManager::activeName() const { return activeName_; }
bool PaletteManager::confirmationRequired() const { return confirmationRequired_; }
PaletteModel* PaletteManager::editor() { return &editor_; }
QString PaletteManager::activeText() const { return activeText_; }
QString PaletteManager::paletteText(const QString& name) const
{
   const auto it = entries_.find(name);
   return it == entries_.end() ? QString {} : it->second.appliedText;
}

bool PaletteManager::activeIsFactoryPalette() const
{
   auto it = entries_.find(activeName_);
   return it != entries_.end() && it->second.factory;
}

bool PaletteManager::Activate(const QString& name)
{
   const auto it = entries_.find(name);
   if (it == entries_.end()) return false;
   std::istringstream stream(it->second.workingText.toStdString());
   auto table = scwx::common::ColorTable::Load(stream);
   if (table == nullptr || !table->IsValid()) return false;

   activeName_ = name;
   activeText_ = it->second.workingText;
   editor_.load(name, activeText_);
   NotifyDraftAppliedChanged();
   Q_EMIT activePaletteChanged();
   Q_EMIT paletteTextChanged(activeText_);
   logger_->info("Activated palette {}", name.toStdString());
   return true;
}

bool PaletteManager::select(const QString& name) { return Activate(name); }

QString PaletteManager::UniqueImportedName(const QString& baseName) const
{
   QString candidate = baseName.isEmpty() ? QStringLiteral("Imported palette") : baseName;
   if (!entries_.contains(candidate)) return candidate;
   for (int suffix = 2;; ++suffix)
   {
      const QString suffixed = QString("%1 (%2)").arg(candidate).arg(suffix);
      if (!entries_.contains(suffixed)) return suffixed;
   }
}

bool PaletteManager::openFile(const QUrl& source, const QString& familyId)
{
   QFile file(source.isLocalFile() ? source.toLocalFile() : source.toString());
   if (!file.open(QIODevice::ReadOnly)) return false;
   const QString text = QString::fromUtf8(file.readAll());
   std::istringstream stream(text.toStdString());
   auto table = scwx::common::ColorTable::Load(stream);
   if (table == nullptr || !table->IsValid()) return false;

   const QString name = UniqueImportedName(QFileInfo(file).completeBaseName());
   const QString family = FamilyMembers(familyId).isEmpty() ? QString {} : familyId;
   names_.append(name);
   entries_.emplace(name, Entry {text, text, {}, source, false, UnitsOfText(text), family});
   Q_EMIT paletteNamesChanged();
   if (!family.isEmpty()) Q_EMIT familyDefaultsChanged();
   return Activate(name);
}

bool PaletteManager::saveAs(const QUrl& destination)
{
   return editor_.saveAs(destination);
}

void PaletteManager::BeginAction(PendingAction action, const QString& target)
{
   // Once a confirmation is visible its action is immutable. This also makes an overlay click
   // that leaks through harmless instead of silently changing what Discard will do.
   if (confirmationRequired_) return;
   pendingAction_ = action;
   pendingTarget_ = target;
   if (editor_.dirty())
   {
      confirmationRequired_ = true;
      Q_EMIT confirmationRequiredChanged();
      return;
   }
   ExecutePendingAction();
}

void PaletteManager::requestClose() { BeginAction(PendingAction::Close); }
void PaletteManager::requestSelect(const QString& name) { BeginAction(PendingAction::Select, name); }
void PaletteManager::requestImport() { BeginAction(PendingAction::Import); }
void PaletteManager::requestResetActive() { BeginAction(PendingAction::ResetActive); }
void PaletteManager::requestResetAll() { BeginAction(PendingAction::ResetAll); }

void PaletteManager::applyActive()
{
   auto it = entries_.find(activeName_);
   if (it == entries_.end()) return;
   it->second.appliedText = it->second.workingText;

   // Applying is the one gesture that changes which palette a product family uses. Order matters:
   // the text is published first so a pane re-resolving on familyDefaultsChanged already sees it.
   bool familyChanged = false;
   const QString family = it->second.family;
   if (!family.isEmpty() && familyDefault(family) != activeName_ &&
       (FamilyMembers(family).contains(activeName_) || !it->second.factory))
   {
      familyDefaults_[family] = activeName_;
      PersistFamilyDefault(family);
      familyChanged = true;
   }

   NotifyDraftAppliedChanged();
   Q_EMIT paletteTextChanged(it->second.appliedText);
   Q_EMIT paletteApplied(activeName_);
   if (familyChanged) Q_EMIT familyDefaultsChanged();
}

void PaletteManager::resolveUnsavedChanges(UnsavedDecision decision)
{
   if (!confirmationRequired_) return;
   if (decision == UnsavedDecision::SaveCopy)
   {
      Q_EMIT saveFileRequested();
      return;
   }
   if (decision == UnsavedDecision::KeepEditing)
   {
      ClearPendingAction();
      return;
   }
   editor_.revertChanges();
   ExecutePendingAction();
}

void PaletteManager::completePendingSave(const QUrl& destination)
{
   if (!confirmationRequired_ || !editor_.saveAs(destination)) return;
   ExecutePendingAction();
}

void PaletteManager::ExecutePendingAction()
{
   const PendingAction action = pendingAction_;
   const QString target = pendingTarget_;
   ClearPendingAction();
   switch (action)
   {
   case PendingAction::Close: Q_EMIT closeRequested(); break;
   case PendingAction::Select: Activate(target); break;
   case PendingAction::Import: Q_EMIT importFileRequested(); break;
   case PendingAction::ResetActive:
   {
      // Resets the *draft* only - like any other edit it reaches panes when applied.
      auto it = entries_.find(activeName_);
      if (it != entries_.end() && it->second.factory)
      {
         it->second.workingText = it->second.factoryText;
         Activate(activeName_);
      }
      break;
   }
   case PendingAction::ResetAll:
   {
      // Unlike ResetActive this is the "get me back to how it shipped" action, so it restores
      // what panes render (applied text and family defaults) as well as the drafts. Leaving the
      // applied palettes edited while telling the user everything was restored is the confusing
      // half-measure this replaces.
      for (auto& [name, entry] : entries_)
      {
         if (!entry.factory) continue;
         entry.workingText = entry.factoryText;
         entry.appliedText = entry.factoryText;
      }
      const QStringList families = KnownFamilies();
      familyDefaults_.clear();
      for (const QString& family : families) PersistFamilyDefault(family);
      // Stay on whatever the user was looking at - its text was just reset in place above, so
      // re-activating it (rather than always jumping to "DR") picks that reset up without moving
      // the editor to a palette the user never asked to see.
      Activate(activeName_);
      Q_EMIT familyDefaultsChanged();
      break;
   }
   case PendingAction::None: break;
   }
}

void PaletteManager::ClearPendingAction()
{
   pendingAction_ = PendingAction::None;
   pendingTarget_.clear();
   if (confirmationRequired_)
   {
      confirmationRequired_ = false;
      Q_EMIT confirmationRequiredChanged();
   }
}

} // namespace palettes
} // namespace wxlens
