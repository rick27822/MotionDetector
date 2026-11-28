#pragma once
#include <QImage>
#include <QJsonObject>
#include <QObject>
#include <opencv2/core/mat.hpp>

namespace camera {
enum class ConnectionState { Disconnected, Selected, Opening, Streaming, Waiting, Error };
struct DeviceInfo {
    QString id, name, protocol;
    QSize resolution;
    double fps = 0, rttMs = -1;
    int battery = -1;
    ConnectionState state = ConnectionState::Disconnected;
};

class ICameraSource : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual QList<DeviceInfo> discoverDevices() = 0;
    virtual DeviceInfo deviceInfo() const = 0;
    virtual bool connectDevice(const QString &id) = 0;
    virtual void disconnect() = 0;
    virtual bool startStream() = 0;
    virtual void stopStream() = 0;
    virtual bool startRecording(const QString &directory) = 0;
    virtual void stopRecording() = 0;
    virtual void setStreamParams(const QSize &resolution, float fps, const QString &protocol) = 0;
    virtual void setAdaptiveQuality(bool enabled) = 0;
    virtual QImage grabFrameForQR() const = 0;
    virtual bool submitAngles(const QString &session, qint64 id, qint64 ms,
                              const QJsonObject &anglesDeg) = 0;
signals:
    void frameReady(const cv::Mat &bgr, qint64 timestampMs);
    void sourceFrameReady(const cv::Mat &bgr, const QString &session, qint64 id, qint64 timestampMs);
    void deviceListChanged(const QList<camera::DeviceInfo> &devices);
    void connectionStateChanged(camera::ConnectionState state);
    void pairingReady(const QImage &qr);
    void statusChanged(const QString &message);
    void recordingChanged(const QJsonObject &event);
};
}
