#pragma once
#include "app/AppState.h"
#include "vendor/analysis_realtime/RealtimeAnalysisService.h"
#include <QElapsedTimer>
#include <QMutex>
#include <QTimer>
#include <optional>

namespace ui {
// The service and all view state belong to the GUI thread. submitPose is the
// bounded cross-thread ingress; capture the session token when binding a source.
class RealtimeController : public QObject {
    Q_OBJECT
  public:
    explicit RealtimeController(AppState *, QObject *parent = nullptr);
    void beginInput(const QString &sourceName);
    bool enabled() const { return m_enabled; }
    bool active() const { return m_active; }
    QString sessionId() const;
    QString sourceName() const { return m_sourceName; }
    const core::PoseResult &pose() const { return m_pose; }
    const motion::RealtimeAnalysisResult &result() const { return m_result; }
    quint64 droppedFrames() const;
    // Thread safe. One pending frame and one queued wake-up at most.
    void submitPose(const core::PoseResult &, const QString &sessionToken);

  public slots:
    // Safe for a DirectConnection from poseReady: cross-thread calls enter the mailbox.
    void onPoseReady(const core::PoseResult &, const QString &sessionToken);
    void poll();

  signals:
    void changed();
    void sessionStarted(const QString &sessionToken);
    void sessionEnded(const QString &sessionToken, quint64 completedReps);
    void repCompleted(const QString &sessionToken, quint64 completedReps);

  private:
    struct Pending { core::PoseResult pose; QString token; qint64 receivedAt; };
    AppState *m_state;
    motion::RealtimeAnalysisService m_service;
    motion::RealtimeAnalysisResult m_result;
    core::PoseResult m_pose;
    QElapsedTimer m_clock;
    QTimer m_timer;
    bool m_enabled = false, m_active = false;
    QString m_sourceName;
    qint64 m_startedAt = 0, m_receivedAt = 0, m_poseTimestamp = -1;
    mutable QMutex m_mutex;
    QString m_token;
    std::optional<Pending> m_pending;
    bool m_scheduled = false;
    quint64 m_dropped = 0;
    void synchronize();
    void drain();
    void accept(const core::PoseResult &, const QString &, qint64 receivedAt);
    void publish(const motion::RealtimeAnalysisResult &, bool force = false);
    void clearPose();
};
}
