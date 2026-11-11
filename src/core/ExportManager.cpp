#include "core/ExportManager.h"

#include <QDateTime>
#include <algorithm>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

namespace sm {

QString ExportManager::csvEscape(const QString &cell)
{
	QString c = cell;
	if (!c.isEmpty()) {
		const QChar f = c.at(0);
		if (f == '=' || f == '+' || f == '-' || f == '@' || f == '\t' || f == '\r')
			c.prepend('\''); // stop Excel/Sheets from evaluating it as a formula
	}
	if (c.contains(',') || c.contains('"') || c.contains('\n') || c.contains('\r')) {
		c.replace('"', "\"\"");
		c = '"' + c + '"';
	}
	return c;
}

QVector<Session> ExportManager::select(const QStringList &ids) const
{
	QVector<Session> out;
	for (const Session &s : sessions_.sessions())
		if (ids.isEmpty() || ids.contains(s.id))
			out.push_back(s);
	std::stable_sort(out.begin(), out.end(),
			 [](const Session &a, const Session &b) { return a.startedUtc < b.startedUtc; });
	return out;
}

QByteArray ExportManager::buildCsv(const QStringList &sessionIds) const
{
	QString text;
	text += "Stream ID,Stream Title,Category,Stream Time,Elapsed Seconds,Date,Actual DateTime,UTC Timestamp,Notes\r\n";
	for (const Session &s : select(sessionIds)) {
		for (const Marker &m : sessions_.markersFor(s.id)) {
			const QStringList row{s.id,
					      s.title,
					      m.categoryName,
					      m.elapsedFormatted,
					      QString::number(m.elapsedSeconds),
					      m.localDate,
					      m.localDateTime,
					      m.utcTimestamp,
					      m.notes};
			QStringList cells;
			for (const QString &c : row)
				cells << csvEscape(c);
			text += cells.join(',') + "\r\n";
		}
	}
	return QByteArray("\xEF\xBB\xBF") + text.toUtf8();
}

namespace {
QString writeAtomic(const QString &path, const QByteArray &data)
{
	QSaveFile f(path);
	if (!f.open(QIODevice::WriteOnly))
		return f.errorString();
	f.write(data);
	if (!f.commit())
		return f.errorString();
	return {};
}
} // namespace

QString ExportManager::exportCsv(const QString &path, const QStringList &sessionIds) const
{
	return writeAtomic(path, buildCsv(sessionIds));
}

QString ExportManager::exportJson(const QString &path, const QStringList &sessionIds) const
{
	QJsonArray streams;
	for (const Session &s : select(sessionIds)) {
		QJsonObject o = s.toJson();
		QJsonArray markers;
		for (const Marker &m : sessions_.markersFor(s.id))
			markers.append(m.toJson());
		o["markers"] = markers;
		streams.append(o);
	}
	QJsonObject root{{"version", 1},
			 {"exportedUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
			 {"streams", streams}};
	return writeAtomic(path, QJsonDocument(root).toJson(QJsonDocument::Indented));
}

} // namespace sm
