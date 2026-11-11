#include "ui/MarkerDock.h"

#include <QDate>
#include <QFileDialog>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "core/ExportManager.h"
#include "ui/CategoryDialogs.h"
#include "ui/MarkerButton.h"
#include "ui/MarkerToast.h"
#include "ui/SessionsDialog.h"
#include "ui/SettingsDialog.h"
#include "ui/Theme.h"

namespace sm {

namespace {
QString safeFileName(QString s)
{
	for (QChar &c : s)
		if (QString("<>:\"/\\|?*").contains(c))
			c = '_';
	s = s.trimmed();
	return s.isEmpty() ? QStringLiteral("stream") : s;
}

QString dockStyle()
{
	using namespace theme;
	return QStringLiteral(
		       "sm--MarkerDock{background:%1;}"
		       "QLabel{color:%2;background:transparent;}"
		       "QLineEdit{background:#222221;color:%2;border:1px solid %3;border-radius:8px;padding:5px 8px;}"
		       "QLineEdit:focus{border:1px solid #3987e5;}"
		       "QPushButton,QToolButton{background:#222221;color:%2;border:1px solid %3;border-radius:10px;"
		       "padding:7px 10px;font-weight:600;}"
		       "QPushButton:hover,QToolButton:hover{background:#2c2c2a;}"
		       "QPushButton:pressed,QToolButton:pressed{background:#1a1a19;}"
		       "QToolButton::menu-indicator{image:none;}"
		       "QListWidget{background:%1;border:1px solid %4;border-radius:12px;outline:0;}"
		       "QListWidget::item{border-bottom:1px solid %4;}"
		       "QListWidget::item:selected{background:#222221;}"
		       "QScrollArea{background:transparent;border:none;}")
		.arg(kSurface, kInk, kBorder, kHair);
}
} // namespace

MarkerDock::MarkerDock(SessionManager *sessions, CategoryManager *cats, QWidget *parent)
	: QWidget(parent), sessions_(sessions), cats_(cats)
{
	setMinimumWidth(200);
	setAttribute(Qt::WA_StyledBackground, true);
	setStyleSheet(dockStyle());

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(6, 6, 6, 6);
	root->setSpacing(6);

	// ---- slim status bar: [LIVE] [timer] ............ [menu] ----
	auto *header = new QHBoxLayout;
	header->setSpacing(6);
	statusLabel_ = new QLabel(this);
	header->addWidget(statusLabel_);
	timerLabel_ = new QLabel(QStringLiteral("--:--:--"), this);
	timerLabel_->setStyleSheet("font-size:17px;font-weight:700;");
	header->addWidget(timerLabel_);
	header->addStretch(1);

	auto *menuBtn = new QToolButton(this);
	menuBtn->setText(QString::fromUtf8("\u22EF"));
	menuBtn->setToolTip(tr("Menu"));
	menuBtn->setPopupMode(QToolButton::InstantPopup);
	menuBtn->setFixedSize(34, 30);
	menuBtn->setStyleSheet("QToolButton{padding:0;font-size:18px;}");
	auto *menu = new QMenu(menuBtn);
	menu->addAction(tr("Add category..."), this, &MarkerDock::addCategory);
	menu->addAction(tr("Edit categories..."), this, [this] {
		if (cats_)
			CategoryListDialog(*cats_, this).exec();
	});
	menu->addSeparator();
	menu->addAction(tr("Stream title..."), this, &MarkerDock::setStreamTitle);
	menu->addAction(tr("Previous streams..."), this, [this] {
		if (sessions_)
			SessionsDialog(*sessions_, this).exec();
	});
	auto *exportMenu = menu->addMenu(tr("Export"));
	exportMenu->addAction(tr("Current stream (CSV)..."), this, [this] { exportCsv(false); });
	exportMenu->addAction(tr("All streams (CSV)..."), this, [this] { exportCsv(true); });
	exportMenu->addAction(tr("All markers (JSON)..."), this, [this] { exportJson(); });
	menu->addAction(tr("Clear history..."), this, &MarkerDock::clearHistory);
	menu->addSeparator();
	menu->addAction(tr("Settings..."), this, [this] {
		if (sessions_ && cats_)
			SettingsDialog(*sessions_, *cats_, this).exec();
	});
	menuBtn->setMenu(menu);
	header->addWidget(menuBtn);
	root->addLayout(header);

	// ---- category buttons ----
	scroll_ = new QScrollArea(this);
	scroll_->setWidgetResizable(true);
	scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	gridHost_ = new QWidget;
	gridHost_->setStyleSheet("background:transparent;");
	grid_ = new QGridLayout(gridHost_);
	grid_->setContentsMargins(0, 0, 0, 0);
	grid_->setSpacing(6);
	scroll_->setWidget(gridHost_);
	root->addWidget(scroll_);

	// ---- last marker + count + undo ----
	auto *info = new QHBoxLayout;
	info->setSpacing(6);
	messageLabel_ = new QLabel(this);
	messageLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
	info->addWidget(messageLabel_, 1);
	countLabel_ = new QLabel(this);
	countLabel_->setStyleSheet(QStringLiteral("color:%1;").arg(theme::kInk2));
	info->addWidget(countLabel_);
	undoBtn_ = new QPushButton(tr("Undo"), this);
	undoBtn_->setToolTip(tr("Remove the most recent marker"));
	undoBtn_->setStyleSheet("QPushButton{padding:3px 8px;border-radius:8px;}");
	info->addWidget(undoBtn_);
	root->addLayout(info);

	// ---- recent markers (compact one-line rows) ----
	history_ = new QListWidget(this);
	history_->setSelectionMode(QAbstractItemView::SingleSelection);
	history_->setContextMenuPolicy(Qt::CustomContextMenu);
	history_->setUniformItemSizes(true);
	history_->setMinimumHeight(60);
	history_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	root->addWidget(history_, 1);

	// Credit / watermark
	auto *credit = new QLabel(this);
	credit->setTextFormat(Qt::RichText);
	credit->setOpenExternalLinks(true);
	credit->setAlignment(Qt::AlignCenter);
	credit->setText(QStringLiteral("<span style='color:%1'>%2 </span><a href='https://instagram.com/veekaxx' "
				       "style='color:%3;text-decoration:none;font-weight:600'>@veekaxx</a>")
				.arg(theme::kMuted, tr("Made by veekaxx on Instagram -"), theme::kInk2));
	credit->setToolTip(QStringLiteral("https://instagram.com/veekaxx"));
	credit->setStyleSheet("font-size:10px;");
	root->addWidget(credit);

	// ---- wiring ----
	connect(undoBtn_, &QPushButton::clicked, this, &MarkerDock::undoLast);
	connect(history_, &QListWidget::itemDoubleClicked, this,
		[this](QListWidgetItem *it) { editNote(it->data(Qt::UserRole).toString()); });
	connect(history_, &QListWidget::customContextMenuRequested, this, [this](const QPoint &p) {
		QListWidgetItem *it = history_->itemAt(p);
		if (!it || !sessions_)
			return;
		const QString id = it->data(Qt::UserRole).toString();
		QMenu m(this);
		m.addAction(tr("Edit note..."), this, [this, id] { editNote(id); });
		m.addAction(tr("Delete marker"), this, [this, id] { sessions_->deleteMarker(id); });
		m.exec(history_->viewport()->mapToGlobal(p));
	});

	if (cats_) {
		connect(cats_, &CategoryManager::changed, this, &MarkerDock::rebuildButtons);
		connect(cats_, &CategoryManager::changed, this, &MarkerDock::refreshHistory);
	}
	if (sessions_) {
		connect(sessions_, &SessionManager::markersChanged, this, [this] {
			refreshHistory();
			refreshStatus();
		});
		connect(sessions_, &SessionManager::sessionStateChanged, this, [this] {
			refreshStatus();
			refreshHistory();
		});
		connect(sessions_, &SessionManager::saveFailed, this, [this](const QString &err) {
			messageLabel_->setStyleSheet(QStringLiteral("color:%1;").arg(theme::kCritical));
			showMessage(tr("Could not save: %1").arg(err));
		});
	}

	tick_ = new QTimer(this);
	tick_->setInterval(1000);
	connect(tick_, &QTimer::timeout, this, &MarkerDock::refreshStatus);
	connect(tick_, &QTimer::timeout, this, &MarkerDock::refreshHotkeyBadges);
	tick_->start();

	messageTimer_ = new QTimer(this);
	messageTimer_->setSingleShot(true);
	connect(messageTimer_, &QTimer::timeout, this, [this] {
		messageLabel_->setStyleSheet(QStringLiteral("color:%1;").arg(theme::kInk2));
		messageLabel_->setText(lastText_);
	});

	columns_ = columnsForWidth();
	rebuildButtons();
	refreshStatus();
	refreshHistory();
}

void MarkerDock::setHotkeyLabelProvider(std::function<QString(const QString &)> fn)
{
	hotkeyLabel_ = std::move(fn);
	refreshHotkeyBadges();
}

void MarkerDock::refreshHotkeyBadges()
{
	if (!hotkeyLabel_)
		return;
	for (MarkerButton *b : buttons_)
		b->setHotkeyText(hotkeyLabel_(b->category().id));
}

int MarkerDock::columnsForWidth() const
{
	return width() < 250 ? 1 : 2;
}

void MarkerDock::resizeEvent(QResizeEvent *e)
{
	QWidget::resizeEvent(e);
	const int c = columnsForWidth();
	if (c != columns_) {
		columns_ = c;
		rebuildButtons();
	}
}

QColor MarkerDock::colorOf(const QString &categoryId) const
{
	if (cats_)
		if (auto c = cats_->byId(categoryId))
			return QColor(c->color);
	return QColor(theme::kMuted);
}

void MarkerDock::rebuildButtons()
{
	if (!cats_)
		return;
	for (MarkerButton *b : buttons_) {
		grid_->removeWidget(b);
		b->deleteLater();
	}
	buttons_.clear();
	while (QLayoutItem *it = grid_->takeAt(0)) { // leftover placeholder label
		if (QWidget *w = it->widget())
			w->deleteLater();
		delete it;
	}

	const auto &cats = cats_->categories();
	if (cats.isEmpty()) {
		auto *l = new QLabel(tr("No categories yet.\nClick \"+ Add Category\"."), gridHost_);
		l->setAlignment(Qt::AlignCenter);
		grid_->addWidget(l, 0, 0, 1, columns_);
		scroll_->setFixedHeight(48);
		return;
	}

	const bool enabled = sessions_ && (sessions_->sessionOpen() || sessions_->settings().allowOfflineMarkers);
	for (int i = 0; i < cats.size(); ++i) {
		auto *b = new MarkerButton(cats[i], gridHost_);
		b->setEnabled(enabled);
		const QString id = cats[i].id;
		connect(b, &QPushButton::clicked, this, [this, id] { onCategoryClicked(id); });
		connect(b, &MarkerButton::editRequested, this, &MarkerDock::editCategory);
		connect(b, &MarkerButton::deleteRequested, this, &MarkerDock::deleteCategory);
		connect(b, &MarkerButton::moveRequested, this, [this](const QString &cid, int delta) {
			const int idx = cats_->indexOf(cid);
			cats_->move(idx, idx + delta);
		});
		grid_->addWidget(b, i / columns_, i % columns_);
		buttons_.push_back(b);
	}
	for (int c = 0; c < columns_; ++c)
		grid_->setColumnStretch(c, 1);

	// Show up to 3 rows without scrolling; more rows scroll inside this area.
	const int rows = (cats.size() + columns_ - 1) / columns_;
	scroll_->setFixedHeight(qMin(rows, 4) * 50 - 6 + 2);
	refreshHotkeyBadges();
}

void MarkerDock::triggerCategory(const QString &categoryId)
{
	onCategoryClicked(categoryId);
}

void MarkerDock::onCategoryClicked(const QString &categoryId)
{
	if (!sessions_ || !cats_)
		return;
	const auto cat = cats_->byId(categoryId);
	if (!cat)
		return;

	const auto res = sessions_->record(*cat); // saved to disk before returning
	if (res.status == SessionManager::RecordStatus::NotStreaming) {
		messageLabel_->setStyleSheet(QStringLiteral("color:%1;").arg(theme::kWarning));
		showMessage(tr("Not streaming - start your stream to record markers."));
		return;
	}
	for (MarkerButton *b : buttons_)
		if (b->category().id == categoryId)
			b->flash();
	if (res.status == SessionManager::RecordStatus::Ok) {
		lastText_ = tr("Last: %1 %2").arg(res.marker.categoryName, res.marker.elapsedFormatted);
		messageLabel_->setStyleSheet(QStringLiteral("color:%1;").arg(theme::kLive));
		messageLabel_->setText(lastText_);
		messageTimer_->start(2500);
		const Settings &st = sessions_->settings();
		if (st.soundOnMarker)
			MarkerFeedback::beep(cats_->indexOf(categoryId));
		if (st.toastOnMarker)
			MarkerFeedback::toast(cat->icon + QStringLiteral("  ") + cat->name + QStringLiteral("  ") +
						      res.marker.elapsedFormatted,
					      QColor(cat->color));
	}
	if (sessions_->settings().promptForNote) {
		bool ok = false;
		const QString note = QInputDialog::getText(this, tr("Marker note"),
							   tr("Note for \"%1\" at %2:")
								   .arg(res.marker.categoryName, res.marker.elapsedFormatted),
							   QLineEdit::Normal, QString(), &ok);
		if (ok && !note.trimmed().isEmpty())
			sessions_->setNote(res.marker.id, note.trimmed());
	}
}

void MarkerDock::showMessage(const QString &text)
{
	messageLabel_->setText(messageLabel_->fontMetrics().elidedText(text, Qt::ElideRight, qMax(60, messageLabel_->width())));
	messageLabel_->setToolTip(text);
	messageTimer_->start(4000);
}

void MarkerDock::refreshStatus()
{
	if (!sessions_)
		return;
	const bool live = sessions_->streaming();
	const bool open = sessions_->sessionOpen();

	QString pill;
	if (live)
		pill = QStringLiteral("<span style='color:%1'>&#9679;</span>&nbsp;<b>%2</b>").arg(theme::kLive, tr("LIVE"));
	else if (open)
		pill = QStringLiteral("<span style='color:%1'>&#9679;</span>&nbsp;<b>%2</b>").arg(theme::kWarning, tr("OFFLINE"));
	else
		pill = QStringLiteral("<span style='color:%1'>&#9679;</span>&nbsp;<b>%2</b>").arg(theme::kMuted, tr("IDLE"));
	statusLabel_->setTextFormat(Qt::RichText);
	statusLabel_->setStyleSheet(QStringLiteral("QLabel{background:%1;border:1px solid %2;border-radius:14px;padding:2px 9px;font-size:11px;}")
					    .arg(live ? "#0f2a12" : "#222221", live ? "#0c6a0c" : theme::kBorder));
	if (statusLabel_->text() != pill)
		statusLabel_->setText(pill);

	const QString t = open ? formatElapsed(sessions_->elapsedSeconds()) : QStringLiteral("--:--:--");
	if (timerLabel_->text() != t)
		timerLabel_->setText(t);

	const bool enabled = open || sessions_->settings().allowOfflineMarkers;
	for (MarkerButton *b : buttons_)
		if (b->isEnabled() != enabled)
			b->setEnabled(enabled);

	const QString sid = sessions_->displaySessionId();
	countLabel_->setTextFormat(Qt::RichText);
	countLabel_->setText(tr("<b>%1</b> markers").arg(sid.isEmpty() ? 0 : sessions_->markerCount(sid)));
	undoBtn_->setEnabled(!sid.isEmpty() && sessions_->markerCount(sid) > 0);
}

void MarkerDock::refreshHistory()
{
	if (!sessions_)
		return;
	history_->clear();
	const QString sid = sessions_->displaySessionId();
	const QVector<Marker> all = sessions_->markersFor(sid);
	const int limit = sessions_->settings().recentCount;

	if (all.isEmpty()) {
		auto *item = new QListWidgetItem(history_);
		item->setFlags(Qt::NoItemFlags);
		auto *empty = new QLabel(tr("No markers yet"), history_);
		empty->setAlignment(Qt::AlignCenter);
		empty->setStyleSheet(QStringLiteral("color:%1;").arg(theme::kMuted));
		item->setSizeHint(QSize(0, 40));
		history_->setItemWidget(item, empty);
		return;
	}

	for (int i = all.size() - 1, shown = 0; i >= 0 && shown < limit; --i, ++shown) { // newest first
		const Marker &m = all[i];
		auto *item = new QListWidgetItem(history_);
		item->setData(Qt::UserRole, m.id);
		item->setToolTip(m.notes.isEmpty() ? tr("Double-click to add a note") : m.notes);

		auto *row = new QWidget;
		auto *l = new QHBoxLayout(row);
		l->setContentsMargins(6, 2, 4, 2);
		l->setSpacing(6);

		auto *actual = new QLabel(m.localDateTime.mid(11), row); // HH:mm:ss
		actual->setStyleSheet(QStringLiteral("color:%1;font-size:11px;").arg(theme::kInk2));
		auto *elapsed = new QLabel(m.elapsedFormatted, row);
		elapsed->setStyleSheet("font-weight:700;font-size:11px;");

		const QColor col = colorOf(m.categoryId);
		auto *chip = new QLabel(row);
		chip->setTextFormat(Qt::RichText);
		chip->setText(QStringLiteral("<span style='color:%1'>&#9679;</span> %2%3")
				      .arg(col.name(), m.categoryName.toHtmlEscaped(), m.notes.isEmpty() ? QString() : QStringLiteral(" *")));
		chip->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		chip->setStyleSheet("font-size:11px;font-weight:600;");

		auto *del = new QPushButton(QString::fromUtf8("\u2715"), row);
		del->setFixedSize(20, 20);
		del->setToolTip(tr("Delete this marker"));
		del->setStyleSheet("QPushButton{padding:0;border-radius:6px;color:#e66767;background:transparent;border:none;}"
				   "QPushButton:hover{background:#3a2020;}");
		const QString id = m.id;
		connect(del, &QPushButton::clicked, this, [this, id] {
			if (sessions_)
				sessions_->deleteMarker(id);
		});

		l->addWidget(actual);
		l->addWidget(elapsed);
		l->addWidget(chip, 1);
		l->addWidget(del);
		item->setSizeHint(QSize(0, 26));
		history_->setItemWidget(item, row);
	}
}

// ---- category actions --------------------------------------------------------

void MarkerDock::addCategory()
{
	if (!cats_)
		return;
	CategoryEditDialog d(Category{}, true, this);
	if (d.exec() == QDialog::Accepted) {
		const Category c = d.result();
		cats_->add(c.name, c.color, c.icon);
	}
}

void MarkerDock::editCategory(const QString &id)
{
	if (!cats_)
		return;
	if (auto c = cats_->byId(id)) {
		CategoryEditDialog d(*c, false, this);
		if (d.exec() == QDialog::Accepted)
			cats_->update(d.result());
	}
}

void MarkerDock::deleteCategory(const QString &id)
{
	if (!cats_)
		return;
	if (auto c = cats_->byId(id))
		if (confirmDeleteCategory(this, *c))
			cats_->remove(id);
}

// ---- history actions -----------------------------------------------------------

void MarkerDock::editNote(const QString &markerId)
{
	if (!sessions_)
		return;
	for (const Marker &m : sessions_->allMarkers())
		if (m.id == markerId) {
			bool ok = false;
			const QString note = QInputDialog::getText(this, tr("Marker note"),
								   tr("%1 at %2").arg(m.categoryName, m.elapsedFormatted),
								   QLineEdit::Normal, m.notes, &ok);
			if (ok)
				sessions_->setNote(markerId, note.trimmed());
			return;
		}
}

void MarkerDock::undoLast()
{
	if (!sessions_)
		return;
	const QVector<Marker> all = sessions_->markersFor(sessions_->displaySessionId());
	if (all.isEmpty())
		return;
	const Marker last = all.last();
	sessions_->deleteMarker(last.id);
	lastText_.clear();
	messageLabel_->setStyleSheet(QStringLiteral("color:%1;").arg(theme::kInk2));
	showMessage(tr("Removed %1 %2").arg(last.categoryName, last.elapsedFormatted));
}

void MarkerDock::setStreamTitle()
{
	if (!sessions_)
		return;
	bool ok = false;
	const QString t = QInputDialog::getText(this, tr("Stream title"), tr("Title (used in exports):"), QLineEdit::Normal,
						sessions_->title(), &ok);
	if (ok)
		sessions_->setTitle(t);
}

void MarkerDock::clearHistory()
{
	if (!sessions_)
		return;
	const QString sid = sessions_->displaySessionId();
	const int n = sessions_->markerCount(sid);
	if (sid.isEmpty() || n == 0) {
		showMessage(tr("Nothing to clear."));
		return;
	}
	const auto s = sessions_->session(sid);
	if (QMessageBox::question(this, tr("Clear History"),
				  tr("Delete all %1 markers of \"%2\"?\n\nA backup of markers.json is saved first.")
					  .arg(n)
					  .arg(s ? s->title : QString()),
				  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
		sessions_->clearMarkers(sid);
}

// ---- export ----------------------------------------------------------------------

void MarkerDock::exportCsv(bool allStreams)
{
	if (!sessions_)
		return;
	QStringList ids;
	QString name = QStringLiteral("all-streams");
	if (!allStreams) {
		const QString sid = sessions_->displaySessionId();
		if (sid.isEmpty()) {
			showMessage(tr("No stream to export yet."));
			return;
		}
		ids << sid;
		name = safeFileName(sessions_->session(sid)->title);
	}
	const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
	const QString path = QFileDialog::getSaveFileName(this, tr("Export CSV"), dir + "/" + name + "-markers.csv",
							  tr("CSV (*.csv)"));
	if (path.isEmpty())
		return;
	const QString err = ExportManager(*sessions_).exportCsv(path, ids);
	if (err.isEmpty()) {
		messageLabel_->setStyleSheet(QStringLiteral("color:%1;").arg(theme::kLive));
		showMessage(tr("Exported to %1").arg(path));
	} else {
		QMessageBox::warning(this, tr("Export failed"), err);
	}
}

void MarkerDock::exportJson()
{
	if (!sessions_)
		return;
	const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
	const QString path = QFileDialog::getSaveFileName(this, tr("Export markers (JSON)"), dir + "/markers-export.json",
							  tr("JSON (*.json)"));
	if (path.isEmpty())
		return;
	const QString err = ExportManager(*sessions_).exportJson(path, {});
	if (err.isEmpty()) {
		messageLabel_->setStyleSheet(QStringLiteral("color:%1;").arg(theme::kLive));
		showMessage(tr("Exported to %1").arg(path));
	} else {
		QMessageBox::warning(this, tr("Export failed"), err);
	}
}

} // namespace sm
