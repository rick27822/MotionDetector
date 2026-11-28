#pragma once
#include "ICameraSource.h"
#include <QMutex>
#include <optional>

namespace camera {
struct CapturedFrame {
    cv::Mat bgr;
    QString session;
    qint64 id, timestampMs;
};

// One mailbox per inference worker. Only a wake-up is queued to the worker.
class LatestCameraFrame : public QObject {
    Q_OBJECT
public:
    explicit LatestCameraFrame(ICameraSource &source, QObject *parent = nullptr) : QObject(parent) {
        connect(&source, &ICameraSource::sourceFrameReady, this,
                [this](const cv::Mat &bgr,const QString &sid,qint64 id,qint64 ms) {
            bool wake;
            {
                QMutexLocker lock(&mutex);
                wake = !latest.has_value();
                latest = CapturedFrame{bgr,sid,id,ms};
            }
            if (wake) emit available();
        },Qt::DirectConnection);
    }
    std::optional<CapturedFrame> takeLatest() {
        QMutexLocker lock(&mutex);
        auto result = std::move(latest);
        latest.reset();
        return result;
    }
signals:
    void available();
private:
    QMutex mutex;
    std::optional<CapturedFrame> latest;
};
}
