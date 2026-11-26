#include "RealtimeController.h"
#include "vendor/analysis_realtime/RealtimePoseAdapter.h"
#include <QMutexLocker>
#include <QThread>
#include <QUuid>
#include <cmath>
#include <limits>

namespace ui {
RealtimeController::RealtimeController(AppState *state, QObject *parent)
    : QObject(parent), m_state(state) {
    m_clock.start();
    m_timer.setInterval(50);
    connect(&m_timer, &QTimer::timeout, this, &RealtimeController::poll);
    connect(state, &AppState::deviceChanged, this, &RealtimeController::synchronize);
    connect(state, &AppState::detectionChanged, this, &RealtimeController::synchronize);
}
QString RealtimeController::sessionId() const {
    QMutexLocker lock(&m_mutex);
    return m_token;
}
quint64 RealtimeController::droppedFrames() const {
    QMutexLocker lock(&m_mutex);
    return m_dropped;
}
void RealtimeController::beginInput(const QString &name) {
    Q_ASSERT(QThread::currentThread() == thread());
    // An explicit source bind starts a new generation, even when already running.
    if (m_active) {
        m_state->detecting = false;
        synchronize();
    }
    m_enabled = true;
    m_sourceName = name;
    m_state->realtimeInput = true;
    m_state->deviceName = name;
    m_state->deviceId = "pose-input";
    m_state->connected = true;
    m_state->detecting = true;
    synchronize();
    emit m_state->deviceChanged();
    emit m_state->detectionChanged();
}
void RealtimeController::clearPose() { m_pose = {}; }
void RealtimeController::synchronize() {
    if (!m_enabled)
        return;
    const bool running = m_state->connected && m_state->detecting;
    if (running == m_active) {
        if (!running) {
            m_result.message = m_state->connected ? "检测已停止" : "设备已断开";
            emit changed();
        }
        return;
    }
    if (!running) {
        const auto oldToken = sessionId();
        const auto oldCount = m_result.completedReps;
        m_active = false;
        m_timer.stop();
        {
            QMutexLocker lock(&m_mutex);
            m_token.clear();
            m_pending.reset();
        }
        m_service.reset();
        m_poseTimestamp = -1;
        clearPose();
        m_result = {};
        m_result.message = m_state->connected ? "检测已停止" : "设备已断开";
        emit sessionEnded(oldToken, oldCount);
        emit changed();
        return;
    }
    m_service.reset();
    clearPose();
    m_poseTimestamp = -1;
    m_startedAt = m_clock.elapsed();
    const auto token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    {
        QMutexLocker lock(&m_mutex);
        m_token = token;
        m_pending.reset();
        m_dropped = 0;
    }
    m_active = true;
    m_result = {};
    m_result.sessionId = token.toStdString();
    m_result.message = "等待姿态数据";
    m_timer.start();
    emit changed();
    emit sessionStarted(token);
}
void RealtimeController::submitPose(const core::PoseResult &pose, const QString &token) {
    // The shared pose contract has exactly 33 landmarks; never copy an unbounded vector.
    if (pose.keypoints.size() > core::kKeypointCount)
        return;
    QMutexLocker lock(&m_mutex);
    if (token.isEmpty() || token != m_token)
        return;
    if (m_pending)
        ++m_dropped;
    m_pending = Pending{pose, token, m_clock.elapsed()};
    if (!m_scheduled) {
        m_scheduled = true;
        QMetaObject::invokeMethod(this, &RealtimeController::drain, Qt::QueuedConnection);
    }
}
void RealtimeController::drain() {
    std::optional<Pending> pending;
    {
        QMutexLocker lock(&m_mutex);
        pending.swap(m_pending);
        m_scheduled = false;
    }
    if (pending)
        accept(pending->pose, pending->token, pending->receivedAt);
}
void RealtimeController::onPoseReady(const core::PoseResult &pose, const QString &token) {
    if (QThread::currentThread() != thread()) {
        submitPose(pose, token);
        return;
    }
    accept(pose, token, m_clock.elapsed());
}
void RealtimeController::publish(const motion::RealtimeAnalysisResult &r, bool force) {
    bool different = r.code != m_result.code || r.message != m_result.message ||
                     r.completedReps != m_result.completedReps ||
                     r.repCompleted != m_result.repCompleted;
    m_result = r;
    if (different || force)
        emit changed();
}
void RealtimeController::accept(const core::PoseResult &pose, const QString &token,
                                qint64 receivedAt) {
    Q_ASSERT(QThread::currentThread() == thread());
    if (!m_active || token.isEmpty() || token != sessionId() ||
        pose.keypoints.size() > core::kKeypointCount)
        return;
    // Interrupt an unfinished rep after silence, even if the next frame's source
    // timestamp advances slowly. Poll never advances the service's last frame time.
    poll();
    auto clean = pose;
    const bool stale = m_clock.elapsed() - receivedAt > 500;
    if (stale)
        clean.detected = false;
    clean.validAngles &= core::kAllAngleBits;
    if (!clean.detected)
        clean.validAngles = 0;
    for (int i = 0; i < core::kAngleCount; ++i)
        if (!std::isfinite(clean.angles[i]) || clean.angles[i] < 0 || clean.angles[i] > 180)
            clean.validAngles &= ~(1u << i);
    for (auto &kp : clean.keypoints)
        if (!clean.detected || !std::isfinite(kp.x) || !std::isfinite(kp.y) ||
            !std::isfinite(kp.confidence) || kp.x < 0 || kp.x > 1 || kp.y < 0 || kp.y > 1 ||
            kp.confidence < 0 || kp.confidence > 1)
            kp.valid = false;
    auto r = m_service.processPose(motion::adaptRealtimePose<core::JointAngle>(clean,
                                                                            token.toStdString()));
    if (r.result.status == motion::RealtimeStatus::InvalidArgument) {
        clearPose();
    } else {
        m_receivedAt = receivedAt;
        m_poseTimestamp = clean.timestampMs;
        if (r.code == motion::RealtimeHintCode::DataLost)
            clearPose();
        else
            m_pose = clean;
    }
    // A mailbox frame that spent >500 ms waiting for the GUI must not appear current.
    if (r.result.status != motion::RealtimeStatus::InvalidArgument && stale) {
        // processPose already interrupted the cycle with invalid angles; no stale rep can count.
        r.code = motion::RealtimeHintCode::DataLost;
        r.message = "姿态帧等待过久，请检查输入负载";
        clearPose();
    }
    publish(r, true);
    if (r.repCompleted)
        emit repCompleted(token, r.completedReps);
}
void RealtimeController::poll() {
    Q_ASSERT(QThread::currentThread() == thread());
    if (!m_active)
        return;
    if (m_poseTimestamp < 0) {
        if (m_clock.elapsed() - m_startedAt > 500) {
            auto r = m_result;
            r.code = motion::RealtimeHintCode::DataLost;
            r.result.status = motion::RealtimeStatus::NoData;
            r.message = "未收到姿态数据，请检查输入连接";
            publish(r);
        }
        return;
    }
    const auto elapsed = qMax<qint64>(0, m_clock.elapsed() - m_receivedAt);
    // Saturation avoids signed overflow from a malformed source timestamp.
    const auto now = m_poseTimestamp > std::numeric_limits<qint64>::max() - elapsed
                         ? std::numeric_limits<qint64>::max() : m_poseTimestamp + elapsed;
    const auto r = m_service.poll(now);
    if (r.code == motion::RealtimeHintCode::DataLost)
        clearPose();
    publish(r);
}
}
