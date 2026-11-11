#pragma once
// Browse previous stream sessions and their markers; export or delete them.

#include <QDialog>

#include "core/SessionManager.h"

class QTableWidget;

namespace sm {

class SessionsDialog : public QDialog {
	Q_OBJECT
public:
	SessionsDialog(SessionManager &sessions, QWidget *parent = nullptr);

private:
	void refreshSessions();
	void refreshMarkers();
	QString selectedId() const;

	SessionManager &sessions_;
	QTableWidget *sessionTable_;
	QTableWidget *markerTable_;
};

} // namespace sm
