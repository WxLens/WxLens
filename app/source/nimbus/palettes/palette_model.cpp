#include <nimbus/palettes/palette_model.hpp>
#include <nimbus/log/logger.hpp>

#include <scwx/common/color_table.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>

namespace nimbus
{
namespace palettes
{

namespace
{
const auto logger_ = nimbus::log::Create("palettes.palette_model");

QColor ToQColor(const boost::gil::rgba8_pixel_t& color)
{
   return QColor {color[0], color[1], color[2], color[3]};
}
} // namespace

PaletteModel::PaletteModel(QObject* parent) : QAbstractListModel(parent) {}

int PaletteModel::rowCount(const QModelIndex& parent) const
{
   return parent.isValid() ? 0 : stops_.size();
}

QVariant PaletteModel::data(const QModelIndex& index, int role) const
{
   if (!index.isValid() || index.row() < 0 || index.row() >= stops_.size()) return {};
   const Stop& stop = stops_[index.row()];
   switch (role)
   {
   case ValueRole: return stop.value;
   case ColorRole: return stop.first;
   case SecondColorRole: return stop.second;
   case HasSecondColorRole: return stop.hasSecond;
   default: return {};
   }
}

QHash<int, QByteArray> PaletteModel::roleNames() const
{
   return {{ValueRole, "value"},
           {ColorRole, "stopColor"},
           {SecondColorRole, "secondColor"},
           {HasSecondColorRole, "hasSecondColor"}};
}

QString PaletteModel::name() const { return name_; }
bool PaletteModel::valid() const { return valid_; }
QVariantList PaletteModel::previewStops() const { return previewStops_; }
double PaletteModel::minimumValue() const { return minimumValue_; }
double PaletteModel::maximumValue() const { return maximumValue_; }
bool PaletteModel::dirty() const { return dirty_; }
QString PaletteModel::text() const { return lines_.join('\n') + '\n'; }

void PaletteModel::load(const QString& name, const QString& text)
{
   beginResetModel();
   name_  = name;
   lines_ = text.split('\n');
   if (!lines_.isEmpty() && lines_.last().isEmpty()) lines_.removeLast();
   Parse();
   originalText_ = this->text();
   dirty_        = false;
   endResetModel();
   Q_EMIT nameChanged();
   Q_EMIT previewChanged();
   Q_EMIT dirtyChanged();
}

void PaletteModel::Parse()
{
   stops_.clear();
   static const QRegularExpression whitespace {"\\s+"};
   for (int i = 0; i < lines_.size(); ++i)
   {
      const QString line = lines_[i];
      const int commentAt = line.indexOf(';');
      const QString body = line.left(commentAt < 0 ? line.size() : commentAt).trimmed();
      const QStringList tokens = body.split(whitespace, Qt::SkipEmptyParts);
      if (tokens.size() < 5) continue;
      const QString directive = tokens[0].toLower();
      const bool color = directive == "color:" || directive == "color4:";
      const bool solid = directive == "solidcolor:" || directive == "solidcolor4:";
      if (!color && !solid) continue;
      bool ok = false;
      const double value = tokens[1].toDouble(&ok);
      if (!ok) continue;
      const bool alpha = directive.endsWith("4:");
      const int width = alpha ? 4 : 3;
      if (tokens.size() < 2 + width) continue;
      auto parseColor = [&](int offset)
      {
         return QColor(tokens[offset].toInt(), tokens[offset + 1].toInt(),
                       tokens[offset + 2].toInt(), alpha ? tokens[offset + 3].toInt() : 255);
      };
      Stop stop {i, tokens[0], value, parseColor(2), {}, false,
                 commentAt < 0 ? QString {} : line.mid(commentAt)};
      if (color && tokens.size() >= 2 + width * 2)
      {
         stop.second = parseColor(2 + width);
         stop.hasSecond = true;
      }
      stops_.append(stop);
   }

   std::istringstream stream(text().toStdString());
   auto table = scwx::common::ColorTable::Load(stream);
   valid_ = table != nullptr && table->IsValid();
   previewStops_.clear();
   if (valid_ && !stops_.isEmpty())
   {
      const auto [minIt, maxIt] = std::minmax_element(
         stops_.cbegin(), stops_.cend(), [](const Stop& a, const Stop& b) { return a.value < b.value; });
      minimumValue_ = minIt->value;
      maximumValue_ = maxIt->value;
      constexpr int kSamples = 64;
      for (int i = 0; i < kSamples; ++i)
      {
         const double t = static_cast<double>(i) / (kSamples - 1);
         const float value = static_cast<float>(std::lerp(minIt->value, maxIt->value, t));
         previewStops_.append(QVariantMap {{"position", t}, {"color", ToQColor(table->Color(value))}});
      }
   }
}

void PaletteModel::RewriteStop(int row)
{
   Stop& stop = stops_[row];
   const bool alpha = stop.directive.toLower().endsWith("4:");
   auto channels = [alpha](const QColor& c)
   {
      QString result = QString("%1 %2 %3").arg(c.red()).arg(c.green()).arg(c.blue());
      if (alpha) result += QString(" %1").arg(c.alpha());
      return result;
   };
   QString line = QString("%1 %2 %3").arg(stop.directive).arg(stop.value, 0, 'g', 12).arg(channels(stop.first));
   if (stop.hasSecond) line += " " + channels(stop.second);
   if (!stop.comment.isEmpty()) line += " " + stop.comment;
   lines_[stop.line] = line;
   Parse();
   const bool wasDirty = dirty_;
   dirty_ = text() != originalText_;
   Q_EMIT dataChanged(index(0), index(rowCount() - 1));
   Q_EMIT previewChanged();
   Q_EMIT contentChanged(text());
   if (dirty_ != wasDirty) Q_EMIT dirtyChanged();
}

void PaletteModel::setStopValue(int row, double value)
{
   if (row < 0 || row >= stops_.size() || !std::isfinite(value)) return;
   stops_[row].value = value;
   RewriteStop(row);
}

void PaletteModel::setStopColor(int row, const QColor& color, bool second)
{
   if (row < 0 || row >= stops_.size() || !color.isValid()) return;
   if (second && stops_[row].hasSecond) stops_[row].second = color;
   else stops_[row].first = color;
   RewriteStop(row);
}

bool PaletteModel::saveAs(const QUrl& destination)
{
   const QString path = destination.toLocalFile();
   if (path.isEmpty()) return false;
   QSaveFile file(path);
   // Binary mode is intentional: QIODevice::Text rewrites LF to CRLF on Windows. A palette is a
   // shareable text artifact and save-as must write exactly what the model previews.
   if (!file.open(QIODevice::WriteOnly)) return false;
   const QByteArray bytes = text().toUtf8();
   const bool success = file.write(bytes) == bytes.size() && file.commit();
   if (success)
   {
      logger_->info("Saved palette as {}", path.toStdString());
      originalText_ = text();
      if (dirty_)
      {
         dirty_ = false;
         Q_EMIT dirtyChanged();
      }
   }
   return success;
}

void PaletteModel::revertChanges()
{
   if (!dirty_) return;
   const QString original = originalText_;
   load(name_, original);
   Q_EMIT contentChanged(text());
}

} // namespace palettes
} // namespace nimbus
