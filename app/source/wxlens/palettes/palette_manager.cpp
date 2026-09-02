#include <wxlens/palettes/palette_manager.hpp>
#include <wxlens/log/logger.hpp>

#include <scwx/common/color_table.hpp>

#include <sstream>

#include <QFile>
#include <QFileInfo>

namespace wxlens
{
namespace palettes
{

namespace
{
const auto logger_ = wxlens::log::Create("palettes.palette_manager");
const QStringList kFactoryNames {"DR", "DV", "SRV", "SW", "ZDR", "CC", "KDP", "KDP2",
                                 "HC", "ET", "VIL", "OHP", "STP", "DOD_DSD", "Default16"};
}

PaletteManager::PaletteManager(QObject* parent) : QObject(parent), editor_(this)
{
   for (const QString& name : kFactoryNames)
   {
      QFile file(":/qt/qml/WxLens/App/res/palettes/wct/" + name + ".pal");
      if (!file.open(QIODevice::ReadOnly)) continue;
      const QString text = QString::fromUtf8(file.readAll());
      names_.append(name);
      entries_.emplace(name, Entry {text, text, text, {}, true});
   }

   connect(&editor_,
           &PaletteModel::contentChanged,
           this,
           [this](const QString& text)
           {
              activeText_ = text;
              if (auto it = entries_.find(activeName_); it != entries_.end())
                 it->second.workingText = text;
           });

   if (entries_.contains("DR")) Activate("DR");
}

PaletteManager& PaletteManager::Instance()
{
   static PaletteManager instance;
   return instance;
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

bool PaletteManager::openFile(const QUrl& source)
{
   QFile file(source.toLocalFile());
   if (!file.open(QIODevice::ReadOnly)) return false;
   const QString text = QString::fromUtf8(file.readAll());
   std::istringstream stream(text.toStdString());
   auto table = scwx::common::ColorTable::Load(stream);
   if (table == nullptr || !table->IsValid()) return false;

   const QString name = UniqueImportedName(QFileInfo(file).completeBaseName());
   names_.append(name);
   entries_.emplace(name, Entry {text, text, {}, source, false});
   Q_EMIT paletteNamesChanged();
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
   Q_EMIT paletteTextChanged(it->second.appliedText);
   Q_EMIT paletteApplied(activeName_);
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
      auto it = entries_.find(activeName_);
      if (it != entries_.end() && it->second.factory)
      {
         it->second.workingText = it->second.factoryText;
         Activate(activeName_);
      }
      break;
   }
   case PendingAction::ResetAll:
      for (auto& [name, entry] : entries_)
         if (entry.factory) entry.workingText = entry.factoryText;
      // Stay on whatever the user was looking at - its text was just reset in place above, so
      // re-activating it (rather than always jumping to "DR") picks that reset up without moving
      // the editor to a palette the user never asked to see.
      Activate(activeName_);
      break;
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
