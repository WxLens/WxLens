#pragma once

#include <QString>

namespace wxlens
{
namespace palettes
{

/** Maps wxdata's canonical palette identity to the corresponding bundled WCT asset name. */
inline QString BundledPaletteName(const QString& canonicalName)
{
   if (canonicalName == QStringLiteral("BR")) return QStringLiteral("DR");
   if (canonicalName == QStringLiteral("BV")) return QStringLiteral("DV");
   if (canonicalName == QStringLiteral("PHI2")) return QStringLiteral("KDP2");
   if (canonicalName == QStringLiteral("PHI3")) return QStringLiteral("KDP");
   if (canonicalName == QStringLiteral("DOD") || canonicalName == QStringLiteral("DSD"))
      return QStringLiteral("DOD_DSD");
   if (canonicalName == QStringLiteral("OHPIN")) return QStringLiteral("OHP");
   if (canonicalName == QStringLiteral("STPIN")) return QStringLiteral("STP");
   return canonicalName;
}

} // namespace palettes
} // namespace wxlens
