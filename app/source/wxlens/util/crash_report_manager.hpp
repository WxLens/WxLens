#pragma once

#include <memory>
#include <QObject>
#include <QString>
#include <QUrl>

namespace wxlens
{
namespace settings
{
class SettingsStore;
} // namespace settings
namespace util
{

class CrashReportManager : public QObject
{
   Q_OBJECT
   Q_PROPERTY(bool visible READ visible NOTIFY changed)
   Q_PROPERTY(bool hasReport READ hasReport NOTIFY changed)
   Q_PROPERTY(bool canSend READ canSend NOTIFY changed)
   Q_PROPERTY(bool sending READ sending NOTIFY changed)
   Q_PROPERTY(QString reportText READ reportText NOTIFY changed)
   Q_PROPERTY(QString status READ status NOTIFY changed)
   Q_PROPERTY(QString destination READ destination CONSTANT)

public:
   explicit CrashReportManager(settings::SettingsStore& store,
                               const QString&           logDirectory,
                               const QString&           dsn,
                               QObject*                 parent = nullptr);
   ~CrashReportManager() override;

   bool    visible() const;
   bool    hasReport() const;
   bool    canSend() const;
   bool    sending() const;
   QString reportText() const;
   QString status() const;
   QString destination() const;

   Q_INVOKABLE void open();
   Q_INVOKABLE void dismiss();
   Q_INVOKABLE void save(const QUrl& file);
   Q_INVOKABLE void send();

signals:
   void changed();

private:
   void scan();
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace util
} // namespace wxlens
