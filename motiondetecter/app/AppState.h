#pragma once
#include "core/types.h"
#include <QJsonObject>
#include <QObject>
namespace ui {
enum class PageId { Realtime, Devices, DetectionConfig, Playback, Analysis, Settings };
struct UiConfig {
    float confidence = .50f, smoothing = .60f;
    bool interpolate = true, nose = false, mirror = false;
    QString resolution = "1920 × 1080";
    int fps = 30, threads = 4;
    QString storagePath;
    QStringList enabledPoints = {"左肩", "右肩", "左肘", "右肘", "左腕", "右腕",
                                 "左髋", "右髋", "左膝", "右膝", "左踝", "右踝"};
    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject &, UiConfig &, QString &error);
};
struct DemoSession {
    QString id, name, date, status = "样例会话";
    qint64 durationMs = 12573;
    QVector<core::AngleSample> samples;
};
class AppState : public QObject {
    Q_OBJECT
  public:
    explicit AppState(QObject *parent = nullptr) : QObject(parent) {}
    UiConfig config;
    QString deviceId = "iphone", deviceName = "iPhone 15 Pro";
    bool connected = true, detecting = true, recording = false;
    bool realtimeInput = false;
    bool showSkeleton = true, showAngles = true;
    QString sessionId = "sample-042";
    QVector<DemoSession> sessions;
  signals:
    void configChanged();
    void deviceChanged();
    void detectionChanged();
    void sessionsChanged();
    void sessionChanged();
};
} // namespace ui
