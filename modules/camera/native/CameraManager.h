#pragma once
#include "ICameraSource.h"
#include <QJsonValue>
#include <QJsonObject>
#include <QProcess>
#include <QTimer>

namespace camera {
class CameraManager : public ICameraSource {
    Q_OBJECT
  public:
    explicit CameraManager(QObject *parent = nullptr);
    ~CameraManager() override;
    void start(const QJsonValue &source, const QString &phoneHost = {});
    void stop();
    void setCaptureSettings(QSize size, int fps, bool adaptive);
    bool startRecording(const QString &directory) override;
    void stopRecording() override;
    QList<DeviceInfo> discoverDevices() override;
    DeviceInfo deviceInfo() const override { return device; }
    bool connectDevice(const QString &id) override;
    void disconnect() override;
    bool startStream() override;
    void stopStream() override;
    void setStreamParams(const QSize &size, float fps, const QString &protocol) override;
    void setAdaptiveQuality(bool value) override {
        setCaptureSettings(QSize(capture["width"].toInt(1280),capture["height"].toInt(720)),
                           capture["fps"].toInt(30),value);
    }
    QImage grabFrameForQR() const override { return latest; }
    bool submitAngles(const QString &sid, qint64 id, qint64 ms, const QJsonObject &angles) override;
  signals:
    void previewReady(const QImage &image, const QString &sessionId, qint64 sourceFrameId, qint64 timestampMs);
    void captureFailed();
    void sessionClosed();
    void captureMeasured(QSize size, double fps, const QString &session);
  private:
    QProcess process;
    QTimer stale;
    QJsonValue selected;
    QJsonObject capture;
    bool adaptiveQuality = true;
    QString host, session;
    qint64 generation = 0, lastId = -1, lastMs = -1;
    bool ready = false, accepting = false;
    void request();
    void readMessages();
    void closeSession();
    void failCapture(const QString &message);
    void setState(ConnectionState state);
    DeviceInfo device;
    QImage latest;
    QString recordingId;
    QString endpoint;
};
}
