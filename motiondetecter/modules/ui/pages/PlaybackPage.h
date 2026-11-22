#pragma once
#include "app/AppState.h"
#include "modules/ui/controllers/Controllers.h"
#include "modules/ui/widgets/ChartView.h"
#include "modules/ui/widgets/VideoView.h"
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
namespace ui {
class PlaybackPage : public QWidget {
    Q_OBJECT
  public:
    PlaybackPage(AppState *, SessionController *, QWidget *parent = nullptr);
    VideoView *video;
    ChartView *chart;

  protected:
    void resizeEvent(QResizeEvent *) override;

  private:
    AppState *m_state;
    SessionController *m_controller;
    QSlider *m_progress;
    QPushButton *m_play;
    QLabel *m_time, *m_count;
    QVBoxLayout *m_list;
    void rebuild();
    void refresh();
};
} // namespace ui
