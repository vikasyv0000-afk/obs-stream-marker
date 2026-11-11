#pragma once
// One OBS hotkey per category (bindable in OBS Settings > Hotkeys), persisted to hotkeys.json.
// Defaults: F8, F9, F10, F11 for the four built-in categories on first run.

#include <QHash>
#include <QJsonArray>
#include <QObject>
#include <functional>

#include "core/CategoryManager.h"
#include "core/StorageManager.h"

namespace sm {

class HotkeyManager : public QObject {
	Q_OBJECT
public:
	HotkeyManager(CategoryManager &cats, StorageManager &storage, QObject *parent = nullptr);
	~HotkeyManager() override;

	// Called on the UI thread with the category id when its hotkey is pressed.
	void setTrigger(std::function<void(const QString &)> fn) { trigger_ = std::move(fn); }

	QString label(const QString &categoryId) const; // e.g. "F8", "Ctrl+Shift+K" or "" if unbound
	void save();                                     // write bindings to hotkeys.json
	void shutdown();                                 // save + unregister (call on OBS exit)

private:
	struct Ctx {
		HotkeyManager *mgr;
		QString categoryId;
	};
	struct Entry {
		size_t id = 0;
		QString name;
		Ctx *ctx = nullptr;
	};
	friend void hotkeyCallback(void *, size_t, void *, bool);
	void sync();
	void registerFor(const Category &c);
	void unregisterFor(const QString &categoryId, bool keepBinding);
	QJsonArray bindingsOf(const QString &categoryId) const;

	CategoryManager &cats_;
	StorageManager &storage_;
	QHash<QString, Entry> entries_;
	QHash<QString, QJsonArray> saved_; // bindings loaded from disk / retained for re-registration
	std::function<void(const QString &)> trigger_;
	bool shut_ = false;
};

} // namespace sm
