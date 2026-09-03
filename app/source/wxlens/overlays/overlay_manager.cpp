#include <wxlens/overlays/overlay_manager.hpp>

#include <wxlens/log/logger.hpp>
#include <wxlens/settings/settings_store.hpp>

#include <scwx/awips/phenomenon.hpp>
#include <scwx/awips/text_product_file.hpp>
#include <scwx/gr/placefile.hpp>
#include <scwx/provider/warnings_provider.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSaveFile>
#include <QTimer>
#include <QUrlQuery>

#include <algorithm>
#include <chrono>
#include <sstream>
#include <thread>

namespace wxlens { namespace overlays {

static const auto logger_ = wxlens::log::Create("wxlens::overlays::overlay_manager");

namespace {

QString ColorString(const boost::gil::rgba8_pixel_t& color)
{
   return QString("#%1%2%3%4")
      .arg(color[3], 2, 16, QLatin1Char('0'))
      .arg(color[0], 2, 16, QLatin1Char('0'))
      .arg(color[1], 2, 16, QLatin1Char('0'))
      .arg(color[2], 2, 16, QLatin1Char('0'));
}

QVariantList Coordinates(const std::vector<scwx::common::Coordinate>& coordinates)
{
   QVariantList result;
   result.reserve(static_cast<qsizetype>(coordinates.size()) * 2);
   for (const auto& coordinate : coordinates)
   {
      result.append(coordinate.latitude_);
      result.append(coordinate.longitude_);
   }
   return result;
}

QString WarningColor(scwx::awips::Phenomenon phenomenon)
{
   switch (phenomenon)
   {
   case scwx::awips::Phenomenon::Tornado: return QStringLiteral("#ffff3b30");
   case scwx::awips::Phenomenon::SevereThunderstorm: return QStringLiteral("#ffffd60a");
   case scwx::awips::Phenomenon::FlashFlood: return QStringLiteral("#ffff2d9a");
   case scwx::awips::Phenomenon::Flood: return QStringLiteral("#ff36d45a");
   default: return QStringLiteral("#ffff9f0a");
   }
}

void AppendWarnings(const std::vector<std::shared_ptr<scwx::awips::TextProductFile>>& files,
                    QVariantList& output)
{
   const auto now = std::chrono::system_clock::now();
   for (const auto& file : files)
   {
      for (const auto& message : file->messages())
      {
         for (const auto& segment : message->segments())
         {
            if (!segment->codedLocation_.has_value() || !segment->header_.has_value() ||
                segment->header_->vtecString_.empty())
            {
               continue;
            }
            const auto& vtec = segment->header_->vtecString_.front().pVtec_;
            if (vtec.action() == scwx::awips::PVtec::Action::Canceled ||
                vtec.action() == scwx::awips::PVtec::Action::Expired ||
                segment->event_end() < now)
            {
               continue;
            }
            const auto coordinates = segment->codedLocation_->coordinates();
            if (coordinates.size() < 3)
            {
               continue;
            }
            QVariantMap item;
            item.insert("coordinates", Coordinates(coordinates));
            item.insert("color", WarningColor(vtec.phenomenon()));
            item.insert("label", QString::fromStdString(
               scwx::awips::GetPhenomenonText(vtec.phenomenon())));
            item.insert("eventId", QString::fromStdString(vtec.office_id()) + '-' +
                                      QString::number(vtec.event_tracking_number()));
            output.append(item);
         }
      }
   }
}

QVariantList FlattenPlacefile(const std::shared_ptr<scwx::gr::Placefile>& placefile)
{
   QVariantList result;
   const QUrl baseUrl = QUrl::fromUserInput(QString::fromStdString(placefile->name()));
   const auto iconFiles = placefile->icon_files();
   for (const auto& base : placefile->GetDrawItems())
   {
      QVariantMap item;
      switch (base->itemType_)
      {
      case scwx::gr::Placefile::ItemType::Icon:
      {
         const auto value = std::static_pointer_cast<scwx::gr::Placefile::IconDrawItem>(base);
         const auto file = std::find_if(iconFiles.begin(), iconFiles.end(), [value](const auto& f)
                                       { return f->fileNumber_ == value->fileNumber_; });
         if (file == iconFiles.end()) continue;
         item.insert("kind", "icon");
         item.insert("latitude", value->latitude_);
         item.insert("longitude", value->longitude_);
         item.insert("source", baseUrl.resolved(QUrl(QString::fromStdString((*file)->filename_))));
         item.insert("width", static_cast<int>((*file)->iconWidth_));
         item.insert("height", static_cast<int>((*file)->iconHeight_));
         item.insert("hotX", static_cast<int>((*file)->hotX_));
         item.insert("hotY", static_cast<int>((*file)->hotY_));
         item.insert("iconNumber", static_cast<int>(value->iconNumber_));
         item.insert("angle", value->angle_.value());
         break;
      }
      case scwx::gr::Placefile::ItemType::Text:
      {
         const auto value = std::static_pointer_cast<scwx::gr::Placefile::TextDrawItem>(base);
         item.insert("kind", "text");
         item.insert("latitude", value->latitude_);
         item.insert("longitude", value->longitude_);
         item.insert("text", QString::fromStdString(value->text_));
         item.insert("color", ColorString(value->color_));
         break;
      }
      case scwx::gr::Placefile::ItemType::Line:
      {
         const auto value = std::static_pointer_cast<scwx::gr::Placefile::LineDrawItem>(base);
         QVariantList coordinates;
         for (const auto& point : value->elements_)
         {
            coordinates.append(point.latitude_);
            coordinates.append(point.longitude_);
         }
         if (coordinates.size() < 4) continue;
         item.insert("kind", "line");
         item.insert("coordinates", coordinates);
         item.insert("color", ColorString(value->color_));
         item.insert("width", std::max(1.0, value->width_));
         break;
      }
      case scwx::gr::Placefile::ItemType::Triangles:
      {
         const auto value = std::static_pointer_cast<scwx::gr::Placefile::TrianglesDrawItem>(base);
         QVariantList coordinates;
         for (const auto& point : value->elements_)
         {
            coordinates.append(point.latitude_);
            coordinates.append(point.longitude_);
         }
         if (coordinates.size() < 6) continue;
         item.insert("kind", "triangles");
         item.insert("coordinates", coordinates);
         item.insert("color", ColorString(value->color_));
         break;
      }
      case scwx::gr::Placefile::ItemType::Polygon:
      {
         const auto value = std::static_pointer_cast<scwx::gr::Placefile::PolygonDrawItem>(base);
         QVariantList contours;
         for (const auto& contour : value->contours_)
         {
            QVariantList coordinates;
            for (const auto& point : contour)
            {
               coordinates.append(point.latitude_);
               coordinates.append(point.longitude_);
            }
            if (coordinates.size() >= 6) contours.append(QVariant(coordinates));
         }
         if (contours.empty()) continue;
         item.insert("kind", "polygon");
         item.insert("contours", contours);
         item.insert("color", ColorString(value->color_));
         break;
      }
      default:
         // Icon/image primitives require external raster resources. The parser still accepts
         // them; vector and text primitives render now and unsupported items are explicit.
         continue;
      }
      result.append(item);
   }
   return result;
}

} // namespace

class OverlayManager::Impl
{
public:
   struct PlacefileRecord
   {
      QUrl source;
      QString title;
      QString error;
      QVariantList items;
      bool loading {false};
   };

