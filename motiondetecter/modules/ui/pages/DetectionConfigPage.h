#pragma once
#include "app/AppState.h"
#include "modules/ui/controllers/Controllers.h"
#include "modules/ui/widgets/Controls.h"
namespace ui {
class DetectionConfigPage : public QWidget {
    Q_OBJECT
  public:
    DetectionConfigPage(AppState *, SettingsController *, QWidget *parent = nullptr);
    UiConfig draft() const;

  private:
    AppState *m_state;
    SliderField *m_conf, *m_smoothing;
    ToggleField *m_interpolate;
    QMap<QString, QPushButton *> m_points;
    QPushButton *m_save;
    QLabel *m_status, *m_count;
    void load(const UiConfig &);
    void updateCount();
};
} // namespace ui
