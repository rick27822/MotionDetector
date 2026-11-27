#include <QApplication>
#include <QElapsedTimer>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPixmap>
#include <QProcess>
#include <QTimer>
#include <QtEndian>
#include <cstdio>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() < 4) {
        std::fprintf(stderr, "camera_preview <python> <camera_runner.py> <camera index|demo|file|phone> [LAN-IP|--check]\n");
        return 2;
    }
    const bool check = args.contains("--check");
    const bool phone = args[3] == "phone";
    if (phone && (args.size() != 5 || check)) return 2;
    bool numeric = false;
    const int device = args[3].toInt(&numeric);
    const QJsonValue source = numeric ? QJsonValue(device) : QJsonValue(args[3]);
    QLabel view;
    view.setWindowTitle(QStringLiteral("4.2 Camera / Qt"));
    view.resize(800, 450);
    view.setAlignment(Qt::AlignCenter);
    if (!check) view.show();

    QProcess worker;
    worker.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    auto command = [&](const QString &name) {
        QJsonObject cmd{{"command", name}, {"requestId", name}};
        if (name == "start") cmd["source"] = source;
        if (phone && name == "start") cmd["host"] = args[4];
        worker.write(QJsonDocument(cmd).toJson(QJsonDocument::Compact) + '\n');
    };
    QByteArray buffer;
    QString current, previous;
    qint64 lastId = -1, lastTime = -1;
    int frames = 0, runs = 0;
    bool stopping = false, passed = false;
    QElapsedTimer timer;
    QPixmap qr;
    auto fail = [&](const QString &reason) {
        qWarning().noquote() << reason;
        app.exit(1);
    };
    QObject::connect(&worker, &QProcess::readyReadStandardOutput, [&] {
        buffer += worker.readAllStandardOutput();
        while (buffer.size() >= 4) {
            auto size = qFromBigEndian<quint32>(buffer.constData());
            if (!size || size > 65536) return fail("Invalid header length");
            if (buffer.size() < 4 + size) return;
            const auto header = QJsonDocument::fromJson(buffer.mid(4, size)).object();
            const auto bytes = header["payloadLength"].toInteger(-1);
            if (header["version"].toInt() != 1 || bytes < 0 || bytes > 32 * 1024 * 1024)
                return fail("Invalid message header");
            const qsizetype end = 4 + size + bytes;
            if (buffer.size() < end) return;
            const auto type = header["type"].toString();
            if (type == "ready") {
                command("start");
            } else if (type == "pairing" && phone) {
                if (!qr.loadFromData(buffer.mid(4 + size, bytes), "PNG")) return fail("Invalid pairing QR");
                view.setPixmap(qr);
                view.setWindowTitle(QStringLiteral("PoseLab - Scan with your phone"));
                std::fprintf(stderr, "Pairing window ready\n");
                std::fflush(stderr);
            } else if (type == "frame" && !stopping) {
                const int w = header["width"].toInt(), h = header["height"].toInt();
                const qint64 stride = header["stride"].toInteger(-1);
                if (w <= 0 || h <= 0 || stride != qint64(w) * 3 ||
                    stride > bytes || stride * h != bytes || header["pixelFormat"] != "BGR8")
                    return fail("Invalid BGR frame");
                const QString session = header["sessionId"].toString();
                if (phone && !current.isEmpty() && session != current) {
                    previous = current;
                    current.clear();
                    lastTime = -1;
                    frames = 0;
                }
                const auto id = header["sourceFrameId"].toInteger(-1);
                const auto ts = header["timestampMs"].toInteger(-1);
                if (session.isEmpty() || session == previous || (!current.isEmpty() && session != current)
                    || id <= lastId || ts <= lastTime) return fail("Frame/session order mismatch");
                if (!frames) timer.start();
                current = session;
                lastId = id;
                lastTime = ts;
                const QImage frame(reinterpret_cast<const uchar *>(buffer.constData() + 4 + size),
                                   w, h, stride, QImage::Format_BGR888);
                view.setPixmap(QPixmap::fromImage(frame.copy()).scaled(view.size(), Qt::KeepAspectRatio));
                ++frames;
                if (frames == 30) {
                    std::fprintf(stderr, "Qt received 30 frames %dx%d elapsedMs=%lld session=%.8s\n",
                                 w, h, static_cast<long long>(timer.elapsed()), qPrintable(current));
                    std::fflush(stderr);
                }
                if (frames == 30 && check) {
                    stopping = true;
                    command("stop");
                }
            } else if (type == "ack" && header["command"] == "stop" && check) {
                if (++runs == 2) {
                    passed = true;
                    worker.closeWriteChannel();
                } else {
                    previous = current;
                    current.clear();
                    lastId = lastTime = -1;
                    frames = 0;
                    stopping = false;
                    command("start");
                }
            } else if (type == "status" || type == "error") {
                view.clear();
                view.setText(header["message"].toString());
                if (phone && type == "status" && !qr.isNull()) {
                    view.setPixmap(qr);
                    std::fprintf(stderr, "Phone state: %s\n", qPrintable(header["state"].toString()));
                    std::fflush(stderr);
                }
                if (check || type == "error") return fail(view.text());
            }
            buffer.remove(0, end);
        }
    });
    QObject::connect(&worker, &QProcess::errorOccurred, [&](QProcess::ProcessError) { fail(worker.errorString()); });
    QObject::connect(&worker, &QProcess::finished, [&](int code, QProcess::ExitStatus status) {
        app.exit(code == 0 && status == QProcess::NormalExit && (!check || passed) ? 0 : 1);
    });
    if (check) QTimer::singleShot(45000, [&] { fail("Frame test timed out"); });
    worker.start(args[1], {args[2]});
    const int result = app.exec();
    if (worker.state() != QProcess::NotRunning) {
        // Keep draining stdout while the producer processes stdin EOF.
        worker.closeWriteChannel();
        QElapsedTimer drain;
        drain.start();
        while (worker.state() != QProcess::NotRunning && drain.elapsed() < 4000) {
            worker.waitForReadyRead(50);
            worker.readAllStandardOutput();
        }
        if (worker.state() != QProcess::NotRunning) { worker.kill(); worker.waitForFinished(1000); }
    }
    return result;
}
