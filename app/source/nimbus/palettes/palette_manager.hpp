#pragma once

#include <map>

#include <QObject>
#include <QStringList>

#include <nimbus/palettes/palette_model.hpp>

namespace nimbus
{
namespace palettes
{

/** Owns the palette catalog and the editor session's state-changing workflow. */
class PaletteManager : public QObject
{
   Q_OBJECT
   Q_PROPERTY(QStringList paletteNames READ paletteNames NOTIFY paletteNamesChanged)
   Q_PROPERTY(QString activeName READ activeName NOTIFY activePaletteChanged)
   Q_PROPERTY(bool activeIsFactoryPalette READ activeIsFactoryPalette NOTIFY activePaletteChanged)
   Q_PROPERTY(bool confirmationRequired READ confirmationRequired NOTIFY confirmationRequiredChanged)
   Q_PROPERTY(PaletteModel* editor READ editor CONSTANT)

public:
   enum class UnsavedDecision
   {
      SaveCopy = 0,
      Discard,
      KeepEditing
   };
   Q_ENUM(UnsavedDecision)

   explicit PaletteManager(QObject* parent = nullptr);
   static PaletteManager& Instance();

   QStringList paletteNames() const;
   QString activeName() const;
   bool activeIsFactoryPalette() const;
   bool confirmationRequired() const;
   PaletteModel* editor();
   QString activeText() const;

   Q_INVOKABLE bool select(const QString& name);
   Q_INVOKABLE bool openFile(const QUrl& source);
   Q_INVOKABLE bool saveAs(const QUrl& destination);

   Q_INVOKABLE void requestClose();
   Q_INVOKABLE void requestSelect(const QString& name);
   Q_INVOKABLE void requestImport();
   Q_INVOKABLE void requestResetActive();
   Q_INVOKABLE void requestResetAll();
   Q_INVOKABLE void resolveUnsavedChanges(UnsavedDecision decision);
   Q_INVOKABLE void completePendingSave(const QUrl& destination);

signals:
   void paletteNamesChanged();
   void activePaletteChanged();
   void confirmationRequiredChanged();
   void paletteTextChanged(const QString& text);
   void closeRequested();
   void importFileRequested();
   void saveFileRequested();

private:
   struct Entry
   {
      QString workingText;
      QString factoryText;
      QUrl    source;
      bool    factory {false};
   };

   enum class PendingAction
   {
      None,
      Close,
      Select,
      Import,
      ResetActive,
      ResetAll
   };

   bool Activate(const QString& name);
   void BeginAction(PendingAction action, const QString& target = {});
   void ExecutePendingAction();
   void ClearPendingAction();
   QString UniqueImportedName(const QString& baseName) const;

   QStringList                  names_;
   std::map<QString, Entry>     entries_;
   QString                      activeName_;
   QString                      activeText_;
   PaletteModel                 editor_;
   PendingAction                pendingAction_ {PendingAction::None};
   QString                      pendingTarget_;
   bool                         confirmationRequired_ {false};
};

} // namespace palettes
} // namespace nimbus
