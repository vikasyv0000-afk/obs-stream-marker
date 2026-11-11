#include "ui/SettingsDialog.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QUrl>
#include <QVBoxLayout>

#include "ui/CategoryDialogs.h"

namespace sm {

SettingsDialog::SettingsDialog(SessionManager &sessions, CategoryManager &cats, QWidget *parent)
	: QDialog(parent), sessions_(sessions), cats_(cats)
{
	setWindowTitle(tr("Stream Marker Settings"));
	const Settings s = sessions_.settings();

	auto *root = new QVBoxLayout(this);
	auto *form = new QFormLayout;
	root->addLayout(form);

	offline_ = new QCheckBox(tr("Allow markers while not streaming"), this);
	offline_->setChecked(s.allowOfflineMarkers);
	offline_->setToolTip(tr("Off by default: markers are only recorded during a live stream.\n"
				"When on, clicking while offline starts a separate \"Offline markers\" session."));
	form->addRow(offline_);

	offset_ = new QSpinBox(this);
	offset_->setRange(-3600, 3600);
	offset_->setSuffix(tr(" s"));
	offset_->setValue(s.offsetSeconds);
	offset_->setToolTip(tr("Added to every marker's stream time. Use a negative value (e.g. -10) to\n"
			       "mark slightly before the click, so clips don't start too late."));
	form->addRow(tr("Timestamp offset"), offset_);

	note_ = new QCheckBox(tr("Ask for a note after each marker"), this);
	note_->setChecked(s.promptForNote);
	form->addRow(note_);

	sound_ = new QCheckBox(tr("Play a sound when a marker is recorded"), this);
	sound_->setChecked(s.soundOnMarker);
	sound_->setToolTip(tr("A short beep; the pitch differs per category so you can tell them apart by ear."));
	form->addRow(sound_);

	toast_ = new QCheckBox(tr("Show an on-screen popup when a marker is recorded"), this);
	toast_->setChecked(s.toastOnMarker);
	toast_->setToolTip(tr("Small popup in the top-right corner that never takes focus from your game.\n"
			      "Fullscreen-exclusive games may hide it; borderless/windowed modes show it."));
	form->addRow(toast_);

	recent_ = new QSpinBox(this);
	recent_->setRange(5, 500);
	recent_->setValue(s.recentCount);
	form->addRow(tr("Recent history rows"), recent_);

	auto *manage = new QPushButton(tr("Manage categories…"), this);
	connect(manage, &QPushButton::clicked, this, [this] {
		CategoryListDialog d(cats_, this);
		d.exec();
	});
	root->addWidget(manage);

	auto *open = new QPushButton(tr("Open data folder"), this);
	connect(open, &QPushButton::clicked, this, [this] {
		QDesktopServices::openUrl(QUrl::fromLocalFile(sessions_.storage().directory()));
	});
	root->addWidget(open);

	auto *path = new QLabel(sessions_.storage().directory(), this);
	path->setWordWrap(true);
	path->setTextInteractionFlags(Qt::TextSelectableByMouse);
	path->setStyleSheet("color:gray; font-size:10px;");
	root->addWidget(path);

	auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(bb, &QDialogButtonBox::accepted, this, [this] {
		Settings n = sessions_.settings();
		n.allowOfflineMarkers = offline_->isChecked();
		n.offsetSeconds = offset_->value();
		n.promptForNote = note_->isChecked();
		n.recentCount = recent_->value();
		n.soundOnMarker = sound_->isChecked();
		n.toastOnMarker = toast_->isChecked();
		sessions_.setSettings(n);
		accept();
	});
	root->addWidget(bb);
}

} // namespace sm
