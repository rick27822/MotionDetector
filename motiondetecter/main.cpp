#include "mainwindow.h"
#include "modules/ui/theme/Theme.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("MotionDetector");
    QCoreApplication::setApplicationName("PoseLab-UI");
    QCoreApplication::setApplicationVersion("0.1.0");
    ui::applyTheme(app);
    QCommandLineParser parser;
    parser.setApplicationDescription("PoseLab · 六页 UI 与 P1 实时提示联调（支持姿态文件回放，未装配采集/推理）");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"data-dir", "独立样例配置与会话目录", "directory"});
    parser.addOption({"capture", "生成六页1440×900验收截图并退出", "directory"});
    parser.addOption({"export-samples", "将样例CSV和HTML写入目录并退出", "directory"});
    parser.addOption({"scenario",
                      "验收场景 "
                      "normal/disconnected/empty/connection-error/config-error/recording-error/"
                      "export-error/loading",
                      "name", "normal"});
    parser.addOption({"replay", "按原始时间间隔回放姿态JSON或角度JSONL，联调P1提示", "file"});
    parser.addOption({"replay-report", "回放结束后写入JSON结果和同名PNG截图并退出", "file"});
    parser.addOption({"page", "启动页1~6", "number", "1"});
    parser.process(app);
    if ((parser.isSet("replay-report") && !parser.isSet("replay")) ||
        (parser.isSet("replay") && (parser.isSet("capture") || parser.isSet("export-samples")))) {
        std::fprintf(stderr, "回放报告需要 --replay，且回放不能与样例截图/导出同时使用\n");
        return 2;
    }
    QTemporaryDir captureData;
    QString data = parser.value("data-dir");
    if ((parser.isSet("capture") || parser.isSet("export-samples")) && data.isEmpty())
        data = captureData.path();
    MainWindow w(data);
    w.setScenario(parser.value("scenario"));
    w.navigate(static_cast<ui::PageId>(qBound(1, parser.value("page").toInt(), 6) - 1));
    w.show();
    if (parser.isSet("replay")) {
        if (parser.isSet("replay-report")) {
            QObject::connect(w.replay(), &ui::PoseReplay::finished, &w, [&] {
                QTimer::singleShot(0, &w, [&] {
                    const auto &r = w.realtime()->result();
                    const auto token = w.realtime()->sessionId();
                    const QString output = parser.value("replay-report");
                    const QString screenshot = output + ".png";
                    QJsonObject result{{"source", "pose-file-replay"},
                                       {"realtimeService", "chenyifu-week03"},
                                       {"sessionId", token},
                                       {"completedReps", double(r.completedReps)},
                                       {"message", QString::fromStdString(r.message)},
                                       {"hintCode", int(r.code)},
                                       {"droppedFrames", double(w.realtime()->droppedFrames())},
                                       {"screenshot", screenshot},
                                       {"liveCameraVerified", false}};
                    QImage image(w.centralWidget()->size(), QImage::Format_ARGB32_Premultiplied);
                    image.fill(Qt::transparent);
                    w.centralWidget()->render(&image);
                    QFile report(output);
                    if (!image.save(screenshot) || !report.open(QFile::WriteOnly)) {
                        app.exit(2);
                        return;
                    }
                    const auto bytes = QJsonDocument(result).toJson();
                    if (report.write(bytes) != bytes.size()) {
                        app.exit(2);
                        return;
                    }
                    std::printf("P1 replay: %llu reps; %s\n",
                                static_cast<unsigned long long>(r.completedReps), qPrintable(output));
                    app.quit();
                });
            });
        }
        QString error;
        if (!w.startPoseReplay(parser.value("replay"), error)) {
            std::fprintf(stderr, "姿态回放失败：%s\n", qPrintable(error));
            return 2;
        }
    }
    if (parser.isSet("export-samples")) {
        QString output = parser.value("export-samples");
        if (!QDir().mkpath(output))
            return 2;
        bool csv = w.sessions()->exportSample(QDir(output).filePath("sample-angles.csv"), true);
        bool html = w.sessions()->exportSample(QDir(output).filePath("sample-report.html"), false);
        return csv && html ? 0 : 2;
    }
    if (parser.isSet("capture")) {
        QString output = parser.value("capture");
        QTimer::singleShot(80, &w, [&w, &app, output] {
            if (!QDir().mkpath(output)) {
                app.exit(2);
                return;
            }
            w.resize(1440, 900);
            w.centralWidget()->setFocus();
            for (int i = 0; i < 6; i++) {
                w.navigate(static_cast<ui::PageId>(i));
                QApplication::processEvents();
                QImage image(w.centralWidget()->size(), QImage::Format_ARGB32_Premultiplied);
                image.fill(Qt::transparent);
                w.centralWidget()->render(&image);
                QString path = QDir(output).filePath(QString("page-%1.png").arg(i + 1));
                if (!image.save(path)) {
                    app.exit(2);
                    return;
                }
                QJsonArray geometry;
                for (auto widget : w.centralWidget()->findChildren<QWidget *>()) {
                    if (!widget->isVisibleTo(w.centralWidget()) || widget->objectName().isEmpty())
                        continue;
                    auto point = widget->mapTo(w.centralWidget(), QPoint());
                    geometry.append(QJsonObject{{"class", widget->metaObject()->className()},
                                                {"name", widget->objectName()},
                                                {"x", point.x()},
                                                {"y", point.y()},
                                                {"w", widget->width()},
                                                {"h", widget->height()}});
                }
                QFile geo(QDir(output).filePath(QString("page-%1-layout.json").arg(i + 1)));
                if (geo.open(QFile::WriteOnly))
                    geo.write(QJsonDocument(geometry).toJson());
                std::printf("Captured %s\n", qPrintable(path));
            }
            app.quit();
        });
    }
    return app.exec();
}
