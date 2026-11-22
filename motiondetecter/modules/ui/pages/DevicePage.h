#pragma once
#include "app/AppState.h"
#include "modules/ui/controllers/Controllers.h"
#include <QLabel>
#include <QPushButton>
#include <QWidget>
namespace ui {
class DevicePage : public QWidget {
    Q_OBJECT
  public:
    DevicePage(AppState *, DeviceController *, QWidget *parent = nullptr);

  private:
    AppState *m_state;
    DeviceController *m_controller;
    QPushButton *m_iphone, *m_xiaomi, *m_refresh, *m_disconnect;
    QLabel *m_info, *m_status;
    QVector<QLabel *> m_infoValues;
    void refresh();
};
} // namespace ui
