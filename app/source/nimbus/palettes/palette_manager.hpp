#pragma once

#include <QObject>
#include <QStringList>

#include <nimbus/palettes/palette_model.hpp>

namespace nimbus
{
namespace palettes
{

class PaletteManager : public QObject
{
   Q_OBJECT
   Q_PROPERTY(QStringList paletteNames READ paletteNames NOTIFY paletteNamesChanged)
   Q_PROPERTY(QString activeName READ activeName NOTIFY activePaletteChanged)
   Q_PROPERTY(PaletteModel* editor READ editor CONSTANT)

public:
   explicit PaletteManager(QObject* parent = nullptr);
   static PaletteManager& Instance();

   QStringList paletteNames() const;
   QString activeName() const;
   PaletteModel* editor();
   QString activeText() const;

   Q_INVOKABLE bool select(const QString& name);
   Q_INVOKABLE bool openFile(const QUrl& source);
   Q_INVOKABLE bool resetActiveToFactory();
   Q_INVOKABLE bool resetAllToFactory();
   Q_INVOKABLE bool activeIsFactoryPalette() const;

signals:
   void paletteNamesChanged();
   void activePaletteChanged();
   void paletteTextChanged(const QString& text);

private:
   bool Activate(const QString& name, const QString& text);
   QStringList  names_ {"DR", "DV", "SRV", "SW", "ZDR", "CC", "KDP", "KDP2",
                        "HC", "ET", "VIL", "OHP", "STP", "DOD_DSD", "Default16"};
   QString      activeName_;
   QString      activeText_;
   PaletteModel editor_;
};

} // namespace palettes
} // namespace nimbus
