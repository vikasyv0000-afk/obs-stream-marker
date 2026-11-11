#include "ui/MarkerButton.h"

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QTimer>

#include "ui/Theme.h"

namespace sm {

MarkerButton::MarkerButton(const Category &c, QWidget *parent) : QPushButton(parent), cat_(c)
{
	setMinimumHeight(44);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	setCursor(Qt::PointingHandCursor);
	setFocusPolicy(Qt::TabFocus);

	auto *lay = new QHBoxLayout(this);
	lay->setContentsMargins(8, 4, 6, 4);
	lay->setSpacing(6);

	icon_ = new QLabel(this);
	icon_->setAlignment(Qt::AlignCenter);
	icon_->setAttribute(Qt::WA_TransparentForMouseEvents);
	lay->addWidget(icon_);

	text_ = new QLabel(this);
	text_->setWordWrap(true);
	text_->setAttribute(Qt::WA_TransparentForMouseEvents);
	text_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
	lay->addWidget(text_, 1);

	badge_ = new QLabel(this);
	badge_->setAlignment(Qt::AlignCenter);
	badge_->setAttribute(Qt::WA_TransparentForMouseEvents);
	badge_->hide();
	lay->addWidget(badge_);

	setCategory(c);
}

void MarkerButton::setCategory(const Category &c)
{
	cat_ = c;
	icon_->setText(c.icon);
	text_->setText(c.name);
	setToolTip(c.name);
	applyStyle(false);
}

void MarkerButton::setHotkeyText(const QString &t)
{
	badge_->setText(t);
	badge_->setVisible(!t.isEmpty());
}

void MarkerButton::applyStyle(bool flashing)
{
	QColor base(cat_.color);
	if (!base.isValid())
		base = QColor("#3987e5");
	const QColor ink = theme::inkOn(base);
	const QColor dimBase(int(base.red() * 0.30 + 40), int(base.green() * 0.30 + 40), int(base.blue() * 0.30 + 40));
	const QColor shown = flashing ? base.lighter(130) : base;

	setStyleSheet(QStringLiteral("QPushButton{background:%1;border:1px solid %2;border-radius:12px;}"
				     "QPushButton:hover{background:%3;}"
				     "QPushButton:pressed{background:%4;}"
				     "QPushButton:focus{border:2px solid %5;}"
				     "QPushButton:disabled{background:%6;border:1px solid %7;}")
			      .arg(shown.name(), base.lighter(125).name(), base.lighter(112).name(), base.darker(115).name(),
				   theme::kInk, dimBase.name(), theme::kBorder));

	const QString inkName = ink.name();
	icon_->setStyleSheet("QLabel{background:transparent;font-size:18px;font-family:'Segoe UI Emoji','Segoe UI Symbol','Segoe UI';}");
	text_->setStyleSheet(QStringLiteral("QLabel{background:transparent;color:%1;font-weight:700;font-size:12px;}"
					    "QLabel:disabled{color:%2;}")
				     .arg(inkName, theme::kInk2));
	badge_->setStyleSheet(QStringLiteral("QLabel{background:rgba(%1,%2,%3,0.22);color:%4;border-radius:6px;"
					     "padding:1px 5px;font-weight:700;font-size:10px;}"
					     "QLabel:disabled{color:%5;}")
				      .arg(ink.red())
				      .arg(ink.green())
				      .arg(ink.blue())
				      .arg(inkName, theme::kInk2));
}

void MarkerButton::flash()
{
	applyStyle(true);
	QTimer::singleShot(180, this, [this] { applyStyle(false); });
}

void MarkerButton::contextMenuEvent(QContextMenuEvent *e)
{
	QMenu menu(this);
	const QString id = cat_.id;
	menu.addAction(tr("Edit category..."), this, [this, id] { emit editRequested(id); });
	menu.addAction(tr("Move earlier"), this, [this, id] { emit moveRequested(id, -1); });
	menu.addAction(tr("Move later"), this, [this, id] { emit moveRequested(id, +1); });
	menu.addSeparator();
	menu.addAction(tr("Delete category"), this, [this, id] { emit deleteRequested(id); });
	menu.exec(e->globalPos());
}

} // namespace sm
