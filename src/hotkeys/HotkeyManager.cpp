#include "hotkeys/HotkeyManager.h"

#include <obs-module.h>
#include <obs.h>
#include <obs-hotkey.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>

namespace sm {

// Runs on OBS's hotkey thread: hop to the UI thread before touching anything.
void hotkeyCallback(void *data, size_t, void *, bool pressed)
{
	if (!pressed)
		return;
	auto *ctx = static_cast<HotkeyManager::Ctx *>(data);
	HotkeyManager *mgr = ctx->mgr;
	const QString id = ctx->categoryId;
	QMetaObject::invokeMethod(
		mgr,
		[mgr, id] {
			if (!mgr->shut_ && mgr->trigger_)
				mgr->trigger_(id);
		},
		Qt::QueuedConnection);
}

namespace {
void obsCallback(void *data, obs_hotkey_id id, obs_hotkey_t *hk, bool pressed)
{
	hotkeyCallback(data, id, hk, pressed);
}

QJsonArray toJson(obs_data_array_t *arr)
{
	QJsonArray out;
	const size_t n = obs_data_array_count(arr);
	for (size_t i = 0; i < n; ++i) {
		obs_data_t *item = obs_data_array_item(arr, i);
		const QJsonDocument d = QJsonDocument::fromJson(QByteArray(obs_data_get_json(item)));
		if (d.isObject())
			out.append(d.object());
		obs_data_release(item);
	}
	return out;
}

obs_data_array_t *toObs(const QJsonArray &a)
{
	obs_data_array_t *arr = obs_data_array_create();
	for (const QJsonValue &v : a) {
		const QByteArray js = QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact);
		obs_data_t *item = obs_data_create_from_json(js.constData());
		obs_data_array_push_back(arr, item);
		obs_data_release(item);
	}
	return arr;
}
} // namespace

HotkeyManager::HotkeyManager(CategoryManager &cats, StorageManager &storage, QObject *parent)
	: QObject(parent), cats_(cats), storage_(storage)
{
	const QJsonDocument doc = storage_.loadJsonFile("hotkeys.json");
	if (doc.isObject()) {
		const QJsonObject o = doc.object().value("bindings").toObject();
		for (auto it = o.begin(); it != o.end(); ++it)
			saved_[it.key()] = it.value().toArray();
	} else {
		// First run: F8-F11 for the built-in categories (rebindable in OBS Settings > Hotkeys).
		const char *ids[] = {"clutch-kill", "funny-moment", "winner-winner", "epic-moment"};
		const char *keys[] = {"OBS_KEY_F8", "OBS_KEY_F9", "OBS_KEY_F10", "OBS_KEY_F11"};
		for (int i = 0; i < 4; ++i)
			saved_[QLatin1String(ids[i])] = QJsonArray{QJsonObject{{"key", keys[i]}}};
	}
	connect(&cats_, &CategoryManager::changed, this, &HotkeyManager::sync);
	sync();
	save();
}

HotkeyManager::~HotkeyManager()
{
	shutdown();
}

void HotkeyManager::registerFor(const Category &c)
{
	Entry e;
	e.name = c.name;
	e.ctx = new Ctx{this, c.id};
	const QByteArray key = ("obs_stream_marker." + c.id).toUtf8();
	const QByteArray desc = (QStringLiteral("Stream Marker: ") + c.name).toUtf8();
	e.id = obs_hotkey_register_frontend(key.constData(), desc.constData(), obsCallback, e.ctx);
	const auto it = saved_.constFind(c.id);
	if (it != saved_.constEnd() && !it->isEmpty()) {
		obs_data_array_t *arr = toObs(*it);
		obs_hotkey_load(e.id, arr);
		obs_data_array_release(arr);
	}
	entries_.insert(c.id, e);
}

QJsonArray HotkeyManager::bindingsOf(const QString &categoryId) const
{
	const auto it = entries_.constFind(categoryId);
	if (it == entries_.constEnd())
		return saved_.value(categoryId);
	obs_data_array_t *arr = obs_hotkey_save(it->id);
	QJsonArray out = toJson(arr);
	obs_data_array_release(arr);
	return out;
}

void HotkeyManager::unregisterFor(const QString &categoryId, bool keepBinding)
{
	const auto it = entries_.find(categoryId);
	if (it == entries_.end())
		return;
	if (keepBinding)
		saved_[categoryId] = bindingsOf(categoryId);
	else
		saved_.remove(categoryId);
	obs_hotkey_unregister(it->id);
	delete it->ctx;
	entries_.erase(it);
}

void HotkeyManager::sync()
{
	if (shut_)
		return;
	QSet<QString> live;
	for (const Category &c : cats_.categories()) {
		live.insert(c.id);
		auto it = entries_.constFind(c.id);
		if (it != entries_.constEnd() && it->name != c.name)
			unregisterFor(c.id, true); // description changed: re-register keeping the binding
		if (!entries_.contains(c.id))
			registerFor(c);
	}
	for (const QString &id : QStringList(entries_.keys()))
		if (!live.contains(id))
			unregisterFor(id, false);
	save();
}

static QString labelFromBindings(const QJsonArray &a)
{
	for (const QJsonValue &v : a) {
		const QJsonObject o = v.toObject();
		QString key = o.value("key").toString();
		if (key.isEmpty() || key == "OBS_KEY_NONE")
			continue;
		key.remove("OBS_KEY_");
		QStringList parts;
		if (o.value("control").toBool())
			parts << "Ctrl";
		if (o.value("alt").toBool())
			parts << "Alt";
		if (o.value("shift").toBool())
			parts << "Shift";
		if (o.value("command").toBool())
			parts << "Win";
		parts << key;
		return parts.join('+');
	}
	return {};
}

QString HotkeyManager::label(const QString &categoryId) const
{
	return labelFromBindings(bindingsOf(categoryId));
}

void HotkeyManager::save()
{
	QJsonObject o;
	for (const Category &c : cats_.categories())
		o[c.id] = bindingsOf(c.id);
	storage_.saveJsonFile("hotkeys.json", QJsonDocument(QJsonObject{{"version", 1}, {"bindings", o}}));
}

void HotkeyManager::shutdown()
{
	if (shut_)
		return;
	save();
	shut_ = true;
	for (const QString &id : QStringList(entries_.keys()))
		unregisterFor(id, true);
}

} // namespace sm
