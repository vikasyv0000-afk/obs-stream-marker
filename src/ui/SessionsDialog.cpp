#include "ui/SessionsDialog.h"

#include <QDateTime>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "core/ExportManager.h"

namespace sm {

namespace {
QTableWidgetItem *cell(const QString &t)
{
	auto *i = new QTableWidgetItem(t);
	i->setFlags(i->flags() & ~Qt::ItemIsEditable);
	return i;
}

QString safeFileName(QString s)
{
	for (QChar &c : s)
		if (QString("<>:\"/\\|?*").contains(c))
			c = '_';
	return s.trimmed();
}
} // namespace

SessionsDialog::SessionsDialog(SessionManager &sessions, QWidget *parent) : QDialog(parent), sessions_(sessions)
{
	setWindowTitle(tr("Stream Sessions"));
	resize(640, 520);

	auto *root = new QVBoxLayout(this);

	sessionTable_ = new QTableWidget(0, 5, this);
	sessionTable_->setHorizontalHeaderLabels({tr("Title"), tr("Started"), tr("Duration"), tr("Markers"), tr("Status")});
	sessionTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
	sessionTable_->setSelectionMode(QAbstractItemView::SingleSelection);
	sessionTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
	sessionTable_->verticalHeader()->hide();
	root->addWidget(sessionTable_, 3);

	markerTable_ = new QTableWidget(0, 5, this);
	markerTable_->setHorizontalHeaderLabels({tr("Stream time"), tr("Category"), tr("Actual time"), tr("UTC"), tr("Notes")});
	markerTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
	markerTable_->verticalHeader()->hide();
	markerTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
	root->addWidget(markerTable_, 4);

	auto *row = new QHBoxLayout;
	auto *rename = new QPushButton(tr("Rename"), this);
	auto *csv = new QPushButton(tr("Export CSV…"), this);
	auto *del = new QPushButton(tr("Delete session"), this);
	auto *close = new QPushButton(tr("Close"), this);
	row->addWidget(rename);
	row->addWidget(csv);
	row->addWidget(del);
	row->addStretch(1);
	row->addWidget(close);
	root->addLayout(row);

	connect(close, &QPushButton::clicked, this, &QDialog::accept);
	connect(sessionTable_, &QTableWidget::itemSelectionChanged, this, &SessionsDialog::refreshMarkers);

	connect(rename, &QPushButton::clicked, this, [this] {
		const auto s = sessions_.session(selectedId());
		if (!s)
			return;
		bool ok = false;
		const QString t = QInputDialog::getText(this, tr("Rename session"), tr("Title"), QLineEdit::Normal, s->title, &ok);
		if (!ok || t.trimmed().isEmpty())
			return;
		sessions_.renameSession(s->id, t);
		refreshSessions();
	});

	connect(csv, &QPushButton::clicked, this, [this] {
		const auto s = sessions_.session(selectedId());
		if (!s)
			return;
		const QString def = safeFileName(s->title) + ".csv";
		const QString path = QFileDialog::getSaveFileName(this, tr("Export CSV"), def, tr("CSV (*.csv)"));
		if (path.isEmpty())
			return;
		const QString err = ExportManager(sessions_).exportCsv(path, {s->id});
		if (!err.isEmpty())
			QMessageBox::warning(this, windowTitle(), tr("Export failed: %1").arg(err));
	});

	connect(del, &QPushButton::clicked, this, [this] {
		const auto s = sessions_.session(selectedId());
		if (!s)
			return;
		if (s->id == sessions_.activeSessionId()) {
			QMessageBox::information(this, windowTitle(), tr("The live session can't be deleted."));
			return;
		}
		const int n = sessions_.markerCount(s->id);
		if (QMessageBox::question(this, tr("Delete session"),
					  tr("Delete \"%1\" and its %2 markers?\nA backup of markers.json is made first.")
						  .arg(s->title)
						  .arg(n),
					  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
			sessions_.deleteSession(s->id);
			refreshSessions();
		}
	});

	refreshSessions();
}

QString SessionsDialog::selectedId() const
{
	const auto rows = sessionTable_->selectionModel()->selectedRows();
	if (rows.isEmpty())
		return {};
	return sessionTable_->item(rows.first().row(), 0)->data(Qt::UserRole).toString();
}

void SessionsDialog::refreshSessions()
{
	sessionTable_->setRowCount(0);
	const auto &all = sessions_.sessions();
	for (int i = all.size() - 1; i >= 0; --i) { // newest first
		const Session &s = all[i];
		const int r = sessionTable_->rowCount();
		sessionTable_->insertRow(r);

		QString duration = QStringLiteral("-");
		const QDateTime a = QDateTime::fromString(s.startedUtc, Qt::ISODateWithMs);
		const QDateTime b = s.isOpen() ? QDateTime::currentDateTimeUtc() : QDateTime::fromString(s.endedUtc, Qt::ISODateWithMs);
		if (a.isValid() && b.isValid())
			duration = formatElapsed(a.secsTo(b));

		QString status = s.isOpen() ? tr("Live") : (s.interrupted ? tr("Interrupted") : tr("Ended"));
		if (s.kind == "manual")
			status += tr(" (offline)");

		auto *t = cell(s.title);
		t->setData(Qt::UserRole, s.id);
		sessionTable_->setItem(r, 0, t);
		sessionTable_->setItem(r, 1, cell(s.startedLocal));
		sessionTable_->setItem(r, 2, cell(duration));
		sessionTable_->setItem(r, 3, cell(QString::number(sessions_.markerCount(s.id))));
		sessionTable_->setItem(r, 4, cell(status));
	}
	if (sessionTable_->rowCount() > 0)
		sessionTable_->selectRow(0);
	else
		refreshMarkers();
}

void SessionsDialog::refreshMarkers()
{
	markerTable_->setRowCount(0);
	for (const Marker &m : sessions_.markersFor(selectedId())) {
		const int r = markerTable_->rowCount();
		markerTable_->insertRow(r);
		markerTable_->setItem(r, 0, cell(m.elapsedFormatted));
		markerTable_->setItem(r, 1, cell(m.categoryName));
		markerTable_->setItem(r, 2, cell(m.localDateTime));
		markerTable_->setItem(r, 3, cell(m.utcTimestamp));
		markerTable_->setItem(r, 4, cell(m.notes));
	}
}

} // namespace sm
