#include "CameraManager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QTemporaryDir>
#include <QTimer>
#include <QJsonDocument>
#include <cstdio>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    camera::CameraManager manager;
    camera::ICameraSource &camera = manager;
    const auto devices = camera.discoverDevices();
    const bool physical = app.arguments().contains("--pc");
    QString id = "demo";
    if (physical) {
        id.clear();
        for (const auto &device : devices)
            if (device.protocol == "USB") { id = device.id; break; }
        if (id.isEmpty()) return 2;
    }
    QTemporaryDir temporary;
    const QString directory = app.arguments().value(1, temporary.path());
    const QString output = directory.startsWith("--") ? temporary.path() : directory;
    int frames = 0;
    bool recording = false, finishing = false;
    cv::Mat retained;
    qint64 firstTime = -1, lastTime = -1;
    QObject::connect(&camera, &camera::ICameraSource::sourceFrameReady, &app,
                     [&](const cv::Mat &bgr, const QString &session, qint64 frame, qint64 ms) {
        if (bgr.empty() || bgr.type()!=CV_8UC3 || ms<=lastTime) { app.exit(3); return; }
        lastTime = ms;
        if (++frames == 1) {
            firstTime = ms;
            retained = bgr;
            if (!camera.startRecording(output)) app.exit(4);
        }
        if (recording && !finishing) {
            camera.submitAngles(session,frame,ms,{{"leftKnee",92.5}});
            if (ms-firstTime >= 4000) {
                finishing = true;
                camera.stopRecording();
            }
        }
    });
    QObject::connect(&camera,&camera::ICameraSource::recordingChanged,&app,[&](const QJsonObject &e) {
        if (e["state"]=="started") recording = true;
        if (e["state"]=="failed") app.exit(5);
        if (e["state"]!="finished") return;
        const auto preview = camera.grabFrameForQR();
        const QJsonObject summary{{"delivered",frames},{"fps",(frames-1)*1000.0/(lastTime-firstTime)},
            {"captureFps",camera.deviceInfo().fps},{"recorded",e["frames"]},{"framesWithAngles",e["framesWithAngles"]},
            {"size",QString("%1x%2").arg(retained.cols).arg(retained.rows)},{"endReason",e["endReason"]}};
        std::puts(QJsonDocument(summary).toJson(QJsonDocument::Compact).constData());
        const bool ok = !retained.empty() && !preview.isNull() && e["framesWithAngles"].toInt()>0
                        && e["endReason"]=="stopped";
        camera.disconnect();
        app.exit(ok ? 0 : 6);
    });
    QObject::connect(&camera,&camera::ICameraSource::statusChanged,&app,[](const QString &s){ qInfo().noquote()<<s; });
    if (!camera.connectDevice(id)) return 7;
    camera.setStreamParams(QSize(640,480),30,physical ? "USB" : "demo");
    if (!camera.startStream()) return 8;
    QTimer::singleShot(25000,&app,[&]{ app.exit(9); });
    return app.exec();
}
