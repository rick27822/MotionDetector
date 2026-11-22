#include "Theme.h"
#include <QApplication>
#include <QFile>
#include <QFontDatabase>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
static void initUiResource() {
    Q_INIT_RESOURCE(ui);
}
namespace ui {
void initializeResources() {
    initUiResource();
}
void applyTheme(QApplication &app) {
    initializeResources();
    app.setStyle("Fusion");
    QFont font("PingFang SC");
    font.setPixelSize(13);
    app.setFont(font);
    QFile f(":/theme/Style.qss");
    if (f.open(QFile::ReadOnly))
        app.setStyleSheet(QString::fromUtf8(f.readAll()));
    QPalette pal;
    pal.setColor(QPalette::Window, QColor("#000000"));
    pal.setColor(QPalette::WindowText, Qt::white);
    pal.setColor(QPalette::Text, Qt::white);
    pal.setColor(QPalette::Base, QColor("#121214"));
    pal.setColor(QPalette::Highlight, Orange);
    pal.setColor(QPalette::ButtonText, Qt::white);
    app.setPalette(pal);
}
QIcon icon(const QString &name, QColor color) {
    QPixmap pix(40, 40);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.scale(2, 2);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (name == "wave") {
        QPainterPath path;
        path.moveTo(1, 11);
        path.lineTo(5, 11);
        path.lineTo(8, 4);
        path.lineTo(11, 16);
        path.lineTo(14, 8);
        path.lineTo(18, 8);
        p.drawPath(path);
    } else if (name == "phone") {
        p.drawRoundedRect(QRectF(5, 2, 10, 16), 2, 2);
        p.drawPoint(10, 15);
    } else if (name == "sliders") {
        for (int i = 0; i < 3; i++) {
            int y = 5 + i * 5;
            p.drawLine(2, y, 18, y);
            p.setBrush(QColor("#0A0A0B"));
            p.drawEllipse(QPointF(i == 1 ? 13 : 7, y), 2, 2);
        }
    } else if (name == "video") {
        p.drawRoundedRect(QRectF(2, 5, 11, 10), 2, 2);
        QPolygonF a{{13, 8}, {18, 5}, {18, 15}, {13, 12}};
        p.drawPolygon(a);
    } else if (name == "chart") {
        for (int i = 0; i < 4; i++)
            p.drawLine(3 + i * 4, 17, 3 + i * 4, 12 - (i % 3) * 4);
    } else if (name == "settings") {
        p.drawEllipse(QPointF(10, 10), 3, 3);
        for (int i = 0; i < 8; i++) {
            p.save();
            p.translate(10, 10);
            p.rotate(i * 45);
            p.drawLine(0, -6, 0, -8);
            p.restore();
        }
    } else if (name == "play") {
        p.setBrush(color);
        p.drawPolygon(QPolygonF{{6, 3}, {17, 10}, {6, 17}});
    } else if (name == "pause") {
        p.drawLine(7, 4, 7, 16);
        p.drawLine(13, 4, 13, 16);
    } else if (name == "stop") {
        p.setBrush(color);
        p.drawRoundedRect(QRectF(5, 5, 10, 10), 1, 1);
    } else if (name == "refresh") {
        p.drawArc(QRectF(3, 3, 14, 14), 40 * 16, 285 * 16);
        p.drawLine(16, 2, 16, 7);
        p.drawLine(12, 6, 16, 7);
    } else if (name == "expand") {
        p.drawLine(3, 8, 3, 3);
        p.drawLine(3, 3, 8, 3);
        p.drawLine(12, 17, 17, 17);
        p.drawLine(17, 17, 17, 12);
    } else {
        p.drawEllipse(QPointF(10, 10), 5, 5);
    }
    p.end();
    pix.setDevicePixelRatio(2);
    return QIcon(pix);
}
} // namespace ui
