#pragma once
#include "app/AppState.h"
#include "modules/ui/controllers/Controllers.h"
#include "modules/ui/controllers/RealtimeController.h"
#include "modules/ui/adapters/PoseReplay.h"
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
    // Call on the GUI thread when the producer is ready. Capture the returned
    // token in the poseReady binding; old-generation callbacks are rejected.
    QString beginRealtimeInput(const QString &sourceName);
    void endRealtimeInput(const QString &sessionToken);
    bool startPoseReplay(const QString &path, QString &error);
    ui::RealtimeController *realtime() const { return m_realtime; }
    ui::PoseReplay *replay() const { return m_replay; }
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
    ui::RealtimeController *m_realtime;
    ui::PoseReplay *m_replay;
    ui::SettingsController *m_settings;
    ui::DeviceController *m_devices;
    ui::DetectionController *m_detection;
    ui::SessionController *m_sessions;
    ui::Sidebar *m_sidebar;
    QStackedWidget *m_pages;
    QLabel *m_title, *m_recordStatus, *m_backendStatus, *m_toast = nullptr;
    QTimer *m_toastTimer;
    QString m_lastMessage;
    QString activateRealtimeInput(const QString &sourceName);
    void showFeedback(const QString &, bool);
};
