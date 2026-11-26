#pragma once
#include "app/AppState.h"
#include "modules/ui/controllers/Controllers.h"
#include "modules/ui/widgets/VideoView.h"
#include "modules/ui/controllers/RealtimeController.h"
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QWidget>
namespace ui {
class RealtimePage : public QWidget {
    Q_OBJECT
  public:
    RealtimePage(AppState *, DetectionController *, RealtimeController *, QWidget *parent = nullptr);
    VideoView *video;

  private:
    AppState *m_state;
    RealtimeController *m_realtime;
    QLabel *m_hintTitle, *m_hintText, *m_repCount, *m_sourceNote;
    QComboBox *m_source;
    QProgressBar *m_kneeProgress, *m_confidenceProgress;
    QPushButton *m_detect, *m_record;
    QLabel *m_confidence;
    QVector<QPair<core::JointAngle, QLabel *>> m_angleValues;
    QVector<core::PoseResult> m_samples;
    int m_sampleIndex = -1;
    void refresh();
};
} // namespace ui