   explicit Impl(OverlayManager* self, settings::SettingsStore& settings) :
      self_ {self}, settings_ {settings}, network_ {self}, warningTimer_ {self}
   {
      warningTimer_.setInterval(std::chrono::minutes {1});
      QObject::connect(&warningTimer_, &QTimer::timeout, self_, &OverlayManager::refreshWarnings);
      warningTimer_.start();
   }

   void SetStatus(const QString& value)
   {
      if (statusText_ == value) return;
      statusText_ = value;
      Q_EMIT self_->statusTextChanged();
   }

   QString ConfigPath() const
   {
      return QDir(settings_.ConfigDirectory()).filePath(QStringLiteral("weather-overlays.json"));
   }

   /// Persists visibility + placefile *sources* only - fetched content is always re-derived by
   /// re-fetching, so it is never worth persisting (and would just go stale on disk).
   void SaveConfig() const
   {
      QJsonArray sources;
      for (const auto& record : placefiles_) sources.append(record.source.toString());
      QSaveFile file {ConfigPath()};
      if (!QDir().mkpath(QFileInfo(file.fileName()).absolutePath()) ||
          !file.open(QIODevice::WriteOnly))
      {
         return;
      }
      const QByteArray data =
         QJsonDocument {QJsonObject {{"version", 1},
                                     {"warningsVisible", warningsVisible_},
                                     {"placefilesVisible", placefilesVisible_},
                                     {"placefiles", sources}}}
            .toJson();
      if (file.write(data) == data.size()) file.commit();
   }

