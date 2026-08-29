#include <wxlens/settings/settings_store.hpp>
#include <wxlens/log/logger.hpp>

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>

#include <toml++/toml.hpp>

namespace wxlens
{
namespace settings
{

static const std::string logPrefix_ = "settings.settings_store";
static const auto        logger_    = wxlens::log::Create(logPrefix_);

namespace
{

/// A category's in-memory state. `failedToParse_` is deliberately sticky for the session: once we
/// know a file is malformed we must keep refusing to overwrite it, not just for the first read.
struct Category
{
   toml::table table {};
   bool        loaded {false};
   bool        dirty {false};
   bool        failedToParse {false};
};

QString DefaultConfigDirectory()
{
   // AppConfigLocation, per §3.2 - a portable directory a user can open, edit, copy between
   // machines, or (Phase 5) host per-user on a server. None of which the Windows registry allows.
   return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
}

} // namespace

class SettingsStore::Impl
{
public:
   Category& Load(const QString& category);

   [[nodiscard]] QString PathFor(const QString& category) const
   {
      return QDir(configDirectory_).filePath(category + QStringLiteral(".toml"));
   }

   /// Reports a rejected value once per read rather than silently substituting the default. A
   /// hand-edited file is an expected input, so "why did my change do nothing?" needs an answer
   /// in the log.
   static void LogRejected(const QString& category,
                           const QString& key,
                           const QString& reason)
   {
      logger_->warn("Ignoring {}.{}: {} - using default",
                    category.toStdString(),
                    key.toStdString(),
                    reason.toStdString());
   }

