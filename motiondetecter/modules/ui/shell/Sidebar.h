#pragma once
#include "app/AppState.h"
#include <QButtonGroup>
#include <QLabel>
#include <QWidget>
namespace ui {
class Sidebar : public QWidget {
    Q_OBJECT
  public:
    Sidebar(AppState *, QWidget *parent = nullptr);
    void setCurrent(PageId id);
    QButtonGroup *navigation;

  private:
    AppState *m_state;
    QLabel *m_device, *m_detail;
  signals:
    void pageRequested(ui::PageId);
};
} // namespace ui
