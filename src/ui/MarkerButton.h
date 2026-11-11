#pragma once
// Wide category button: [icon]  Label  [hotkey badge], filled with the category colour.

#include <QPushButton>

#include "core/Models.h"

class QLabel;

namespace sm {

class MarkerButton : public QPushButton {
	Q_OBJECT
public:
	explicit MarkerButton(const Category &c, QWidget *parent = nullptr);

	const Category &category() const { return cat_; }
	void setCategory(const Category &c);
	void setHotkeyText(const QString &text);
	void flash(); // brief visual confirmation after a marker is recorded

signals:
	void editRequested(const QString &categoryId);
	void deleteRequested(const QString &categoryId);
	void moveRequested(const QString &categoryId, int delta);

protected:
	void contextMenuEvent(QContextMenuEvent *e) override;

private:
	void applyStyle(bool flashing);

	Category cat_;
	QLabel *icon_;
	QLabel *text_;
	QLabel *badge_;
};

} // namespace sm
