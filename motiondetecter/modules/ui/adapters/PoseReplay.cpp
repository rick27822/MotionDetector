#include "PoseReplay.h"
#include "PoseSampleReader.h"
#include <QFileInfo>
namespace ui {
PoseReplay::PoseReplay(RealtimeController *target, QObject *parent)
    : QObject(parent), m_target(target) {
    m_timer.setInterval(10);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &PoseReplay::tick);
    connect(target, &RealtimeController::sessionStarted, this, &PoseReplay::start);
    connect(target, &RealtimeController::sessionEnded, this, &PoseReplay::stop);
}
bool PoseReplay::load(const QString &path, QString &error) {
    if (QFileInfo(path).size() > 16 * 1024 * 1024) {
        error = "联调姿态文件超过16MB，请截取短片段";
        return false;
    }
    QVector<core::PoseResult> poses;
    if (path.endsWith(".jsonl", Qt::CaseInsensitive)) {
        QVector<core::AngleSample> samples;
        if (!PoseSampleReader::readSeries(path, samples, error))
            return false;
        int id = 0;
        for (const auto &sample : samples) {
            core::PoseResult pose;
            pose.timestampMs = sample.t;
            pose.frameId = id++;
            pose.angles = sample.deg;
            pose.validAngles = sample.valid;
            pose.detected = sample.valid != 0;
            poses.append(pose);
        }
    } else if (!PoseSampleReader::readPoses(path, poses, error)) {
        return false;
    }
    stop();
    m_frames = std::move(poses);
    return true;
}
void PoseReplay::start(const QString &token) {
    if (m_frames.isEmpty())
        return;
    m_token = token;
    m_index = 0;
    m_clock.start();
    m_timer.start();
    tick();
}
void PoseReplay::stop() {
    m_timer.stop();
    m_token.clear();
}
void PoseReplay::tick() {
    // Bounded work per event keeps stop/disconnect responsive even for dense files.
    int delivered = 0;
    while (m_index < m_frames.size() && delivered < 8 &&
           m_frames[m_index].timestampMs - m_frames.first().timestampMs <= m_clock.elapsed()) {
        m_target->onPoseReady(m_frames[m_index++], m_token);
        ++delivered;
    }
    if (m_index == m_frames.size()) {
        m_timer.stop();
        emit finished();
    }
}
}
