#pragma once
// The "Stream Marker" dock: category button grid, stream status, history, tools.

#include <QColor>
#include <QPointer>
#include <functional>
#include <QWidget>

#include "core/CategoryManager.h"
#include "core/SessionManager.h"

class QGridLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QScrollArea;
class QTimer;

namespace sm {

class MarkerButton;

class MarkerDock : public QWidget {
	Q_OBJECT
public:
	MarkerDock(SessionManager *sessions, CategoryManager *cats, QWidget *parent = nullptr);

	// Optional: returns the OBS hotkey text for a category ("F8"), shown as a badge on its button.
	void setHotkeyLabelProvider(std::function<QString(const QString &)> fn);

public slots:
	void triggerCategory(const QString &categoryId); // same as clicking the button (used by hotkeys)

protected:
	void resizeEvent(QResizeEvent *e) override;

private slots:
	void rebuildButtons();
	void refreshHistory();
	void refreshStatus();
	void onCategoryClicked(const QString &categoryId);
	void refreshHotkeyBadges();

private:
	void addCategory();
	void editCategory(const QString &id);
	void deleteCategory(const QString &id);
	void editNote(const QString &markerId);
	void clearHistory();
	void undoLast();
	void setStreamTitle();
	QColor colorOf(const QString &categoryId) const;
	void exportCsv(bool allStreams);
	void exportJson();
	void showMessage(const QString &text);
	int columnsForWidth() const;

	QPointer<SessionManager> sessions_;
	QPointer<CategoryManager> cats_;

	QScrollArea *scroll_ = nullptr;
	QWidget *gridHost_ = nullptr;
	QGridLayout *grid_ = nullptr;
	QVector<MarkerButton *> buttons_;
	int columns_ = 2;

	QLabel *statusLabel_ = nullptr;
	QLabel *timerLabel_ = nullptr;
	QLabel *countLabel_ = nullptr;
	std::function<QString(const QString &)> hotkeyLabel_;
	QLabel *messageLabel_ = nullptr;
	QPushButton *undoBtn_ = nullptr;
	QString lastText_;
	QListWidget *history_ = nullptr;
	QTimer *tick_ = nullptr;
	QTimer *messageTimer_ = nullptr;
};

} // namespace sm
