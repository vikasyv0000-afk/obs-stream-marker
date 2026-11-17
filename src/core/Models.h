#pragma once
// Plain data types shared by the whole plugin (no OBS or UI dependencies).

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace sm {

struct Category {
	QString id;
	QString name;
	QString color; // "#RRGGBB" pastel background
	QString icon;  // emoji / short text shown above the label

	QJsonObject toJson() const;
	static Category fromJson(const QJsonObject &o, bool *ok = nullptr);
};

struct Session {
	QString id;
	QString title;
	QString kind;         // "stream" (real OBS stream) or "manual" (offline markers)
	QString startedUtc;   // ISO 8601 UTC
	QString startedLocal; // yyyy-MM-dd HH:mm:ss (local)
	QString endedUtc;     // empty while the session is open
	bool interrupted = false; // OBS closed/crashed before the session was closed

	bool isOpen() const { return endedUtc.isEmpty(); }
	QJsonObject toJson() const;
	static Session fromJson(const QJsonObject &o, bool *ok = nullptr);
};

struct Marker {
	QString id;
	QString sessionId;
	QString categoryId;
	QString categoryName;
	qint64 elapsedSeconds = 0;
	QString elapsedFormatted; // HH:MM:SS
	QString localDate;        // yyyy-MM-dd
	QString localDateTime;    // yyyy-MM-dd HH:mm:ss
	QString utcTimestamp;     // ISO 8601, e.g. 2026-11-09T14:03:21.123Z
	QString notes;

	QJsonObject toJson() const;
	static Marker fromJson(const QJsonObject &o, bool *ok = nullptr);
};

struct Settings {
	bool allowOfflineMarkers = false; // record markers while not streaming
	int offsetSeconds = 0;            // added to elapsed time (e.g. -10 for stream latency lead-in)
	bool promptForNote = false;       // ask for a note after each click
	int recentCount = 50;             // rows shown in the dock history
	bool trackRecording = true;       // OBS "Start Recording" also opens a marker session
	bool trackVirtualCam = true;      // OBS "Start Virtual Camera" also opens a marker session
	bool soundOnMarker = true;        // short tone when a marker is recorded (pitch differs per category)
	bool toastOnMarker = false;       // small always-on-top, non-focus-stealing confirmation popup

	QJsonObject toJson() const;
	static Settings fromJson(const QJsonObject &o);
};

// Elapsed seconds -> "HH:MM:SS" (hours keep growing past 99).
QString formatElapsed(qint64 seconds);
QString newId();

} // namespace sm
