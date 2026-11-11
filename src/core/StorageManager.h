#pragma once
// Local JSON persistence with crash-safe writes.
//
// Files (inside the plugin's OBS config directory):
//   categories.json, streams.json, markers.json, settings.json
// Every write goes to a temporary file first and is atomically renamed over
// the old one (QSaveFile). The previous version is kept as <name>.bak and used
// as a fallback if the main file turns out to be corrupt.

#include <QJsonArray>
#include <QJsonDocument>
#include <QString>
#include <QVector>

#include "core/Models.h"

namespace sm {

class StorageManager {
public:
	explicit StorageManager(const QString &directory);

	const QString &directory() const { return dir_; }
	QString filePath(const QString &name) const;

	// Loaders never fail hard: missing/corrupt files yield empty results and
	// (for corrupt files) the bad file is quarantined as *.corrupt-<time>.
	QVector<Category> loadCategories(bool *fileExisted = nullptr);
	QVector<Session> loadSessions();
	QVector<Marker> loadMarkers();
	Settings loadSettings();

	bool saveCategories(const QVector<Category> &v);
	bool saveSessions(const QVector<Session> &v);
	bool saveMarkers(const QVector<Marker> &v);
	bool saveSettings(const Settings &s);

	// Generic helpers for auxiliary files (e.g. hotkeys.json); same crash-safety guarantees.
	QJsonDocument loadJsonFile(const QString &name);
	bool saveJsonFile(const QString &name, const QJsonDocument &doc) { return writeJson(name, doc); }

	// Copy markers.json to markers.backup-<timestamp>.json (used before destructive ops).
	QString backupMarkers();

	QString lastError() const { return lastError_; }

private:
	bool writeJson(const QString &name, const QJsonDocument &doc);
	// Reads an object {"version":1,"<key>":[...]} (or a bare array). Returns false if missing/corrupt.
	bool readJson(const QString &name, QJsonDocument &out, bool *existed);

	QString dir_;
	QString lastError_;
};

} // namespace sm
