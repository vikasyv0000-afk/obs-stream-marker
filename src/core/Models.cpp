#include "core/Models.h"

#include <QUuid>
#include <algorithm>

namespace sm {

QString newId()
{
	return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString formatElapsed(qint64 seconds)
{
	if (seconds < 0)
		seconds = 0;
	const qint64 h = seconds / 3600;
	const qint64 m = (seconds % 3600) / 60;
	const qint64 s = seconds % 60;
	return QStringLiteral("%1:%2:%3")
		.arg(h, 2, 10, QLatin1Char('0'))
		.arg(m, 2, 10, QLatin1Char('0'))
		.arg(s, 2, 10, QLatin1Char('0'));
}

// ---- Category -------------------------------------------------------------

QJsonObject Category::toJson() const
{
	return QJsonObject{{"id", id}, {"name", name}, {"color", color}, {"icon", icon}};
}

Category Category::fromJson(const QJsonObject &o, bool *ok)
{
	Category c;
	c.id = o.value("id").toString();
	c.name = o.value("name").toString();
	c.color = o.value("color").toString("#CCCCCC");
	c.icon = o.value("icon").toString();
	if (c.id.isEmpty())
		c.id = newId();
	if (ok)
		*ok = !c.name.isEmpty();
	return c;
}

// ---- Session --------------------------------------------------------------

QJsonObject Session::toJson() const
{
	return QJsonObject{{"id", id},
			   {"title", title},
			   {"kind", kind},
			   {"startedUtc", startedUtc},
			   {"startedLocal", startedLocal},
			   {"endedUtc", endedUtc},
			   {"interrupted", interrupted}};
}

Session Session::fromJson(const QJsonObject &o, bool *ok)
{
	Session s;
	s.id = o.value("id").toString();
	s.title = o.value("title").toString();
	s.kind = o.value("kind").toString("stream");
	s.startedUtc = o.value("startedUtc").toString();
	s.startedLocal = o.value("startedLocal").toString();
	s.endedUtc = o.value("endedUtc").toString();
	s.interrupted = o.value("interrupted").toBool(false);
	if (ok)
		*ok = !s.id.isEmpty();
	return s;
}

// ---- Marker ---------------------------------------------------------------

QJsonObject Marker::toJson() const
{
	return QJsonObject{{"id", id},
			   {"sessionId", sessionId},
			   {"categoryId", categoryId},
			   {"categoryName", categoryName},
			   {"elapsedSeconds", static_cast<double>(elapsedSeconds)},
			   {"elapsedFormatted", elapsedFormatted},
			   {"localDate", localDate},
			   {"localDateTime", localDateTime},
			   {"utcTimestamp", utcTimestamp},
			   {"notes", notes}};
}

Marker Marker::fromJson(const QJsonObject &o, bool *ok)
{
	Marker m;
	m.id = o.value("id").toString();
	m.sessionId = o.value("sessionId").toString();
	m.categoryId = o.value("categoryId").toString();
	m.categoryName = o.value("categoryName").toString();
	m.elapsedSeconds = static_cast<qint64>(o.value("elapsedSeconds").toDouble());
	m.elapsedFormatted = o.value("elapsedFormatted").toString(formatElapsed(m.elapsedSeconds));
	m.localDate = o.value("localDate").toString();
	m.localDateTime = o.value("localDateTime").toString();
	m.utcTimestamp = o.value("utcTimestamp").toString();
	m.notes = o.value("notes").toString();
	if (ok)
		*ok = !m.id.isEmpty() && !m.sessionId.isEmpty();
	return m;
}

// ---- Settings -------------------------------------------------------------

QJsonObject Settings::toJson() const
{
	return QJsonObject{{"allowOfflineMarkers", allowOfflineMarkers},
			   {"offsetSeconds", offsetSeconds},
			   {"promptForNote", promptForNote},
			   {"recentCount", recentCount},
			   {"trackRecording", trackRecording},
			   {"trackVirtualCam", trackVirtualCam},
			   {"soundOnMarker", soundOnMarker},
			   {"toastOnMarker", toastOnMarker}};
}

Settings Settings::fromJson(const QJsonObject &o)
{
	Settings s;
	s.allowOfflineMarkers = o.value("allowOfflineMarkers").toBool(false);
	s.offsetSeconds = std::clamp(o.value("offsetSeconds").toInt(0), -3600, 3600);
	s.promptForNote = o.value("promptForNote").toBool(false);
	s.recentCount = std::clamp(o.value("recentCount").toInt(50), 5, 500);
	s.trackRecording = o.value("trackRecording").toBool(true);
	s.trackVirtualCam = o.value("trackVirtualCam").toBool(true);
	s.soundOnMarker = o.value("soundOnMarker").toBool(true);
	s.toastOnMarker = o.value("toastOnMarker").toBool(false);
	return s;
}

} // namespace sm
