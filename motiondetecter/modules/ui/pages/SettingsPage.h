#pragma once
#include "app/AppState.h"
#include "modules/ui/controllers/Controllers.h"
#include "modules/ui/widgets/Controls.h"
#include <QComboBox>
#include <QSpinBox>
namespace ui {
class SettingsPage : public QWidget {
    Q_OBJECT
  public:
    SettingsPage(AppState *, SettingsController *, QWidget *parent = nullptr);

  private:
    AppState *m_state;
    SettingsController *m_controller;
    QComboBox *m_resolution, *m_fps;
    QSpinBox *m_threads;
    ToggleField *m_mirror;
    QPushButton *m_path;
    QLabel *m_status;
    bool m_loading = false;
    void load();
    void apply();
};
} // namespace ui
