#pragma once
#include "app/AppState.h"
#include "modules/ui/controllers/Controllers.h"
#include <QLabel>
#include <QMainWindow>
#include <QStackedWidget>
class QTimer;
namespace ui {
class Sidebar;
}
class MainWindow : public QMainWindow {
    Q_OBJECT
  public:
    explicit MainWindow(const QString &dataDirectory = {}, QWidget *parent = nullptr);
    void navigate(ui::PageId page);
    void setScenario(const QString &scenario);
    ui::PageId currentPage() const;
    ui::AppState *state() const {
        return m_state;
    }
    ui::SettingsController *settings() const {
        return m_settings;
    }
    ui::DeviceController *devices() const {
        return m_devices;
    }
    ui::DetectionController *detection() const {
        return m_detection;
    }
    ui::SessionController *sessions() const {
        return m_sessions;
    }
    QStackedWidget *pages() const {
        return m_pages;
    }
    QString lastMessage() const {
        return m_lastMessage;
    }

  protected:
    void closeEvent(QCloseEvent *) override;
    void resizeEvent(QResizeEvent *) override;

  private:
    ui::AppState *m_state;
    ui::SettingsController *m_settings;
    ui::DeviceController *m_devices;
    ui::DetectionController *m_detection;
    ui::SessionController *m_sessions;
    ui::Sidebar *m_sidebar;
    QStackedWidget *m_pages;
    QLabel *m_title, *m_recordStatus, *m_backendStatus, *m_toast = nullptr;
    QTimer *m_toastTimer;
    QString m_lastMessage;
    void showFeedback(const QString &, bool);
};
