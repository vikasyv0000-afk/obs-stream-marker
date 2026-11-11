#pragma once
// CSV / JSON export. CSV is UTF-8 with BOM + CRLF so Excel and Google Sheets
// open it cleanly; cells starting with = + - @ are neutralised against
// spreadsheet formula injection.

#include <QString>
#include <QStringList>

#include "core/SessionManager.h"

namespace sm {

class ExportManager {
public:
	explicit ExportManager(const SessionManager &sessions) : sessions_(sessions) {}

	// Empty sessionIds = every session. Returns an empty string on success, else an error message.
	QString exportCsv(const QString &path, const QStringList &sessionIds) const;
	QString exportJson(const QString &path, const QStringList &sessionIds) const;

	// Exposed for tests.
	QByteArray buildCsv(const QStringList &sessionIds) const;
	static QString csvEscape(const QString &cell);

private:
	QVector<Session> select(const QStringList &ids) const;
	const SessionManager &sessions_;
};

} // namespace sm
