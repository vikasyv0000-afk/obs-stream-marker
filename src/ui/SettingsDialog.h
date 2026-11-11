#pragma once

#include <QDialog>

#include "core/CategoryManager.h"
#include "core/SessionManager.h"

class QCheckBox;
class QSpinBox;

namespace sm {

class SettingsDialog : public QDialog {
	Q_OBJECT
public:
	SettingsDialog(SessionManager &sessions, CategoryManager &cats, QWidget *parent = nullptr);

private:
	SessionManager &sessions_;
	CategoryManager &cats_;
	QCheckBox *offline_;
	QCheckBox *note_;
	QCheckBox *sound_;
	QCheckBox *toast_;
	QSpinBox *offset_;
	QSpinBox *recent_;
};

} // namespace sm
