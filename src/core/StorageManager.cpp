#include "core/StorageManager.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QSaveFile>

namespace sm {

namespace {
constexpr int kFormatVersion = 1;

bool parseFile(const QString &path, QJsonDocument &doc)
{
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly))
		return false;
	QJsonParseError err;
	doc = QJsonDocument::fromJson(f.readAll(), &err);
	return err.error == QJsonParseError::NoError && (doc.isObject() || doc.isArray());
}

QJsonArray arrayOf(const QJsonDocument &doc, const char *key)
{
	if (doc.isArray())
		return doc.array();
	return doc.object().value(key).toArray();
}
} // namespace

StorageManager::StorageManager(const QString &directory) : dir_(QDir::cleanPath(directory))
{
	QDir().mkpath(dir_);
}

QString StorageManager::filePath(const QString &name) const
{
	return dir_ + QLatin1Char('/') + name;
}

bool StorageManager::readJson(const QString &name, QJsonDocument &out, bool *existed)
{
	const QString path = filePath(name);
	const bool exists = QFileInfo::exists(path);
	if (existed)
		*existed = exists;
	if (!exists)
		return false;
	if (parseFile(path, out))
		return true;

	// Corrupt: keep the evidence, then try the previous good copy.
	const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
	QFile::rename(path, path + ".corrupt-" + stamp);
	lastError_ = QStringLiteral("%1 was corrupt; moved aside").arg(name);
	return parseFile(path + ".bak", out);
}

bool StorageManager::writeJson(const QString &name, const QJsonDocument &doc)
{
	const QString path = filePath(name);
	QDir().mkpath(dir_);

	// Keep the last good version as .bak (cheap; files are small).
	if (QFileInfo::exists(path)) {
		QFile::remove(path + ".bak");
		QFile::copy(path, path + ".bak");
	}

	QSaveFile f(path); // writes to a temp file, commit() atomically replaces the target
	f.setDirectWriteFallback(false);
	if (!f.open(QIODevice::WriteOnly)) {
		lastError_ = f.errorString();
		return false;
	}
	f.write(doc.toJson(QJsonDocument::Indented));
	if (!f.commit()) {
		lastError_ = f.errorString();
		return false;
	}
	return true;
}

QVector<Category> StorageManager::loadCategories(bool *fileExisted)
{
	QVector<Category> out;
	QJsonDocument doc;
	bool parsed = false;
	if (readJson("categories.json", doc, nullptr)) {
		parsed = true;
		for (const QJsonValue &v : arrayOf(doc, "categories")) {
			bool ok = false;
			Category c = Category::fromJson(v.toObject(), &ok);
			if (ok)
				out.push_back(c);
		}
	}
	if (fileExisted)
		*fileExisted = parsed; // an intentionally empty list is respected
	return out;
}

QVector<Session> StorageManager::loadSessions()
{
	QVector<Session> out;
	QJsonDocument doc;
	if (readJson("streams.json", doc, nullptr)) {
		for (const QJsonValue &v : arrayOf(doc, "streams")) {
			bool ok = false;
			Session s = Session::fromJson(v.toObject(), &ok);
			if (ok)
				out.push_back(s);
		}
	}
	return out;
}

QVector<Marker> StorageManager::loadMarkers()
{
	QVector<Marker> out;
	QJsonDocument doc;
	if (readJson("markers.json", doc, nullptr)) {
		for (const QJsonValue &v : arrayOf(doc, "markers")) {
			bool ok = false;
			Marker m = Marker::fromJson(v.toObject(), &ok);
			if (ok)
				out.push_back(m);
		}
	}
	return out;
}

Settings StorageManager::loadSettings()
{
	QJsonDocument doc;
	if (readJson("settings.json", doc, nullptr) && doc.isObject())
		return Settings::fromJson(doc.object());
	return Settings{};
}

bool StorageManager::saveCategories(const QVector<Category> &v)
{
	QJsonArray a;
	for (const auto &c : v)
		a.append(c.toJson());
	return writeJson("categories.json", QJsonDocument(QJsonObject{{"version", kFormatVersion}, {"categories", a}}));
}

bool StorageManager::saveSessions(const QVector<Session> &v)
{
	QJsonArray a;
	for (const auto &s : v)
		a.append(s.toJson());
	return writeJson("streams.json", QJsonDocument(QJsonObject{{"version", kFormatVersion}, {"streams", a}}));
}

bool StorageManager::saveMarkers(const QVector<Marker> &v)
{
	QJsonArray a;
	for (const auto &m : v)
		a.append(m.toJson());
	return writeJson("markers.json", QJsonDocument(QJsonObject{{"version", kFormatVersion}, {"markers", a}}));
}

bool StorageManager::saveSettings(const Settings &s)
{
	return writeJson("settings.json", QJsonDocument(s.toJson()));
}

QJsonDocument StorageManager::loadJsonFile(const QString &name)
{
	QJsonDocument doc;
	readJson(name, doc, nullptr);
	return doc;
}

QString StorageManager::backupMarkers()
{
	const QString src = filePath("markers.json");
	if (!QFileInfo::exists(src))
		return {};
	const QString dst = filePath("markers.backup-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + ".json");
	return QFile::copy(src, dst) ? dst : QString();
}

} // namespace sm
