#include <nimbus/palettes/palette_manager.hpp>
#include <nimbus/log/logger.hpp>

#include <scwx/common/color_table.hpp>

#include <sstream>
#include <QFile>
#include <QFileInfo>

namespace nimbus
{
namespace palettes
{

namespace { const auto logger_ = nimbus::log::Create("palettes.palette_manager"); }

PaletteManager::PaletteManager(QObject* parent) : QObject(parent), editor_(this)
{
   connect(&editor_, &PaletteModel::contentChanged, this,
           [this](const QString& text)
           {
              activeText_ = text;
              Q_EMIT paletteTextChanged(activeText_);
           });
   QFile file(":/qt/qml/Nimbus/App/res/palettes/wct/DR.pal");
   if (file.open(QIODevice::ReadOnly)) Activate("DR", QString::fromUtf8(file.readAll()));
}

PaletteManager& PaletteManager::Instance()
{
   static PaletteManager instance;
   return instance;
}

QStringList PaletteManager::paletteNames() const { return names_; }
QString PaletteManager::activeName() const { return activeName_; }
PaletteModel* PaletteManager::editor() { return &editor_; }
QString PaletteManager::activeText() const { return activeText_; }

bool PaletteManager::Activate(const QString& name, const QString& text)
{
   std::istringstream stream(text.toStdString());
   auto table = scwx::common::ColorTable::Load(stream);
   if (table == nullptr || !table->IsValid())
   {
      logger_->warn("Rejected invalid palette {}", name.toStdString());
      return false;
   }
   activeName_ = name;
   activeText_ = text;
   editor_.load(name, text);
   Q_EMIT activePaletteChanged();
   Q_EMIT paletteTextChanged(activeText_);
   logger_->info("Activated palette {}", name.toStdString());
   return true;
}

bool PaletteManager::select(const QString& name)
{
   if (!names_.contains(name)) return false;
   QFile file(":/qt/qml/Nimbus/App/res/palettes/wct/" + name + ".pal");
   return file.open(QIODevice::ReadOnly) && Activate(name, QString::fromUtf8(file.readAll()));
}

bool PaletteManager::openFile(const QUrl& source)
{
   QFile file(source.toLocalFile());
   if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
   const QString name = QFileInfo(file).completeBaseName();
   if (!names_.contains(name))
   {
      names_.append(name);
      Q_EMIT paletteNamesChanged();
   }
   return Activate(name, QString::fromUtf8(file.readAll()));
}

bool PaletteManager::activeIsFactoryPalette() const
{
   return names_.mid(0, 15).contains(activeName_);
}

bool PaletteManager::resetActiveToFactory()
{
   if (!activeIsFactoryPalette()) return false;
   logger_->info("Resetting palette {} to factory colors", activeName_.toStdString());
   return select(activeName_);
}

bool PaletteManager::resetAllToFactory()
{
   // Factory palettes are immutable Qt resources. Runtime edits never modify them, so resetting
   // all means discarding the current working copy and returning to the factory reflectivity
   // palette. Imported/saved files are deliberately outside this operation and remain untouched.
   logger_->info("Resetting all factory palettes");
   return select("DR");
}

} // namespace palettes
} // namespace nimbus