   mutable std::mutex              mutex_;
   QString                         configDirectory_ {DefaultConfigDirectory()};
   std::map<QString, Category>     categories_ {};
};

Category& SettingsStore::Impl::Load(const QString& category)
{
   Category& entry = categories_[category];
   if (entry.loaded)
   {
      return entry;
   }

   entry.loaded = true;

   const QString path = PathFor(category);
   QFile         file {path};
   if (!file.exists())
   {
      // Not an error: a fresh install has no config yet, and every read simply gets its default.
      return entry;
   }

   if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
   {
      entry.failedToParse = true;
      logger_->error("Cannot read {} - settings in it fall back to defaults",
                     path.toStdString());
      return entry;
   }

   const QByteArray contents = file.readAll();
   file.close();

   try
   {
      entry.table = toml::parse(std::string {contents.constData(),
                                             static_cast<std::size_t>(contents.size())});
   }
   catch (const toml::parse_error& error)
   {
      // Left on disk untouched, and Save() will refuse to overwrite it. Destroying a config the
      // user hand-wrote is a worse outcome than ignoring it for this session.
      entry.failedToParse = true;
      entry.table         = toml::table {};
      logger_->error("{} is not valid TOML ({}) - its settings fall back to defaults, and it will "
                     "not be overwritten",
                     path.toStdString(),
                     error.description());
   }

   return entry;
}

SettingsStore::SettingsStore() : p {std::make_unique<Impl>()} {}

SettingsStore::~SettingsStore() = default;

SettingsStore& SettingsStore::Instance()
{
   static SettingsStore instance;
   return instance;
}

void SettingsStore::SetConfigDirectory(const QString& path)
{
   std::scoped_lock lock {p->mutex_};
   p->configDirectory_ = path;
   p->categories_.clear();
}

QString SettingsStore::ConfigDirectory() const
{
   std::scoped_lock lock {p->mutex_};
   return p->configDirectory_;
}

QString SettingsStore::FilePath(const QString& category) const
{
   std::scoped_lock lock {p->mutex_};
   return p->PathFor(category);
}

bool SettingsStore::CategoryFailedToParse(const QString& category)
{
   std::scoped_lock lock {p->mutex_};
   return p->Load(category).failedToParse;
}

bool SettingsStore::GetBool(const QString& category, const QString& key, bool defaultValue)
{
   std::scoped_lock lock {p->mutex_};
   const Category&  entry = p->Load(category);

   const auto* node = entry.table.get(key.toStdString());
   if (node == nullptr)
   {
      return defaultValue;
   }
   if (const auto value = node->value<bool>())
   {
      return *value;
   }

   Impl::LogRejected(category, key, QStringLiteral("not a boolean"));
   return defaultValue;
}

int SettingsStore::GetInt(
   const QString& category, const QString& key, int defaultValue, int minimum, int maximum)
{
   std::scoped_lock lock {p->mutex_};
   const Category&  entry = p->Load(category);

   const auto* node = entry.table.get(key.toStdString());
   if (node == nullptr)
   {
      return defaultValue;
   }

   const auto value = node->value<std::int64_t>();
   if (!value.has_value())
   {
      Impl::LogRejected(category, key, QStringLiteral("not an integer"));
      return defaultValue;
   }

   // Out of range falls back rather than clamping. Clamping would turn a typo into a different
   // valid-looking setting the user never chose; falling back is at least predictable, and the
   // log line says what happened.
   if (*value < minimum || *value > maximum)
   {
      Impl::LogRejected(
         category,
         key,
         QStringLiteral("%1 is outside %2..%3").arg(*value).arg(minimum).arg(maximum));
      return defaultValue;
   }

   return static_cast<int>(*value);
}

double SettingsStore::GetDouble(
   const QString& category, const QString& key, double defaultValue, double minimum, double maximum)
{
   std::scoped_lock lock {p->mutex_};
   const Category&  entry = p->Load(category);

   const auto* node = entry.table.get(key.toStdString());
   if (node == nullptr)
   {
      return defaultValue;
   }

   // An integer in the file is a perfectly reasonable way to write a double by hand ("radius = 50"
   // rather than "50.0"), so accept both rather than rejecting the tidier spelling.
   std::optional<double> value = node->value<double>();
   if (!value.has_value())
   {
      if (const auto integral = node->value<std::int64_t>())
      {
         value = static_cast<double>(*integral);
      }
   }

   if (!value.has_value())
   {
      Impl::LogRejected(category, key, QStringLiteral("not a number"));
      return defaultValue;
   }

   if (*value < minimum || *value > maximum)
   {
      Impl::LogRejected(
         category,
         key,
         QStringLiteral("%1 is outside %2..%3").arg(*value).arg(minimum).arg(maximum));
      return defaultValue;
   }

   return *value;
}

QString
SettingsStore::GetString(const QString& category, const QString& key, const QString& defaultValue)
{
   std::scoped_lock lock {p->mutex_};
   const Category&  entry = p->Load(category);

   const auto* node = entry.table.get(key.toStdString());
   if (node == nullptr)
   {
      return defaultValue;
   }
   if (const auto value = node->value<std::string>())
   {
      return QString::fromStdString(*value);
   }

   Impl::LogRejected(category, key, QStringLiteral("not a string"));
   return defaultValue;
}

void SettingsStore::SetBool(const QString& category, const QString& key, bool value)
{
   std::scoped_lock lock {p->mutex_};
   Category&        entry = p->Load(category);
   entry.table.insert_or_assign(key.toStdString(), value);
   entry.dirty = true;
}

void SettingsStore::SetInt(const QString& category, const QString& key, int value)
{
   std::scoped_lock lock {p->mutex_};
   Category&        entry = p->Load(category);
   entry.table.insert_or_assign(key.toStdString(), static_cast<std::int64_t>(value));
   entry.dirty = true;
}

void SettingsStore::SetDouble(const QString& category, const QString& key, double value)
{
   std::scoped_lock lock {p->mutex_};
   Category&        entry = p->Load(category);
   entry.table.insert_or_assign(key.toStdString(), value);
   entry.dirty = true;
}

void SettingsStore::SetString(const QString& category, const QString& key, const QString& value)
{
   std::scoped_lock lock {p->mutex_};
   Category&        entry = p->Load(category);
   entry.table.insert_or_assign(key.toStdString(), value.toStdString());
   entry.dirty = true;
}

bool SettingsStore::Save()
{
   std::scoped_lock lock {p->mutex_};

   if (!QDir().mkpath(p->configDirectory_))
   {
      logger_->error("Cannot create config directory {}", p->configDirectory_.toStdString());
      return false;
   }

   bool allSaved = true;

   for (auto& [category, entry] : p->categories_)
   {
      if (!entry.dirty)
      {
         continue;
      }

      if (entry.failedToParse)
      {
         // See the class comment: we do not know what else is in that file, so writing our view of
         // it would discard whatever the user was in the middle of hand-editing.
         logger_->warn("Not overwriting {} - it failed to parse this session",
                       p->PathFor(category).toStdString());
         allSaved = false;
         continue;
      }

      // QSaveFile writes to a temporary and renames on commit, so an interrupted write cannot
      // leave a half-written config behind - the failure mode being guarded against is a config
      // file that exists but is truncated, which would then fail to parse on next launch.
      QSaveFile file {p->PathFor(category)};
      if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
      {
         logger_->error("Cannot write {}", p->PathFor(category).toStdString());
         allSaved = false;
         continue;
      }

      std::ostringstream out;
      out << "# WxLens settings - " << category.toStdString() << "\n"
          << "# Hand-editing is supported. Unknown keys are left alone; a value that is the wrong\n"
          << "# type or outside its valid range is ignored (see logs) and the default is used.\n\n"
          << entry.table << "\n";

      const std::string text = out.str();
      if (file.write(text.data(), static_cast<qint64>(text.size())) < 0 || !file.commit())
      {
         logger_->error("Failed writing {}", p->PathFor(category).toStdString());
         allSaved = false;
         continue;
      }

      entry.dirty = false;
   }

   return allSaved;
}

void SettingsStore::Reload()
{
   std::scoped_lock lock {p->mutex_};
   p->categories_.clear();
}

} // namespace settings
} // namespace wxlens
