#include "SkeletonPainter.h"

#include <QPainterPath>
#include <QSet>
#include <QtMath>

namespace pose {

namespace {

bool isLeft(int index)
{
    static const QSet<int> left = {1, 2, 3, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31};
    return left.contains(index);
}

bool isRight(int index)
{
    static const QSet<int> right = {4, 5, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32};
    return right.contains(index);
}

QColor withAlpha(QColor c, int alpha)
{
    c.setAlpha(alpha);
    return c;
}

} // namespace

QRectF SkeletonPainter::fittedRect(const QSize &frameSize, const QRectF &target)
{
    if (frameSize.isEmpty() || target.isEmpty()) return target;
    const qreal frameAspect = qreal(frameSize.width()) / qreal(frameSize.height());
    const qreal targetAspect = target.width() / target.height();
    if (frameAspect > targetAspect) {
        const qreal h = target.width() / frameAspect;
        return QRectF(target.left(), target.top() + (target.height() - h) * 0.5,
                      target.width(), h);
    }
    const qreal w = target.height() * frameAspect;
    return QRectF(target.left() + (target.width() - w) * 0.5, target.top(), w, target.height());
}

QPointF SkeletonPainter::toPixel(const core::Keypoint &keypoint, const QRectF &fitted, bool mirror)
{
    const qreal x = mirror ? (1.0 - keypoint.x) : keypoint.x;
    return QPointF(fitted.left() + x * fitted.width(),
                   fitted.top() + keypoint.y * fitted.height());
}

QVector<QPair<int, int>> SkeletonPainter::connections()
{
    static const QVector<QPair<int, int>> conns = {
        {0, 1},   {1, 2},   {2, 3},   {3, 7},   {0, 4},   {4, 5},   {5, 6},   {6, 8},
        {9, 10},  {11, 12}, {11, 13}, {13, 15}, {15, 17}, {15, 19}, {15, 21}, {17, 19},
        {12, 14}, {14, 16}, {16, 18}, {16, 20}, {16, 22}, {18, 20}, {11, 23}, {12, 24},
        {23, 24}, {23, 25}, {25, 27}, {27, 29}, {29, 31}, {27, 31}, {24, 26}, {26, 28},
        {28, 30}, {30, 32}, {28, 32},
    };
    return conns;
}

void SkeletonPainter::draw(QPainter &painter, const QRectF &target, const core::PoseResult &pose,
                           const SkeletonStyle &style, const QSize &frameSize)
{
    if (target.isEmpty()) return;
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF fitted = fittedRect(frameSize, target);
    const qreal h = fitted.height();
    const qreal boneWidth = qMax(1.2, style.boneWidthScale * h);
    const qreal jointRadius = qMax(1.8, style.jointRadiusScale * h);

    auto pointOf = [&](int i, QPointF *out) {
        const core::Keypoint &kp = pose.kp(i);
        if (!kp.valid || kp.confidence < style.minConfidence) return false;
        *out = toPixel(kp, fitted, style.mirror);
        return true;
    };

    for (const auto &conn : connections()) {
        QPointF a, b;
        if (!pointOf(conn.first, &a) || !pointOf(conn.second, &b)) continue;
        const float conf = qMin(pose.kp(conn.first).confidence, pose.kp(conn.second).confidence);
        QColor color = style.centerBone;
        if (isLeft(conn.first) && isLeft(conn.second)) color = style.leftBone;
        else if (isRight(conn.first) && isRight(conn.second)) color = style.rightBone;

        const int alpha = int(120 + 135 * qBound(0.0f, conf, 1.0f));
        if (style.glow) {
            painter.setPen(QPen(withAlpha(color, 38), boneWidth * 3.2, Qt::SolidLine,
                                Qt::RoundCap, Qt::RoundJoin));
            painter.drawLine(a, b);
        }
        painter.setPen(QPen(withAlpha(color, alpha), boneWidth, Qt::SolidLine, Qt::RoundCap,
                            Qt::RoundJoin));
        painter.drawLine(a, b);
    }

    if (style.drawJoints) {
        for (int i = 0; i < pose.keypoints.size(); ++i) {
            QPointF p;
            if (!pointOf(i, &p)) continue;
            const float conf = pose.kp(i).confidence;
            painter.setPen(Qt::NoPen);
            painter.setBrush(withAlpha(style.jointHalo, 150));
            painter.drawEllipse(p, jointRadius * 1.9, jointRadius * 1.9);
            painter.setBrush(withAlpha(style.jointFill, int(140 + 115 * qBound(0.0f, conf, 1.0f))));
            painter.drawEllipse(p, jointRadius, jointRadius);
        }
    }

    if (style.drawLabels) {
        painter.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), int(qMax(7.0, h * 0.018))));
        painter.setPen(QColor(255, 255, 255, 190));
        for (int i = 0; i < pose.keypoints.size(); ++i) {
            QPointF p;
            if (!pointOf(i, &p)) continue;
            painter.drawText(p + QPointF(jointRadius * 2.0, -jointRadius * 1.2),
                             core::keypointName(i));
        }
    }

    painter.restore();
}

void SkeletonPainter::drawJoints(QPainter &painter, const QRectF &target, const core::PoseResult &pose,
                                 const SkeletonStyle &style, const QSize &frameSize)
{
    SkeletonStyle s = style;
    s.drawJoints = true;
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF fitted = fittedRect(frameSize, target);
    const qreal jointRadius = qMax(1.8, s.jointRadiusScale * fitted.height());
    for (int i = 0; i < pose.keypoints.size(); ++i) {
        const core::Keypoint &kp = pose.kp(i);
        if (!kp.valid || kp.confidence < s.minConfidence) continue;
        const QPointF p = toPixel(kp, fitted, s.mirror);
        painter.setPen(Qt::NoPen);
        painter.setBrush(withAlpha(s.jointFill, 220));
        painter.drawEllipse(p, jointRadius, jointRadius);
    }
    painter.restore();
}

} // namespace pose
