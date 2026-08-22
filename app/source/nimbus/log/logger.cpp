#include <nimbus/log/logger.hpp>

#include <QDir>
#include <QStandardPaths>

namespace nimbus
{
namespace log
{

void Initialize()
{
   scwx::util::Logger::Initialize();

   const QString logDir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
   QDir().mkpath(logDir);

   scwx::util::Logger::AddFileSink((logDir + "/nimbus.log").toStdString());
}

std::shared_ptr<spdlog::logger> Create(const std::string& subsystemName)
{
   return scwx::util::Logger::Create(subsystemName);
}

} // namespace log
} // namespace nimbus
