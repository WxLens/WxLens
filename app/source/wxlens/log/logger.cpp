#include <wxlens/log/logger.hpp>

#include <QDir>
#include <QStandardPaths>

namespace wxlens
{
namespace log
{

namespace
{
QString LogDirectoryPath()
{
   return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
}
} // namespace

void Initialize()
{
   const QString logDir = LogDirectoryPath();
   QDir().mkpath(logDir);

   scwx::util::Logger::Initialize();
   scwx::util::Logger::AddFileSink((logDir + "/wxlens.log").toStdString());
}

std::string LogDirectory()
{
   return QDir::toNativeSeparators(LogDirectoryPath()).toStdString();
}

std::shared_ptr<spdlog::logger> Create(const std::string& subsystemName)
{
   return scwx::util::Logger::Create(subsystemName);
}

} // namespace log
} // namespace wxlens
