#pragma once
#include <QColor>
#include <QIcon>
class QApplication;
namespace ui {
inline const QColor Orange{"#FF8800"}, Cyan{"#00E5FF"}, Muted{"#A1A1AA"};
void initializeResources();
void applyTheme(QApplication &app);
QIcon icon(const QString &name, QColor color = Muted);
} // namespace ui
