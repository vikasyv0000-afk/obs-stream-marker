#pragma once
// Dialogs for creating / editing / deleting / reordering categories.

#include <QDialog>

#include "core/CategoryManager.h"

class QLineEdit;
class QListWidget;
class QPushButton;

namespace sm {

// Add or edit one category: name, colour (swatches + colour picker), icon (emoji grid + free text).
class CategoryEditDialog : public QDialog {
	Q_OBJECT
public:
	explicit CategoryEditDialog(const Category &initial, bool isNew, QWidget *parent = nullptr);
	Category result() const;

private:
	void setColor(const QString &hex);
	void updatePreview();

	Category cat_;
	QLineEdit *name_;
	QLineEdit *icon_;
	QPushButton *colorBtn_;
	QPushButton *preview_;
};

// Manage the whole list: add, edit, delete, reorder. Changes apply immediately.
class CategoryListDialog : public QDialog {
	Q_OBJECT
public:
	CategoryListDialog(CategoryManager &mgr, QWidget *parent = nullptr);

private:
	void refresh(int select = -1);
	QString currentId() const;

	CategoryManager &mgr_;
	QListWidget *list_;
};

// Shared helper used by the dock and the list dialog.
bool confirmDeleteCategory(QWidget *parent, const Category &c);

} // namespace sm
