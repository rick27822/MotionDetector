#pragma once
#include "core/types.h"
#include <QWidget>
namespace ui {
class ChartView : public QWidget {
    Q_OBJECT
  public:
    explicit ChartView(QWidget *parent = nullptr);
    void setSamples(const QVector<core::AngleSample> &samples);
    void setPosition(qint64 position);
    void setSecondChannel(core::JointAngle channel);
    qint64 position() const {
        return m_position;
    }

  protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;

  private:
    QVector<core::AngleSample> m_samples;
    qint64 m_position = 0;
    core::JointAngle m_second = core::JointAngle::RightKnee;
    QRectF plotRect() const;
  signals:
    void seekRequested(qint64 position);
};
} // namespace ui
