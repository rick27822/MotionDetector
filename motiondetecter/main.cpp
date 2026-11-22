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
    parser.setApplicationDescription("PoseLab 阶段一 · 六页 UI 与样例交互验收（无真实采集/推理）");
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
    parser.addOption({"page", "启动页1~6", "number", "1"});
    parser.process(app);
    QTemporaryDir captureData;
    QString data = parser.value("data-dir");
    if ((parser.isSet("capture") || parser.isSet("export-samples")) && data.isEmpty())
        data = captureData.path();
    MainWindow w(data);
    w.setScenario(parser.value("scenario"));
    w.navigate(static_cast<ui::PageId>(qBound(1, parser.value("page").toInt(), 6) - 1));
    w.show();
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
