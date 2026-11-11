#pragma once
// Design tokens for the dock (dark surface from the dataviz reference palette).
// Category fills are the palette's validated dark-mode categorical steps.

#include <QColor>
#include <cmath>
#include <QString>

namespace sm::theme {

inline const char *kSurface = "#1a1a19";     // panel surface
inline const char *kPage = "#0d0d0d";        // page plane
inline const char *kInk = "#f0efec";         // primary text
inline const char *kInk2 = "#c3c2b7";        // secondary text
inline const char *kMuted = "#898781";       // labels
inline const char *kHair = "#2c2c2a";        // hairline
inline const char *kBorder = "#383835";      // control border
inline const char *kLive = "#0ca30c";        // status: good
inline const char *kCritical = "#d03b3b";    // status: critical
inline const char *kWarning = "#fab219";     // status: warning

inline double luminance(const QColor &c)
{
	auto f = [](double v) { return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
	return 0.2126 * f(c.redF()) + 0.7152 * f(c.greenF()) + 0.0722 * f(c.blueF());
}

// Text colour with the better contrast on this fill (white, or near-black on light fills such as yellow).
inline QColor inkOn(const QColor &fill)
{
	const double l = luminance(fill);
	const double white = 1.05 / (l + 0.05);
	const double dark = (l + 0.05) / (luminance(QColor("#0b0b0b")) + 0.05);
	return white >= dark ? QColor("#ffffff") : QColor("#0b0b0b");
}

} // namespace sm::theme
