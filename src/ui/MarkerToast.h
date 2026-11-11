#pragma once
// Out-of-game confirmation for a recorded marker: a short tone and/or a small popup.

#include <QColor>
#include <QString>

namespace sm::MarkerFeedback {

// Short non-blocking beep. Pitch depends on the category position so each category sounds different.
void beep(int categoryIndex);

// Small always-on-top popup (top-right of the primary screen) that never takes focus.
void toast(const QString &text, const QColor &accent);

} // namespace sm::MarkerFeedback
