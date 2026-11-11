#include "ui/CategoryDialogs.h"

#include "ui/Theme.h"

#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace sm {

namespace {
const char *kSwatches[] = {"#199e70", "#c98500", "#3987e5", "#d55181", "#d95926", "#9085e9",
			   "#008300", "#e66767", "#6da7ec", "#eda100", "#e87ba4", "#898781"};

// UTF-8 literals so the source is encoding-safe.
const char *kIcons[] = {"\xF0\x9F\x8E\xAF", "\xF0\x9F\x94\xA5", "\xF0\x9F\x98\x82", "\xF0\x9F\x8D\x97", "\xE2\xAD\x90",
			"\xF0\x9F\x8F\x86", "\xF0\x9F\x92\x80", "\xF0\x9F\x92\xA5", "\xF0\x9F\x91\x91", "\xF0\x9F\x9A\x80",
			"\xF0\x9F\x98\xB1", "\xF0\x9F\xA4\xA3", "\xF0\x9F\x91\x8F", "\xF0\x9F\x92\xAA", "\xF0\x9F\x8E\xAE",
			"\xF0\x9F\x94\xAB", "\xF0\x9F\x9B\xA1", "\xF0\x9F\x9A\x97", "\xF0\x9F\x8E\x89", "\xF0\x9F\x92\xAF",
			"\xE2\x9A\xA1", "\xE2\x9D\xA4", "\xF0\x9F\x91\x80", "\xF0\x9F\x8E\xA4", "\xF0\x9F\x92\xAC",
			"\xE2\x9D\x97", "\xF0\x9F\x90\x94", "\xF0\x9F\xA7\xA0", "\xF0\x9F\x98\x8E", "\xF0\x9F\x9A\xA8"};
} // namespace

// ---- CategoryEditDialog ----------------------------------------------------

CategoryEditDialog::CategoryEditDialog(const Category &initial, bool isNew, QWidget *parent)
	: QDialog(parent), cat_(initial)
{
	setWindowTitle(isNew ? tr("Add Category") : tr("Edit Category"));
	if (cat_.color.isEmpty())
		cat_.color = kSwatches[0];

	auto *root = new QVBoxLayout(this);
	auto *form = new QFormLayout;
	root->addLayout(form);

	name_ = new QLineEdit(cat_.name, this);
	name_->setMaxLength(40);
	name_->setPlaceholderText(tr("e.g. Funny Moment"));
	form->addRow(tr("Name"), name_);

	icon_ = new QLineEdit(cat_.icon, this);
	icon_->setMaxLength(8);
	icon_->setPlaceholderText(tr("emoji or short text"));
	form->addRow(tr("Icon"), icon_);

	auto *iconGrid = new QGridLayout;
	iconGrid->setSpacing(2);
	int i = 0;
	for (const char *e : kIcons) {
		const QString emoji = QString::fromUtf8(e);
		auto *b = new QPushButton(emoji, this);
		b->setFixedSize(34, 34);
		b->setFlat(true);
		b->setStyleSheet("font-size:18px; font-family:'Segoe UI Emoji','Segoe UI Symbol';");
		connect(b, &QPushButton::clicked, this, [this, emoji] {
			icon_->setText(emoji);
			updatePreview();
		});
		iconGrid->addWidget(b, i / 10, i % 10);
		++i;
	}
	form->addRow(QString(), iconGrid);

	colorBtn_ = new QPushButton(tr("Pick colour…"), this);
	connect(colorBtn_, &QPushButton::clicked, this, [this] {
		const QColor c = QColorDialog::getColor(QColor(cat_.color), this, tr("Button colour"));
		if (c.isValid())
			setColor(c.name());
	});
	form->addRow(tr("Colour"), colorBtn_);

	auto *swatchRow = new QHBoxLayout;
	swatchRow->setSpacing(4);
	for (const char *hex : kSwatches) {
		const QString h = QString::fromLatin1(hex);
		auto *b = new QPushButton(this);
		b->setFixedSize(26, 26);
		b->setStyleSheet(QStringLiteral("background:%1;border:1px solid #666;border-radius:6px;").arg(h));
		connect(b, &QPushButton::clicked, this, [this, h] { setColor(h); });
		swatchRow->addWidget(b);
	}
	swatchRow->addStretch(1);
	form->addRow(QString(), swatchRow);

	preview_ = new QPushButton(this);
	preview_->setMinimumHeight(64);
	preview_->setEnabled(false);
	root->addWidget(preview_);

	auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	root->addWidget(bb);
	connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(bb, &QDialogButtonBox::accepted, this, [this] {
		if (name_->text().trimmed().isEmpty()) {
			QMessageBox::warning(this, windowTitle(), tr("Please enter a category name."));
			return;
		}
		accept();
	});
	connect(name_, &QLineEdit::textChanged, this, &CategoryEditDialog::updatePreview);
	connect(icon_, &QLineEdit::textChanged, this, &CategoryEditDialog::updatePreview);

	setColor(cat_.color);
	name_->setFocus();
}

