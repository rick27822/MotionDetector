#include "CameraManager.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtEndian>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QUrl>
#include <cmath>
#ifdef Q_OS_WIN
#include <mfapi.h>
#include <mfidl.h>
#endif

namespace camera {
CameraManager::CameraManager(QObject *parent) : ICameraSource(parent) {
    process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    stale.setSingleShot(true);
    connect(&stale, &QTimer::timeout, this, [this] { emit statusChanged("等待新的相机画面"); });
    connect(&process, &QProcess::readyReadStandardOutput, this, &CameraManager::readMessages);
    connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            failCapture("无法启动采集程序，请检查 Python 路径：" + process.errorString());
    });
    connect(&process, &QProcess::finished, this, [this] {
        stale.stop();
        ready = accepting = false;
        emit recordingChanged({{"state","exited"}});
        failCapture("采集进程已退出，请重新开始");
    });
}
CameraManager::~CameraManager() {
    QObject::disconnect(&process, nullptr, this, nullptr);
    process.closeWriteChannel();
    if (process.state() != QProcess::NotRunning && !process.waitForFinished(12000)) {
        process.kill();
        process.waitForFinished(1500);
    }
}
bool CameraManager::startRecording(const QString &directory) {
    if (!accepting || session.isEmpty() || directory.isEmpty()) return false;
    QJsonObject cmd{{"command","startRecording"},{"directory",directory},{"requestId",QString::number(generation)}};
    return process.write(QJsonDocument(cmd).toJson(QJsonDocument::Compact) + '\n') >= 0;
}
bool CameraManager::submitAngles(const QString &sid, qint64 id, qint64 ms, const QJsonObject &angles) {
    if (recordingId.isEmpty() || sid != session || id < 0 || ms < 0 || angles.isEmpty()) return false;
    QJsonObject cmd{{"command","angles"},{"recordingId",recordingId},{"sessionId",sid},
                    {"sourceFrameId",id},{"timestampMs",ms},{"anglesDeg",angles}};
    return process.write(QJsonDocument(cmd).toJson(QJsonDocument::Compact) + '\n') >= 0;
}
void CameraManager::stopRecording() {
    QJsonObject cmd{{"command","stopRecording"},{"requestId",QString::number(generation)}};
    process.write(QJsonDocument(cmd).toJson(QJsonDocument::Compact) + '\n');
}
void CameraManager::request() {
    if (!ready) return;
    QJsonObject cmd{{"requestId", QString::number(generation)},
                    {"command", selected.isNull() ? "stop" : "start"}};
    if (!selected.isNull()) {
        cmd["source"] = selected;
        cmd["capture"] = capture;
        cmd["adaptiveQuality"] = adaptiveQuality;
        if (selected == "phone") cmd["host"] = host;
    }
    process.write(QJsonDocument(cmd).toJson(QJsonDocument::Compact) + '\n');
}
void CameraManager::setCaptureSettings(QSize size, int fps, bool adaptive) {
    const QJsonObject target{{"width",size.width()},{"height",size.height()},{"fps",fps}};
    if (capture == target && adaptive == adaptiveQuality) return;
    capture = target;
    adaptiveQuality = adaptive;
    if (ready && !selected.isNull()) {
        const QJsonObject cmd{{"command","setCapture"},{"capture",capture},{"adaptiveQuality",adaptive},
                              {"requestId","settings"}};
        process.write(QJsonDocument(cmd).toJson(QJsonDocument::Compact)+'\n');
    }
}
void CameraManager::setState(ConnectionState state) {
    if (device.state == state) return;
    device.state = state;
    emit connectionStateChanged(state);
}
QList<DeviceInfo> CameraManager::discoverDevices() {
    QList<DeviceInfo> devices;
#ifdef Q_OS_WIN
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if ((SUCCEEDED(com) || com == RPC_E_CHANGED_MODE) && SUCCEEDED(MFStartup(MF_VERSION))) {
        IMFAttributes *attributes = nullptr;
        IMFActivate **sources = nullptr;
        UINT32 count = 0;
        if (SUCCEEDED(MFCreateAttributes(&attributes, 1)) &&
            SUCCEEDED(attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                                          MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID)) &&
            SUCCEEDED(MFEnumDeviceSources(attributes, &sources, &count))) {
            for (UINT32 i = 0; i < count; ++i) {
                WCHAR *name = nullptr;
                UINT32 length = 0;
                sources[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &name, &length);
                devices.append(DeviceInfo{QString("usb:%1").arg(i),
                    name ? QString::fromWCharArray(name, length) : QString("摄像头 %1").arg(i), "USB"});
                CoTaskMemFree(name);
                sources[i]->Release();
            }
            CoTaskMemFree(sources);
        }
        if (attributes) attributes->Release();
        MFShutdown();
    }
    if (SUCCEEDED(com)) CoUninitialize();