   /// Restores visibility + placefile sources. Malformed/missing config is left at the built-in
   /// defaults, never a crash or a half-applied state (matches SettingsStore's error policy).
   void LoadConfig()
   {
      QFile file {ConfigPath()};
      if (!file.open(QIODevice::ReadOnly)) return;
      QJsonParseError error;
      const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
      if (error.error != QJsonParseError::NoError || !document.isObject()) return;
      const QJsonObject root = document.object();
      if (root.value("version").toInt() != 1) return;
      warningsVisible_ = root.value("warningsVisible").toBool(warningsVisible_);
      placefilesVisible_ = root.value("placefilesVisible").toBool(placefilesVisible_);
      for (const QJsonValue value : root.value("placefiles").toArray())
      {
         const QUrl url {value.toString()};
         if (url.isValid() && !url.isEmpty()) placefiles_.push_back({url, url.fileName(), {}, {}, true});
      }
   }

   void ApplyPlacefile(int index, const QByteArray& bytes)
   {
      if (index < 0 || index >= static_cast<int>(placefiles_.size())) return;
      auto& record = placefiles_[index];
      std::istringstream stream {bytes.toStdString()};
      auto parsed = scwx::gr::Placefile::Load(record.source.toString().toStdString(), stream);
      record.loading = false;
      if (!parsed || !parsed->IsValid())
      {
         record.error = QStringLiteral("Invalid placefile");
         SetStatus(record.error);
      }
      else
      {
         record.title = QString::fromStdString(parsed->title());
         record.items = FlattenPlacefile(parsed);
         record.error.clear();
         SetStatus(QStringLiteral("Loaded %1").arg(record.title));
      }
      RebuildItems();
      Q_EMIT self_->placefilesChanged();
   }

   void RebuildItems()
   {
      placefileItems_.clear();
      for (const auto& record : placefiles_)
         for (const auto& item : record.items) placefileItems_.append(item);
   }

