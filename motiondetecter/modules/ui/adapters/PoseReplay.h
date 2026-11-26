#pragma once
#include "modules/ui/controllers/RealtimeController.h"
#include <QElapsedTimer>
#include <QTimer>
namespace ui {
// Paced file input exercises the exact P1 controller used by live poseReady.
// Angle-only JSONL intentionally has no skeleton or corresponding video.
class PoseReplay : public QObject {
    Q_OBJECT
  public:
    explicit PoseReplay(RealtimeController *, QObject *parent = nullptr);
    bool load(const QString &path, QString &error);
    bool loaded() const { return !m_frames.isEmpty(); }
    void start(const QString &sessionToken);
    void stop();
    void clear() { stop(); m_frames.clear(); }
  signals:
    void finished();
  private:
    RealtimeController *m_target;
    QVector<core::PoseResult> m_frames;
    QTimer m_timer;
    QElapsedTimer m_clock;
    QString m_token;
    int m_index = 0;
    void tick();
};
}
