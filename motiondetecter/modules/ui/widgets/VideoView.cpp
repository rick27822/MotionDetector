#include "VideoView.h"
#include "Controls.h"
#include "modules/pose/SkeletonPainter.h"
#include "modules/ui/theme/Theme.h"
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
namespace ui {
VideoView::VideoView(bool playback, QWidget *parent)
    : QWidget(parent), m_image(":/images/person.png"), m_pose(referencePose()),
      m_playback(playback) {
    setMinimumSize(300, 260);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAccessibleName("静态参考画面与人工标注，非真实模型输出");
}
core::PoseResult VideoView::referencePose() {
    core::PoseResult r;
    r.keypoints.resize(33);
    r.detected = true;
    r.keypointCount = 17;
    r.modelName = "人工标注视觉样例";
    auto kp = [&](int i, float x, float y) {
        r.keypoints[i] = {x / 1536.f, y / 1024.f, 1.f, true};
    };
    kp(0, 774, 356);
    kp(11, 700, 433);
    kp(12, 835, 433);
    kp(13, 673, 475);
    kp(14, 850, 480);
    kp(15, 720, 478);
    kp(16, 787, 484);
    kp(23, 715, 615);
    kp(24, 844, 615);
    kp(25, 626, 688);
    kp(26, 912, 690);
    kp(27, 655, 857);
    kp(28, 911, 855);
    r.setAngle(core::JointAngle::LeftKnee, 87);
    r.setAngle(core::JointAngle::RightKnee, 92);
    r.setAngle(core::JointAngle::Trunk, 31.4f);
    r.setAngle(core::JointAngle::LeftHip, 142);
    r.setAngle(core::JointAngle::RightHip, 138);
    r.setAngle(core::JointAngle::LeftShoulder, 72);
    r.setAngle(core::JointAngle::LeftElbow, 87);
    return r;
}
void VideoView::setPose(const core::PoseResult &r) {
    m_pose = r;
    update();
}
void VideoView::setOverlay(bool s, bool a) {
    m_skeleton = s;
    m_angles = a;
    update();
}
void VideoView::setAvailable(bool a) {
    m_available = a;
    update();
}
void VideoView::setMirror(bool m) {
    m_mirror = m;
    update();
}
void VideoView::setPlaying(bool p) {
    m_playing = p;
    update();
}
void VideoView::setPosition(qint64 p, qint64 d) {
    m_position = p;
    m_duration = d;
    update();
}
void VideoView::setReferenceImageEnabled(bool enabled) {
    m_reference = enabled;
    update();
}
void VideoView::setCover(bool c) {
    m_cover = c;
    update();
}
QRectF VideoView::imageRect() const {
    if (m_image.isNull())
        return {};
    QSize source = m_reference ? m_image.size() : QSize(1280, 720);
    qreal s = m_cover && m_reference
                  ? qMax(width() / qreal(source.width()), height() / qreal(source.height()))
                  : qMin(width() / qreal(source.width()), height() / qreal(source.height()));
    QSizeF size = source * s;
    return {(width() - size.width()) / 2., (height() - size.height()) / 2., size.width(),
            size.height()};
}
QPointF VideoView::mapPoint(const QPointF &n) const {
    auto r = imageRect();
    return {r.x() + (m_mirror ? 1. - n.x() : n.x()) * r.width(), r.y() + n.y() * r.height()};
}
void VideoView::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor("#101011"));
    QPainterPath clip;
    clip.addRoundedRect(rect(), m_playback ? 14 : 0, m_playback ? 14 : 0);
    p.setClipPath(clip);
    if (!m_available) {
        p.setPen(Muted);
        p.drawText(rect(), Qt::AlignCenter, "暂无画面\n请先连接样例设备，再开始检测");
        return;
    }
    auto r = imageRect();
    if (m_reference && m_mirror) {
        p.save();
        p.translate(width(), 0);
        p.scale(-1, 1);
        p.drawImage(r, m_image);
        p.restore();
    } else if (m_reference)
        p.drawImage(r, m_image);
    if (m_skeleton && m_pose.detected) {
        pose::SkeletonStyle style;
        style.leftBone = style.rightBone = style.centerBone = Cyan;
        style.jointFill = Cyan;
        style.jointHalo = Qt::white;
        style.boneWidthScale = .0025;
        style.jointRadiusScale = .004;
        style.glow = false;
        style.mirror = m_mirror;
        pose::SkeletonPainter::draw(p, r, m_pose, style,
                                    m_reference ? m_image.size() : QSize(1280, 720));
    }
    QFont f = font();
    f.setPixelSize(12);
    p.setFont(f);
    if (m_angles && !m_playback) {
        const QVector<core::JointAngle> channels = {
            core::JointAngle::LeftHip, core::JointAngle::RightHip, core::JointAngle::Trunk,
            core::JointAngle::LeftShoulder, core::JointAngle::LeftKnee};
        int x = 16;
        for (auto a : channels) {
            QString text =
                core::angleName(a) + " " +
                (m_pose.hasAngle(a)
                     ? QString::number(m_pose.angle(a), 'f', a == core::JointAngle::Trunk ? 1 : 0) +
                           "°"
                     : "—");
            int w = p.fontMetrics().horizontalAdvance(text) + 16;
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(25, 25, 28, 155));
            p.drawRoundedRect(QRect(x, 16, w, 29), 5, 5);
            p.setPen(a == core::JointAngle::LeftKnee ? Cyan : QColor("#EEEEEF"));
            p.drawText(QRect(x, 16, w, 29), Qt::AlignCenter, text);
            x += w + 7;
        }
        QRect box(width() / 2 - 44, 64, 88, 89);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(15, 15, 17, 180));
        p.drawRoundedRect(box, 12, 12);
        p.setPen(Qt::white);
        p.drawText(box.adjusted(0, 10, 0, -55), Qt::AlignCenter, "膝关节角度");
        f.setPixelSize(32);
        f.setWeight(QFont::DemiBold);
        p.setFont(f);
        p.setPen(Cyan);
        p.drawText(box.adjusted(0, 31, 0, -3), Qt::AlignCenter,
                   m_pose.hasAngle(core::JointAngle::LeftKnee)
                       ? QString::number(m_pose.angle(core::JointAngle::LeftKnee), 'f', 0) + "°"
                       : "—");
    }
    if (m_playback && !m_playing) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 150));
        p.drawEllipse(rect().center(), 32, 32);
        auto c = rect().center();
        p.setBrush(Qt::white);
        p.drawPolygon(QPolygonF{{qreal(c.x() - 7), qreal(c.y() - 13)},
                                {qreal(c.x() + 15), qreal(c.y())},
                                {qreal(c.x() - 7), qreal(c.y() + 13)}});
    }
    f = font();
    f.setPixelSize(10);
    p.setFont(f);
    QString note = m_reference ? "静态参考图 · 人工标注" : "交接姿态样例 · 无对应视频帧";
    int nw = p.fontMetrics().horizontalAdvance(note) + 16;
    p.fillRect(QRect(12, height() - 30, nw, 20), QColor(0, 0, 0, 145));
    p.setPen(Qt::white);
    p.drawText(QRect(12, height() - 30, nw, 20), Qt::AlignCenter, note);
    if (m_playback) {
        QString time = core::formatDuration(m_position) + " / " + core::formatDuration(m_duration);
        p.fillRect(QRect(width() - 160, height() - 40, 148, 26), QColor(0, 0, 0, 145));
        p.drawText(QRect(width() - 160, height() - 40, 148, 26), Qt::AlignCenter, time);
    }
}
void VideoView::mousePressEvent(QMouseEvent *e) {
    if (m_playback && m_available && e->button() == Qt::LeftButton)
        emit playRequested();
    QWidget::mousePressEvent(e);
}
} // namespace ui
