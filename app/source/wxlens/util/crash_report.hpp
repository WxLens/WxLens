#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QUrl>

namespace wxlens
{
namespace util
{

// Select diagnostic fields only. Raw OS reports can contain usernames, paths,
// machine identifiers, environment values, and application-specific text.
QJsonObject ParseMacCrashReport(const QByteArray& data);
QJsonObject ParseWindowsCrashReport(const QByteArray& data);
QUrl        SentryEnvelopeUrl(const QString& dsn);
QByteArray  MakeCrashEnvelope(const QJsonObject& event, const QString& dsn);

} // namespace util
} // namespace wxlens
