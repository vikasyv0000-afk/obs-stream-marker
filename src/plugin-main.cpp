// Plugin entry point: wires OBS frontend events to the session manager and
// registers the "Stream Marker" dock.

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>

#include <QCoreApplication>
#include <QFileInfo>
#include <QMainWindow>
#include <QMetaObject>
#include <QPointer>
#include <memory>

#include "core/CategoryManager.h"
#include "core/SessionManager.h"
#include "core/StorageManager.h"
#include "core/StreamTimer.h"
#include "hotkeys/HotkeyManager.h"
#include "ui/MarkerDock.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

namespace {

constexpr const char *kDockId = "obs-stream-marker-dock";

struct PluginState {
	std::unique_ptr<sm::StorageManager> storage;
	std::unique_ptr<sm::CategoryManager> categories;
	std::unique_ptr<sm::SessionManager> sessions;
	std::unique_ptr<sm::HotkeyManager> hotkeys;
	QPointer<sm::MarkerDock> dock;
	bool saveCallbackRegistered = false;
	bool callbackRegistered = false; // guards against duplicate event registration
	bool dockAdded = false;
};
PluginState *g_state = nullptr;

void addDock()
{
	if (!g_state || g_state->dockAdded)
		return;
	auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!mainWindow)
		return;

	// Ownership of the widget passes to OBS (it wraps it in a QDockWidget).
	auto *dock = new sm::MarkerDock(g_state->sessions.get(), g_state->categories.get());
	if (obs_frontend_add_dock_by_id(kDockId, obs_module_text("StreamMarker.DockTitle"), dock)) {
		g_state->dockAdded = true;
		g_state->dock = dock;
		if (g_state->hotkeys) {
			sm::HotkeyManager *hk = g_state->hotkeys.get();
			dock->setHotkeyLabelProvider([hk](const QString &id) { return hk->label(id); });
		}
	} else {
		delete dock;
		obs_log(LOG_WARNING, "dock '%s' could not be registered (already exists?)", kDockId);
	}
}

// OBS saves profile/scene data (and on exit): persist hotkey bindings alongside.
void onSave(obs_data_t *, bool saving, void *)
{
	if (saving && g_state && g_state->hotkeys)
		g_state->hotkeys->save();
}

void onFrontendEvent(enum obs_frontend_event event, void *)
{
	if (!g_state || !g_state->sessions)
		return;
	sm::SessionManager *sessions = g_state->sessions.get();

	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		addDock();
		// Plugin loaded while a stream was already running: best effort, start the clock now.
		if (obs_frontend_streaming_active() && !sessions->streaming())
			sessions->onStreamingStarted();
		break;

	case OBS_FRONTEND_EVENT_STREAMING_STARTED: {
		// OBS reports the output as connected and running. Sample the monotonic clock
		// right here, before any queued hop, so the start time is as exact as possible.
		const auto at = sm::StreamTimer::Clock::now();
		QMetaObject::invokeMethod(sessions, [sessions, at] { sessions->onStreamingStarted(at); });
		break;
	}
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		QMetaObject::invokeMethod(sessions, [sessions] { sessions->onStreamingStopped(); });
		break;

	case OBS_FRONTEND_EVENT_EXIT:
		// OBS is shutting down: close the session and flush everything to disk.
		if (g_state->hotkeys)
			g_state->hotkeys->shutdown();
		sessions->finalize();
		if (g_state->saveCallbackRegistered) {
			obs_frontend_remove_save_callback(onSave, nullptr);
			g_state->saveCallbackRegistered = false;
		}
		if (g_state->callbackRegistered) {
			obs_frontend_remove_event_callback(onFrontendEvent, nullptr);
			g_state->callbackRegistered = false;
		}
		break;

	default:
		break;
	}
}

} // namespace

bool obs_module_load(void)
{
	if (g_state)
		return true; // never initialise twice

	g_state = new PluginState;

	char *cfgFile = obs_module_config_path("categories.json");
	QString dir;
	if (cfgFile) {
		dir = QFileInfo(QString::fromUtf8(cfgFile)).absolutePath();
		bfree(cfgFile);
	}
	if (dir.isEmpty()) {
		obs_log(LOG_ERROR, "could not determine plugin config directory");
		delete g_state;
		g_state = nullptr;
		return false;
	}

	g_state->storage = std::make_unique<sm::StorageManager>(dir);
	g_state->categories = std::make_unique<sm::CategoryManager>(*g_state->storage);
	g_state->sessions = std::make_unique<sm::SessionManager>(*g_state->storage);

	g_state->hotkeys = std::make_unique<sm::HotkeyManager>(*g_state->categories, *g_state->storage);
	g_state->hotkeys->setTrigger([](const QString &categoryId) {
		if (!g_state)
			return;
		if (g_state->dock) {
			g_state->dock->triggerCategory(categoryId);
		} else if (auto cat = g_state->categories->byId(categoryId)) {
			g_state->sessions->record(*cat);
		}
	});
	obs_frontend_add_save_callback(onSave, nullptr);
	g_state->saveCallbackRegistered = true;

	if (!g_state->callbackRegistered) {
		obs_frontend_add_event_callback(onFrontendEvent, nullptr);
		g_state->callbackRegistered = true;
	}

	addDock(); // main window already exists when modules load; otherwise FINISHED_LOADING retries

	obs_log(LOG_INFO, "loaded (version %s), data directory: %s", PLUGIN_VERSION, dir.toUtf8().constData());
	return true;
}

void obs_module_unload(void)
{
	if (!g_state)
		return;
	if (g_state->callbackRegistered)
		obs_frontend_remove_event_callback(onFrontendEvent, nullptr);
	if (g_state->saveCallbackRegistered)
		obs_frontend_remove_save_callback(onSave, nullptr);
	if (g_state->hotkeys)
		g_state->hotkeys->shutdown();
	g_state->hotkeys.reset();
	if (g_state->sessions)
		g_state->sessions->finalize(); // idempotent safety net
	// The dock widget is owned and destroyed by OBS; it only holds QPointers to our managers.
	g_state->sessions.reset();
	g_state->categories.reset();
	g_state->storage.reset();
	delete g_state;
	g_state = nullptr;
	obs_log(LOG_INFO, "unloaded");
}