#elif defined(Q_OS_LINUX)
    for (const auto &file : QDir("/dev").entryList({"video[0-9]*"}, QDir::System))
        devices.append(DeviceInfo{"usb:"+file.mid(5),file,"USB"});
#endif
    for (const auto &addr : QNetworkInterface::allAddresses()) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol && !addr.isLoopback())
            devices.append(DeviceInfo{"phone:"+addr.toString(),"手机扫码入口 · "+addr.toString(),"WebRTC"});
    }
    for (auto &item : devices) if (item.id == device.id) { auto name=item.name; item=device; item.name=name; }
    emit deviceListChanged(devices);
    return devices;
}
bool CameraManager::connectDevice(const QString &id) {
    const QString address = id.section(':',1);
    bool ok = false;
    const int index = address.toInt(&ok);
    const bool usb = id.startsWith("usb:") && ok && index >= 0 && index < 100;
    const bool phone = id.startsWith("phone:") && !QHostAddress(address).isNull();
    const QUrl url(id, QUrl::StrictMode);
    const bool rtsp = url.scheme()=="rtsp" && url.isValid() && !url.host().isEmpty();
    if (!usb && !phone && !rtsp && id != "demo") return false;
    if (endpoint != id) stop();
    endpoint = id;
    device.id = rtsp ? "rtsp:"+url.host() : id;
    device.protocol = rtsp ? "RTSP" : phone ? "WebRTC" : usb ? "USB" : "demo";
    device.name = rtsp ? "RTSP 网络摄像头" : phone ? "手机摄像头" : usb ? "电脑摄像头 "+address : "合成演示";
    if (selected.isNull()) setState(ConnectionState::Selected);
    return true;
}
bool CameraManager::startStream() {
    if (device.id.isEmpty()) return false;
    if (!selected.isNull()) return true;
    const auto address = device.id.section(':',1);
    if (device.protocol=="RTSP") start(endpoint);
    else if (device.id.startsWith("phone:")) start("phone",address);
    else if (device.id == "demo") start("demo");
    else {
        int index = address.toInt();
#ifdef Q_OS_WIN
        index += 1400; // OpenCV CAP_MSMF, matching MFEnumDeviceSources order.
#endif
        start(index);
    }
    return !selected.isNull();
}
void CameraManager::stopStream() { stop(); }
void CameraManager::disconnect() {
    stop();
    device = {};
    endpoint.clear();
    emit connectionStateChanged(device.state);
}
void CameraManager::setStreamParams(const QSize &size, float fps, const QString &protocol) {
    if (size.width()<16 || size.width()>3840 || size.height()<16 || size.height()>2160 ||
        !std::isfinite(fps) || fps<1 || fps>60 || fps!=std::floor(fps) ||
        (!protocol.isEmpty() && protocol.compare(device.protocol,Qt::CaseInsensitive)!=0)) {
        emit statusChanged("采集参数或协议不受当前设备支持");
        return;
    }
    setCaptureSettings(size, int(fps), adaptiveQuality);
}
void CameraManager::start(const QJsonValue &source, const QString &phoneHost) {
    stop();
    selected = source;
    setState(ConnectionState::Opening);
    host = phoneHost;
    ++generation;
    emit statusChanged("正在打开相机…");
    if (process.state() != QProcess::NotRunning) { request(); return; }
    ready = false;
    QDir folder(qEnvironmentVariable("POSELAB_CAMERA_DIR", "modules/camera"));
    const QString script = folder.absoluteFilePath("camera_runner.py");
#ifdef Q_OS_WIN
    const QString venv = ".venv/Scripts/python.exe";
#else
    const QString venv = ".venv/bin/python";
#endif
    const QString python = qEnvironmentVariable("POSELAB_PYTHON", folder.absoluteFilePath(venv));
    if (!QFileInfo::exists(script)) {
        failCapture("找不到 camera_runner.py，请设置 POSELAB_CAMERA_DIR");
        return;
    }
    process.start(python, {script});
}
void CameraManager::stop() {
    ++generation;
    selected = QJsonValue(QJsonValue::Null);
    accepting = false;
    closeSession();
    lastId = lastMs = -1;
    stale.stop();
    latest = {};
    recordingId.clear();
    device.rttMs = device.battery = -1;
    device.resolution = {};
    device.fps = 0;
    setState(device.id.isEmpty() ? ConnectionState::Disconnected : ConnectionState::Selected);
    request();
}
void CameraManager::closeSession() {
    if (session.isEmpty()) return;
    session.clear();
    emit sessionClosed();
}
void CameraManager::failCapture(const QString &message) {
    if (selected.isNull()) return;
    selected = QJsonValue(QJsonValue::Null);
    accepting = false;
    stale.stop();
    closeSession();
    emit captureFailed();
    latest = {};
    recordingId.clear();
    device.rttMs = device.battery = -1;
    device.fps = 0;
    device.resolution = {};
    setState(ConnectionState::Error);
    emit statusChanged(message);
}
void CameraManager::readMessages() {
    auto invalid = [this] {
        failCapture("相机数据格式错误");
        process.kill();
    };
    while (process.bytesAvailable() >= 4) {
        const auto prefix = process.peek(4);
        const auto len = qFromBigEndian<quint32>(prefix.constData());
        if (!len || len > 65536) return invalid();
        if (process.bytesAvailable() < 4 + len) return;
        const auto h = QJsonDocument::fromJson(process.peek(4+len).mid(4)).object();
        const auto bytes = h["payloadLength"].toInteger(-1);
        if (h["version"].toInt() != 1 || bytes < 0 || bytes > 32 * 1024 * 1024) return invalid();
        const auto total = 4 + len + bytes;
        if (process.bytesAvailable() < total) return;
        if (process.skip(4+len) != 4+len) return invalid();
        const auto type = h["type"].toString();
        const bool ours = h["requestId"].toString() == QString::number(generation);
        if (type == "pairing" && ours) {
            const auto qr = QImage::fromData(process.read(bytes),"PNG");
            if (qr.isNull()) return invalid();
            emit pairingReady(qr);
            continue;
        }
        if (type == "frame" && !accepting) {
            if (process.skip(bytes) != bytes) return invalid();
            continue;
        }
        if (type != "frame" && process.skip(bytes) != bytes) return invalid();
        if (type == "ready") { ready = true; request(); }
        else if (type == "recording") {
            if (h["state"]=="started" && ours) recordingId=h["recordingId"].toString();
            else if (h["state"]!="started") recordingId.clear();
            emit recordingChanged(h);
        }
        else if (type == "ack" && ours) accepting = h["command"] == "start";
        else if ((type == "error" && ours) || (type == "status" && accepting)) {
            stale.stop();
            const auto state = h["state"].toString();
            if (type == "error" || state == "error" || state == "end_of_stream") {
                failCapture(h["message"].toString());
            } else {
                if (state == "disconnected") {
                    closeSession();
                    latest = {};
                    device.fps = 0;
                    device.rttMs = device.battery = -1;
                    device.resolution = {};
                    setState(ConnectionState::Waiting);
                }
                emit statusChanged(h["message"].toString());
            }
        } else if (type == "frame" && accepting) {
            const int w = h["width"].toInt(), height = h["height"].toInt();
            const qint64 stride = h["stride"].toInteger(-1);
            if (w <= 0 || height <= 0 || stride != qint64(w)*3 || stride > bytes ||
                stride*height != bytes || h["pixelFormat"] != "BGR8") return invalid();
            const QString sid = h["sessionId"].toString();
            const auto id = h["sourceFrameId"].toInteger(-1), ms = h["timestampMs"].toInteger(-1);
            if (sid.isEmpty() || id < 0 || ms < 0 ||
                (session == sid && (id <= lastId || ms <= lastMs))) return invalid();
            if (session != sid) closeSession();
            session = sid;
            lastId = id;
            lastMs = ms;
            stale.start(550);
            cv::Mat frame(height,w,CV_8UC3);
            if (process.read(reinterpret_cast<char *>(frame.data),bytes) != bytes) return invalid();
            auto *owner = new cv::Mat(frame);
            latest = QImage(static_cast<const uchar *>(owner->data),w,height,stride,QImage::Format_BGR888,
                            [](void *p){ delete static_cast<cv::Mat *>(p); },owner);
            device.resolution = QSize(w,height);
            device.fps = h["captureFps"].toDouble();
            device.rttMs = h["rttMs"].toDouble(-1);
            device.battery = h["battery"].toInt(-1);
            setState(ConnectionState::Streaming);
            emit previewReady(latest,sid,id,ms);
            emit sourceFrameReady(frame,sid,id,ms);
            emit frameReady(frame,ms);
            if (h.contains("captureFps")) emit captureMeasured(QSize(w,height), h["captureFps"].toDouble(), sid);
        }
    }
}
}
