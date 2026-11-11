#include "core/SessionManager.h"

#include <QDateTime>
#include <algorithm>

namespace sm {

namespace {
QString utcIso(const QDateTime &dt)
{
	return dt.toUTC().toString(Qt::ISODateWithMs); // e.g. 2026-11-09T14:03:21.123Z
}
QString localStr(const QDateTime &dt)
{
	return dt.toString("yyyy-MM-dd HH:mm:ss");
}
} // namespace

SessionManager::SessionManager(StorageManager &storage, QObject *parent) : QObject(parent), storage_(storage)
{
	sessions_ = storage_.loadSessions();
	markers_ = storage_.loadMarkers();
	settings_ = storage_.loadSettings();

	// Sessions still open on disk mean OBS exited/crashed mid-stream. Close them
	// at their last marker (or start) time and flag them; markers were already saved.
	bool repaired = false;
	for (Session &s : sessions_) {
		if (!s.isOpen())
			continue;
		QString last = s.startedUtc;
		for (const Marker &m : markers_)
			if (m.sessionId == s.id && m.utcTimestamp > last)
				last = m.utcTimestamp;
		s.endedUtc = last.isEmpty() ? utcIso(QDateTime::currentDateTime()) : last;
		s.interrupted = true;
		repaired = true;
	}
	if (repaired)
		saveSessions();
}

QString SessionManager::defaultTitle()
{
	return QStringLiteral("Stream ") + QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm");
}

// ---- lifecycle ------------------------------------------------------------

void SessionManager::openSession(const QString &kind, StreamTimer::Clock::time_point at)
{
	const QDateTime now = QDateTime::currentDateTime();
	Session s;
	s.id = newId();
	s.kind = kind;
	s.title = pendingTitle_.trimmed().isEmpty() ? defaultTitle() : pendingTitle_.trimmed();
	if (kind == "manual")
		s.title = QStringLiteral("Offline markers ") + now.toString("yyyy-MM-dd HH:mm");
	s.startedUtc = utcIso(now);
	s.startedLocal = localStr(now);
	sessions_.push_back(s);
	openSessionId_ = s.id;
	timer_.start(at);
	saveSessions(); // persist immediately so a crash still leaves a valid session record
}

void SessionManager::closeSession()
{
	if (openSessionId_.isEmpty())
		return;
	for (Session &s : sessions_)
		if (s.id == openSessionId_) {
			s.endedUtc = utcIso(QDateTime::currentDateTime());
			break;
		}
	openSessionId_.clear();
	timer_.stop();
	saveSessions();
}

void SessionManager::onStreamingStarted(StreamTimer::Clock::time_point at)
{
	if (!streamSessionId_.isEmpty())
		return; // duplicate event: the session is already running
	closeSession(); // an offline "manual" session ends when the real stream begins
	openSession("stream", at);
	streamSessionId_ = openSessionId_;
	pendingTitle_.clear();
	emit sessionStateChanged();
	emit markersChanged();
}

void SessionManager::onStreamingStopped()
{
	if (streamSessionId_.isEmpty())
		return;
	closeSession();
	streamSessionId_.clear();
	emit sessionStateChanged();
}

void SessionManager::finalize()
{
	const bool wasOpen = !openSessionId_.isEmpty();
	closeSession();
	streamSessionId_.clear();
	saveMarkers();
	storage_.saveSettings(settings_);
	if (wasOpen)
		emit sessionStateChanged();
}

// ---- markers ----------------------------------------------------------------

SessionManager::RecordResult SessionManager::record(const Category &category, const QString &notes)
{
	RecordResult r;
	const auto clickTime = StreamTimer::Clock::now(); // sample the monotonic clock first
	const QDateTime wall = QDateTime::currentDateTime();

	if (!timer_.running()) {
		if (!settings_.allowOfflineMarkers) {
			r.status = RecordStatus::NotStreaming;
			return r;
		}
		openSession("manual", clickTime);
		emit sessionStateChanged();
	}

	qint64 secs = timer_.elapsedSeconds(clickTime) + settings_.offsetSeconds;
	secs = std::max<qint64>(0, secs);

	Marker m;
	m.id = newId();
	m.sessionId = openSessionId_;
	m.categoryId = category.id;
	m.categoryName = category.name;
	m.elapsedSeconds = secs;
	m.elapsedFormatted = formatElapsed(secs);
	m.localDate = wall.toString("yyyy-MM-dd");
	m.localDateTime = localStr(wall);
	m.utcTimestamp = utcIso(wall);
	m.notes = notes;

	markers_.push_back(m);
	r.marker = m;
	if (!storage_.saveMarkers(markers_)) { // auto-save after every click
		r.status = RecordStatus::SaveFailed;
		emit saveFailed(storage_.lastError());
	} else {
		r.status = RecordStatus::Ok;
	}
	emit markerAdded(m);
	emit markersChanged();
	return r;
}

bool SessionManager::setNote(const QString &markerId, const QString &notes)
{
	for (Marker &m : markers_)
		if (m.id == markerId) {
			m.notes = notes;
			saveMarkers();
			emit markersChanged();
			return true;
		}
	return false;
}

bool SessionManager::deleteMarker(const QString &markerId)
{
	for (int i = 0; i < markers_.size(); ++i)
		if (markers_[i].id == markerId) {
			markers_.removeAt(i);
			saveMarkers();
			emit markersChanged();
			return true;
		}
	return false;
}

int SessionManager::clearMarkers(const QString &sessionId)
{
	storage_.backupMarkers(); // never destroy data without a copy
	const int before = markers_.size();
	markers_.erase(std::remove_if(markers_.begin(), markers_.end(),
				      [&](const Marker &m) { return m.sessionId == sessionId; }),
		       markers_.end());
	const int removed = before - markers_.size();
	if (removed > 0) {
		saveMarkers();
		emit markersChanged();
	}
	return removed;
}

bool SessionManager::deleteSession(const QString &sessionId)
{
	if (sessionId == openSessionId_)
		return false; // can't delete the live session
	const int n = sessions_.size();
	storage_.backupMarkers();
	sessions_.erase(std::remove_if(sessions_.begin(), sessions_.end(),
				       [&](const Session &s) { return s.id == sessionId; }),
			sessions_.end());
	if (sessions_.size() == n)
		return false;
	markers_.erase(std::remove_if(markers_.begin(), markers_.end(),
				      [&](const Marker &m) { return m.sessionId == sessionId; }),
		       markers_.end());
	saveSessions();
	saveMarkers();
	emit sessionStateChanged();
	emit markersChanged();
	return true;
}

// ---- queries ----------------------------------------------------------------

QVector<Marker> SessionManager::markersFor(const QString &sessionId) const
{
	QVector<Marker> out;
	for (const Marker &m : markers_)
		if (m.sessionId == sessionId)
			out.push_back(m);
	return out; // already in insertion (chronological) order
}

std::optional<Session> SessionManager::session(const QString &id) const
{
	for (const Session &s : sessions_)
		if (s.id == id)
			return s;
	return std::nullopt;
}

QString SessionManager::displaySessionId() const
{
	if (!openSessionId_.isEmpty())
		return openSessionId_;
	return sessions_.isEmpty() ? QString() : sessions_.last().id;
}

int SessionManager::markerCount(const QString &sessionId) const
{
	int n = 0;
	for (const Marker &m : markers_)
		if (m.sessionId == sessionId)
			++n;
	return n;
}

QString SessionManager::title() const
{
	if (!openSessionId_.isEmpty())
		if (auto s = session(openSessionId_))
			return s->title;
	return pendingTitle_;
}

void SessionManager::setTitle(const QString &t)
{
	if (openSessionId_.isEmpty()) {
		pendingTitle_ = t;
		return;
	}
	for (Session &s : sessions_)
		if (s.id == openSessionId_) {
			s.title = t.trimmed().isEmpty() ? s.title : t.trimmed();
			break;
		}
	saveSessions();
	emit sessionStateChanged();
}

bool SessionManager::renameSession(const QString &id, const QString &title)
{
	if (title.trimmed().isEmpty())
		return false;
	for (Session &s : sessions_)
		if (s.id == id) {
			s.title = title.trimmed();
			saveSessions();
			emit sessionStateChanged();
			return true;
		}
	return false;
}

void SessionManager::setSettings(const Settings &s)
{
	settings_ = s;
	storage_.saveSettings(settings_);
	emit sessionStateChanged();
}

void SessionManager::saveSessions()
{
	if (!storage_.saveSessions(sessions_))
		emit saveFailed(storage_.lastError());
}

void SessionManager::saveMarkers()
{
	if (!storage_.saveMarkers(markers_))
		emit saveFailed(storage_.lastError());
}

} // namespace sm
