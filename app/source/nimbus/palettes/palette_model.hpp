#pragma once

#include <QAbstractListModel>
#include <QColor>
#include <QUrl>

namespace nimbus
{
namespace palettes
{

class PaletteModel : public QAbstractListModel
{
   Q_OBJECT
   Q_PROPERTY(QString name READ name NOTIFY nameChanged)
   Q_PROPERTY(bool valid READ valid NOTIFY previewChanged)
   Q_PROPERTY(QVariantList previewStops READ previewStops NOTIFY previewChanged)
   Q_PROPERTY(double minimumValue READ minimumValue NOTIFY previewChanged)
   Q_PROPERTY(double maximumValue READ maximumValue NOTIFY previewChanged)
   Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)

public:
   enum Role
   {
      ValueRole = Qt::UserRole + 1,
      ColorRole,
      SecondColorRole,
      HasSecondColorRole
   };

   explicit PaletteModel(QObject* parent = nullptr);

   int rowCount(const QModelIndex& parent = {}) const override;
   QVariant data(const QModelIndex& index, int role) const override;
   QHash<int, QByteArray> roleNames() const override;

   QString name() const;
   bool valid() const;
   QVariantList previewStops() const;
   double minimumValue() const;
   double maximumValue() const;
   bool dirty() const;

   Q_INVOKABLE bool setStopValue(int row, double value);
   Q_INVOKABLE bool setStopColor(int row, const QColor& color, bool second = false);
   Q_INVOKABLE bool saveAs(const QUrl& destination);
   Q_INVOKABLE void revertChanges();

   void load(const QString& name, const QString& text);
   QString text() const;

signals:
   void nameChanged();
   void previewChanged();
   void dirtyChanged();
   void contentChanged(const QString& text);

private:
   struct Stop
   {
      int     line {-1};
      QString directive;
      double  value {0.0};
      QColor  first;
      QColor  second;
      bool    hasSecond {false};
      QString comment;
   };
   void Parse();
   void RewriteStop(int row);

   QString       name_;
   QString       originalText_;
   QStringList   lines_;
   QList<Stop>   stops_;
   QVariantList previewStops_;
   bool          valid_ {false};
   bool          dirty_ {false};
   double        minimumValue_ {0.0};
   double        maximumValue_ {1.0};
};

} // namespace palettes
} // namespace nimbus
