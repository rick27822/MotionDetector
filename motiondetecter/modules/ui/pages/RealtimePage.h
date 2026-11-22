#pragma once
#include "app/AppState.h"
#include "modules/ui/controllers/Controllers.h"
#include "modules/ui/widgets/VideoView.h"
#include <QLabel>
#include <QPushButton>
#include <QWidget>
namespace ui {
class RealtimePage : public QWidget {
    Q_OBJECT
  public:
    RealtimePage(AppState *, DetectionController *, QWidget *parent = nullptr);
    VideoView *video;

  private:
    AppState *m_state;
    QPushButton *m_detect, *m_record;
    QLabel *m_confidence;
    QVector<QPair<core::JointAngle, QLabel *>> m_angleValues;
    QVector<core::PoseResult> m_samples;
    int m_sampleIndex = -1;
    void refresh();
};
} // namespace ui