   OverlayManager* self_;
   settings::SettingsStore& settings_;
   QNetworkAccessManager network_;
   QTimer warningTimer_;
   QVariantList warningPolygons_;
   QVariantList placefileItems_;
   std::vector<PlacefileRecord> placefiles_;
   bool warningsVisible_ {true};
   bool placefilesVisible_ {true};
   bool refreshingWarnings_ {false};
   QString statusText_;
};

OverlayManager::OverlayManager(settings::SettingsStore& settings, QObject* parent) :
   QObject(parent), p(std::make_unique<Impl>(this, settings))
{
   p->LoadConfig();
   // Sources restored from disk still need their content fetched - only the source list itself
   // was persisted, per LoadConfig's comment.
   for (int index = 0; index < static_cast<int>(p->placefiles_.size()); ++index)
   {
      refreshPlacefile(index);
   }
}
OverlayManager::~OverlayManager() = default;
QVariantList OverlayManager::warningPolygons() const { return p->warningPolygons_; }
QVariantList OverlayManager::placefileItems() const { return p->placefileItems_; }
bool OverlayManager::warningsVisible() const { return p->warningsVisible_; }
bool OverlayManager::placefilesVisible() const { return p->placefilesVisible_; }
bool OverlayManager::refreshingWarnings() const { return p->refreshingWarnings_; }
QString OverlayManager::statusText() const { return p->statusText_; }

QVariantList OverlayManager::placefiles() const
{
   QVariantList result;
   for (const auto& record : p->placefiles_)
      result.append(QVariantMap {{"source", record.source.toString()}, {"title", record.title},
                                 {"error", record.error}, {"loading", record.loading},
                                 {"itemCount", record.items.size()}});
   return result;
}

void OverlayManager::setWarningsVisible(bool value)
{
   if (p->warningsVisible_ == value) return;
   p->warningsVisible_ = value;
   p->SaveConfig();
   Q_EMIT warningsVisibleChanged();
}
void OverlayManager::setPlacefilesVisible(bool value)
{
   if (p->placefilesVisible_ == value) return;
   p->placefilesVisible_ = value;
   p->SaveConfig();
   Q_EMIT placefilesVisibleChanged();
}

void OverlayManager::refreshWarnings()
{
   if (p->refreshingWarnings_) return;
   p->refreshingWarnings_ = true;
   p->SetStatus(QStringLiteral("Refreshing warnings…"));
   Q_EMIT refreshingWarningsChanged();
   QPointer<OverlayManager> guard {this};
   std::thread([guard]()
   {
      QVariantList warnings;
      QString error;
      try
      {
         scwx::provider::WarningsProvider provider {"https://warnings.cod.edu"};
         AppendWarnings(provider.LoadUpdatedFiles(), warnings);
         provider.Shutdown();
      }
      catch (const std::exception& ex) { error = QString::fromUtf8(ex.what()); }
      if (!guard) return;
      QMetaObject::invokeMethod(guard, [guard, warnings = std::move(warnings), error]() mutable
      {
         if (!guard) return;
         guard->p->refreshingWarnings_ = false;
         if (error.isEmpty())
         {
            guard->p->warningPolygons_ = std::move(warnings);
            guard->p->SetStatus(QStringLiteral("%1 active warning polygons")
                                   .arg(guard->p->warningPolygons_.size()));
            Q_EMIT guard->warningsChanged();
         }
         else guard->p->SetStatus(QStringLiteral("Warning refresh failed: %1").arg(error));
         Q_EMIT guard->refreshingWarningsChanged();
      }, Qt::QueuedConnection);
   }).detach();
}

bool OverlayManager::importWarningFile(const QUrl& url)
{
   const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
   scwx::awips::TextProductFile file;
   if (!file.LoadFile(path.toStdString()))
   {
      p->SetStatus(QStringLiteral("Could not parse warning file"));
      return false;
   }
   QVariantList warnings;
   auto owned = std::make_shared<scwx::awips::TextProductFile>(std::move(file));
   AppendWarnings({owned}, warnings);
   p->warningPolygons_ = std::move(warnings);
   p->SetStatus(QStringLiteral("Imported %1 warning polygons").arg(p->warningPolygons_.size()));
   Q_EMIT warningsChanged();
   return true;
}

void OverlayManager::addPlacefile(const QUrl& url)
{
   if (!url.isValid() || url.isEmpty()) return;
   p->placefiles_.push_back({url, url.fileName(), {}, {}, true});
   p->SaveConfig();
   Q_EMIT placefilesChanged();
   refreshPlacefile(static_cast<int>(p->placefiles_.size()) - 1);
}

void OverlayManager::removePlacefile(int index)
{
   if (index < 0 || index >= static_cast<int>(p->placefiles_.size())) return;
   p->placefiles_.erase(p->placefiles_.begin() + index);
   p->RebuildItems();
   p->SaveConfig();
   Q_EMIT placefilesChanged();
}

void OverlayManager::refreshPlacefile(int index)
{
   if (index < 0 || index >= static_cast<int>(p->placefiles_.size())) return;
   auto& record = p->placefiles_[index];
   record.loading = true;
   record.error.clear();
   Q_EMIT placefilesChanged();
   if (record.source.isLocalFile())
   {
      auto parsed = scwx::gr::Placefile::Load(record.source.toLocalFile().toStdString());
      record.loading = false;
      if (parsed && parsed->IsValid())
      {
         record.title = QString::fromStdString(parsed->title());
         record.items = FlattenPlacefile(parsed);
      }
      else record.error = QStringLiteral("Could not parse placefile");
      p->RebuildItems();
      p->SetStatus(record.error.isEmpty() ? QStringLiteral("Loaded %1").arg(record.title)
                                          : record.error);
      Q_EMIT placefilesChanged();
      return;
   }
   QNetworkReply* reply = p->network_.get(QNetworkRequest(record.source));
   connect(reply, &QNetworkReply::finished, this, [this, reply, index]()
   {
      const QByteArray bytes = reply->readAll();
      const QString error = reply->error() == QNetworkReply::NoError ? QString() : reply->errorString();
      reply->deleteLater();
      if (!error.isEmpty())
      {
         if (index < static_cast<int>(p->placefiles_.size()))
         {
            p->placefiles_[index].loading = false;
            p->placefiles_[index].error = error;
            p->SetStatus(QStringLiteral("Placefile refresh failed: %1").arg(error));
            Q_EMIT placefilesChanged();
         }
         return;
      }
      p->ApplyPlacefile(index, bytes);
   });
}

}} // namespace wxlens::overlays
