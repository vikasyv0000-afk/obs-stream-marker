#pragma once
// Stream session lifecycle + marker recording.
//
// A session is opened by onStreamingStarted() (taken from the real OBS
// "streaming started" event) and closed by onStreamingStopped(). Marker times
// are measured with a steady_clock based StreamTimer.

#include <QObject>
#include <QVector>
#include <optional>

#include "core/CategoryManager.h"
#include "core/Models.h"
#include "core/StorageManager.h"
#include "core/StreamTimer.h"

namespace sm {

class SessionManager : public QObject {
	Q_OBJECT
public:
	enum class RecordStatus { Ok, NotStreaming, SaveFailed };
	struct RecordResult {
		RecordStatus status = RecordStatus::NotStreaming;
		Marker marker;
	};

	explicit SessionManager(StorageManager &storage, QObject *parent = nullptr);

	// --- lifecycle (call from the UI thread) ---
	void onStreamingStarted(StreamTimer::Clock::time_point at = StreamTimer::Clock::now());
	void onStreamingStopped();
	void finalize(); // OBS exit: close any open session and flush everything

	bool streaming() const { return streamSessionId_.size() > 0; }
	bool sessionOpen() const { return timer_.running(); }
	QString activeSessionId() const { return openSessionId_; }
	qint64 elapsedSeconds() const { return timer_.elapsedSeconds(); }

	// --- markers ---
	RecordResult record(const Category &category, const QString &notes = QString());
	bool setNote(const QString &markerId, const QString &notes);
	bool deleteMarker(const QString &markerId);
	int clearMarkers(const QString &sessionId); // makes a backup first, returns removed count
	bool deleteSession(const QString &sessionId);

	// --- queries ---
	const QVector<Session> &sessions() const { return sessions_; }
	const QVector<Marker> &allMarkers() const { return markers_; }
	QVector<Marker> markersFor(const QString &sessionId) const; // chronological
	std::optional<Session> session(const QString &id) const;
	QString displaySessionId() const; // open session, else most recent one
	int markerCount(const QString &sessionId) const;

	// Title used for the open session / the next one.
	QString title() const;
	void setTitle(const QString &t);
	bool renameSession(const QString &id, const QString &title);

	const Settings &settings() const { return settings_; }
	void setSettings(const Settings &s);
	StorageManager &storage() { return storage_; }

signals:
	void sessionStateChanged();                // started / stopped / title changed
	void markersChanged();                     // added / removed / edited
	void markerAdded(const sm::Marker &marker);
	void saveFailed(const QString &message);

private:
	void openSession(const QString &kind, StreamTimer::Clock::time_point at);
	void closeSession();
	void saveSessions();
	void saveMarkers();
	static QString defaultTitle();

	StorageManager &storage_;
	QVector<Session> sessions_;
	QVector<Marker> markers_;
	Settings settings_;
	StreamTimer timer_;
	QString openSessionId_;   // session currently receiving markers (stream or manual)
	QString streamSessionId_; // non-empty only while a real stream is live
	QString pendingTitle_;    // title typed before the stream starts
};

} // namespace sm