void CategoryEditDialog::setColor(const QString &hex)
{
	cat_.color = hex;
	updatePreview();
}

void CategoryEditDialog::updatePreview()
{
	preview_->setText(icon_->text() + QStringLiteral("  ") + name_->text());
	preview_->setStyleSheet(QStringLiteral("QPushButton:disabled{background:%1;color:%2;border-radius:12px;"
					       "font-weight:600;font-size:14px;"
					       "font-family:'Segoe UI Emoji','Segoe UI';}")
					.arg(cat_.color, theme::inkOn(QColor(cat_.color)).name()));
}

Category CategoryEditDialog::result() const
{
	Category c = cat_;
	c.name = name_->text().trimmed();
	c.icon = icon_->text().trimmed();
	return c;
}

// ---- helpers ---------------------------------------------------------------

bool confirmDeleteCategory(QWidget *parent, const Category &c)
{
	return QMessageBox::question(parent, QObject::tr("Delete category"),
				     QObject::tr("Delete the category \"%1\"?\n\nMarkers already recorded with it are kept.")
					     .arg(c.name),
				     QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
}

// ---- CategoryListDialog ----------------------------------------------------

CategoryListDialog::CategoryListDialog(CategoryManager &mgr, QWidget *parent) : QDialog(parent), mgr_(mgr)
{
	setWindowTitle(tr("Manage Categories"));
	resize(360, 380);

	auto *root = new QVBoxLayout(this);
	list_ = new QListWidget(this);
	root->addWidget(list_, 1);

	auto *row = new QHBoxLayout;
	auto mk = [&](const QString &text) {
		auto *b = new QPushButton(text, this);
		row->addWidget(b);
		return b;
	};
	auto *add = mk(tr("Add…"));
	auto *edit = mk(tr("Edit…"));
	auto *del = mk(tr("Delete"));
	auto *up = mk(tr("Up"));
	auto *down = mk(tr("Down"));
	root->addLayout(row);

	auto *close = new QDialogButtonBox(QDialogButtonBox::Close, this);
	connect(close, &QDialogButtonBox::rejected, this, &QDialog::accept);
	root->addWidget(close);

	connect(add, &QPushButton::clicked, this, [this] {
		CategoryEditDialog d(Category{}, true, this);
		if (d.exec() == QDialog::Accepted) {
			const Category c = d.result();
			mgr_.add(c.name, c.color, c.icon);
			refresh(mgr_.categories().size() - 1);
		}
	});
	auto doEdit = [this] {
		const auto c = mgr_.byId(currentId());
		if (!c)
			return;
		CategoryEditDialog d(*c, false, this);
		if (d.exec() == QDialog::Accepted) {
			mgr_.update(d.result());
			refresh(list_->currentRow());
		}
	};
	connect(edit, &QPushButton::clicked, this, doEdit);
	connect(list_, &QListWidget::itemDoubleClicked, this, doEdit);
	connect(del, &QPushButton::clicked, this, [this] {
		const auto c = mgr_.byId(currentId());
		if (c && confirmDeleteCategory(this, *c)) {
			const int row = list_->currentRow();
			mgr_.remove(c->id);
			refresh(qMin(row, mgr_.categories().size() - 1));
		}
	});
	connect(up, &QPushButton::clicked, this, [this] {
		const int r = list_->currentRow();
		if (mgr_.move(r, r - 1))
			refresh(r - 1);
	});
	connect(down, &QPushButton::clicked, this, [this] {
		const int r = list_->currentRow();
		if (mgr_.move(r, r + 1))
			refresh(r + 1);
	});

	refresh(0);
}

QString CategoryListDialog::currentId() const
{
	const int r = list_->currentRow();
	return (r >= 0 && r < mgr_.categories().size()) ? mgr_.categories()[r].id : QString();
}

void CategoryListDialog::refresh(int select)
{
	list_->clear();
	for (const Category &c : mgr_.categories()) {
		auto *it = new QListWidgetItem(c.icon + QStringLiteral("  ") + c.name, list_);
		it->setBackground(QColor(c.color));
		it->setForeground(theme::inkOn(QColor(c.color)));
	}
	if (select >= 0 && select < list_->count())
		list_->setCurrentRow(select);
}

} // namespace sm
