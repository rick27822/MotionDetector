#include "ChartView.h"
#include "modules/ui/theme/Theme.h"
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>
namespace ui {
ChartView::ChartView(QWidget *p) : QWidget(p) {
    setMinimumHeight(155);
    setMouseTracking(true);
    setAccessibleName("关节角度时间曲线，点击定位样例时间");
}
void ChartView::setSamples(const QVector<core::AngleSample> &s) {
    m_samples = s;
    update();
}
void ChartView::setPosition(qint64 t) {
    m_position = t;
    update();
}
void ChartView::setSecondChannel(core::JointAngle c) {
    m_second = c;
    update();
}
QRectF ChartView::plotRect() const {
    return QRectF(40, 28, qMax(1, width() - 55), qMax(1, qMin(height() - 65, 300)));
}
void ChartView::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QFont f = font();
    f.setPixelSize(10);
    p.setFont(f);
    auto r = plotRect();
    const double ymin = m_second == core::JointAngle::Trunk ? 0 : 60, ymax = 180;
    auto y = [&](double v) { return r.bottom() - (v - ymin) / (ymax - ymin) * r.height(); };
    for (int i = 0; i < 5; i++) {
        double v = ymin + (ymax - ymin) * i / 4.;
        p.setPen(QColor("#1B1B1E"));
        p.drawLine(QPointF(r.left(), y(v)), QPointF(r.right(), y(v)));
        p.setPen(QColor("#71717A"));
        p.drawText(QRectF(0, y(v) - 8, 30, 16), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(v, 'f', 0) + "°");
    }
    if (m_samples.isEmpty()) {
        p.setPen(Muted);
        p.drawText(r, Qt::AlignCenter, "暂无角度数据");
        return;
    }
    qint64 end = qMax<qint64>(1, m_samples.last().t);
    auto x = [&](qint64 t) { return r.left() + double(t) / end * r.width(); };
    for (int i = 0; i < 4; i++) {
        qint64 t = end * i / 3;
        p.setPen(QColor("#71717A"));
        p.drawText(QRectF(x(t) - 18, r.bottom() + 12, 45, 18), Qt::AlignLeft,
                   QString::number(t / 1000., 'f', i ? 1 : 0) + "s");
    }
    p.save();
    p.setClipRect(r.adjusted(-1, -2, 1, 2));
    for (int series = 0; series < 2; series++) {
        core::JointAngle channel = series == 0 ? core::JointAngle::LeftKnee : m_second;
        QPainterPath path;
        bool active = false;
        // Fixtures are small. Retain all valid samples; gaps always start a new subpath.
        for (const auto &s : m_samples) {
            float v = s.angle(channel);
            if (!s.hasAngle(channel) || !std::isfinite(v)) {
                active = false;
                continue;
            }
            QPointF pt(x(s.t), y(v));
            if (active)
                path.lineTo(pt);
            else
                path.moveTo(pt);
            active = true;
        }
        p.setPen(QPen(series == 0                           ? Cyan
                      : m_second == core::JointAngle::Trunk ? Orange
                                                            : QColor("#77777D"),
                      2, series == 0 ? Qt::SolidLine : Qt::DashLine));
        p.drawPath(path);
    }
    qint64 clamped = qBound<qint64>(qint64(0), m_position, end);
    p.setPen(QPen(QColor("#8E550B"), 1, Qt::DashLine));
    p.drawLine(QPointF(x(clamped), r.top()), QPointF(x(clamped), r.bottom()));
    auto it = std::lower_bound(m_samples.begin(), m_samples.end(), clamped,
                               [](const auto &s, qint64 t) { return s.t < t; });
    if (it != m_samples.end() && it->hasAngle(core::JointAngle::LeftKnee)) {
        QPointF pt(x(it->t), y(it->angle(core::JointAngle::LeftKnee)));
        p.setPen(QPen(Qt::white, 1.4));
        p.setBrush(Cyan);
        p.drawEllipse(pt, 4, 4);
        p.setPen(Qt::white);
        p.drawText(pt + QPointF(9, -10),
                   QString::number(it->angle(core::JointAngle::LeftKnee), 'f', 0) + "°");
    }
    p.restore();
}
void ChartView::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton && !m_samples.isEmpty()) {
        auto r = plotRect();
        emit seekRequested(qBound<qint64>(
            qint64(0), qRound64((e->position().x() - r.left()) / r.width() * m_samples.last().t),
            m_samples.last().t));
    }
}
} // namespace ui
