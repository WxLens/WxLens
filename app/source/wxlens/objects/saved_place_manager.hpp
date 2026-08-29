#pragma once

#include <memory>

#include <QObject>
#include <QVariantList>

namespace wxlens
{
namespace settings
{
class SettingsStore;
}
namespace objects
{

class MapObjectStore;

/** Persistent personal locations and their colour/visibility groups (ROADMAP §4.9).
 *
 * Places themselves are Saved Marker MapObjects in the unified MapObjectStore. This object owns
 * only the extra taxonomy and the portable JSON representation; it is deliberately not a second
 * object store.
 */
class SavedPlaceManager : public QObject
{
   Q_OBJECT
   Q_PROPERTY(QVariantList groups READ groups NOTIFY changed)
   Q_PROPERTY(QVariantList places READ places NOTIFY changed)

public:
   explicit SavedPlaceManager(MapObjectStore& store,
                              settings::SettingsStore& settings,
                              QObject* parent = nullptr);
   ~SavedPlaceManager() override;

   [[nodiscard]] QVariantList groups() const;
   [[nodiscard]] QVariantList places() const;

   Q_INVOKABLE QString addGroup(const QString& name, const QString& color);
   Q_INVOKABLE bool editGroup(const QString& id, const QString& name, const QString& color);
   Q_INVOKABLE bool setGroupVisible(const QString& id, bool visible);
   Q_INVOKABLE bool removeGroup(const QString& id);

   Q_INVOKABLE int addPlace(const QString& name,
                            double latitude,
                            double longitude,
                            const QString& groupId,
                            const QString& colorOverride = {});
   Q_INVOKABLE bool editPlace(int id,
                              const QString& name,
                              double latitude,
                              double longitude,
                              const QString& groupId,
                              const QString& colorOverride = {});
   Q_INVOKABLE bool removePlace(int id);
   Q_INVOKABLE QVariantList search(const QString& query) const;

   Q_INVOKABLE bool importFile(const QString& fileUrl);
   Q_INVOKABLE bool exportFile(const QString& fileUrl);

   [[nodiscard]] bool IsSavedPlaceVisible(const QString& groupId) const;
   [[nodiscard]] QString EffectiveColor(const QString& groupId,
                                        const QString& overrideColor) const;

signals:
   void changed();
   void errorOccurred(const QString& message);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace objects
} // namespace wxlens
