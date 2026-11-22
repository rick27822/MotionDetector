#pragma once
#include "app/AppState.h"
#include "modules/ui/controllers/Controllers.h"
#include "modules/ui/widgets/ChartView.h"
#include "modules/ui/widgets/Controls.h"
namespace ui {
class AnalysisPage : public QWidget {
    Q_OBJECT
  public:
    AnalysisPage(AppState *, SessionController *, QWidget *parent = nullptr);

  private:
    AppState *m_state;
    SessionController *m_controller;
    QVector<QLabel *> m_metrics;
    QLabel *m_heading, *m_status;
    ScoreRing *m_ring;
    ChartView *m_chart;
    QPushButton *m_csv, *m_report;
    void refresh();
    void exportFile(bool csv);
};
} // namespace ui
