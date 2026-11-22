#pragma once
#include "core/types.h"
#include <QImage>
#include <QWidget>
namespace ui {
class VideoView : public QWidget {
    Q_OBJECT
  public:
    explicit VideoView(bool playback = false, QWidget *parent = nullptr);
    void setPose(const core::PoseResult &pose);
    void setOverlay(bool skeleton, bool angles);
    void setAvailable(bool available);
    void setMirror(bool mirror);
    void setPlaying(bool playing);
    void setPosition(qint64 position, qint64 duration);
    static core::PoseResult referencePose();
    QRectF imageRect() const;
    QPointF mapPoint(const QPointF &) const;
    void setCover(bool cover);
    void setReferenceImageEnabled(bool enabled);

  protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;

  private:
    QImage m_image;
    core::PoseResult m_pose;
    bool m_reference = true;
    bool m_playback = false, m_skeleton = true, m_angles = true, m_available = true,
         m_mirror = false, m_playing = false, m_cover = true;
    qint64 m_position = 0, m_duration = 12573;
  signals:
    void playRequested();
};
} // namespace ui
