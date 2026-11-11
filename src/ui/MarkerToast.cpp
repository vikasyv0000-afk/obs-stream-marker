#include "ui/MarkerToast.h"

#include <QGuiApplication>
#include <QLabel>
#include <QPointer>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>
#include <thread>

#include "ui/Theme.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace sm::MarkerFeedback {

void beep(int categoryIndex)
{
#ifdef _WIN32
	const DWORD freq = 700 + 140 * static_cast<DWORD>(categoryIndex < 0 ? 0 : categoryIndex % 6);
	// Beep() blocks for its duration, so run it on a throw-away thread; the UI never waits.
	std::thread([freq] { ::Beep(freq, 80); }).detach();
#else
	Q_UNUSED(categoryIndex);
	QGuiApplication::beep();
#endif
}

void toast(const QString &text, const QColor &accent)
{
	static QPointer<QLabel> label;
	static QPointer<QTimer> hideTimer;

	if (!label) {
		label = new QLabel(nullptr, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
						    Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput);
		label->setAttribute(Qt::WA_ShowWithoutActivating);
		label->setAttribute(Qt::WA_TranslucentBackground, false);
		label->setAlignment(Qt::AlignCenter);
		hideTimer = new QTimer(label);
		hideTimer->setSingleShot(true);
		QObject::connect(hideTimer, &QTimer::timeout, label, &QWidget::hide);
	}

	label->setStyleSheet(QStringLiteral("QLabel{background:%1;color:%2;border:2px solid %3;border-radius:12px;"
					    "padding:12px 20px;font-size:18px;font-weight:700;"
					    "font-family:'Segoe UI','Segoe UI Emoji';}")
				     .arg(theme::kSurface, theme::kInk, accent.name()));
	label->setText(text);
	label->adjustSize();

	if (QScreen *s = QGuiApplication::primaryScreen()) {
		const QRect g = s->availableGeometry();
		label->move(g.right() - label->width() - 24, g.top() + 24);
	}
	label->show();
	label->raise();
	hideTimer->start(1400);
}

} // namespace sm::MarkerFeedback
