#include "mainwindow.h"
#include "modules/ui/adapters/AnalysisM0Adapter.h"
#include "modules/ui/adapters/PoseSampleReader.h"
#include "modules/ui/pages/DetectionConfigPage.h"
#include "modules/ui/theme/Theme.h"
#include "modules/ui/widgets/ChartView.h"
#include "modules/ui/widgets/Controls.h"
#include "modules/ui/widgets/VideoView.h"
#include <QButtonGroup>
#include <QComboBox>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <thread>
#include <limits>
using namespace ui;
class UiTests : public QObject {
    Q_OBJECT
    static void snapshot(MainWindow &w, const QString &name) {
        QString dir = qEnvironmentVariable("POSELAB_EVIDENCE_DIR");
        if (dir.isEmpty())
            return;
        QDir().mkpath(dir);
        w.resize(1440, 900);
        w.show();
        QApplication::processEvents();
        QImage image(w.centralWidget()->size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        w.centralWidget()->render(&image);
        image.save(QDir(dir).filePath(name + ".png"));
    }
    static core::PoseResult livePose(qint64 t, float knee, int frame = -1) {
        core::PoseResult p;
        p.timestampMs = t;
        p.frameId = frame;
        p.detected = true;
        p.setAngle(core::JointAngle::LeftKnee, knee);
        p.setAngle(core::JointAngle::Trunk, 0);
        return p;
    }
    static void completeRep(RealtimeController *c, const QString &token, qint64 start = 0) {
        const float angles[] = {170, 140, 90, 140, 170};
        for (int i = 0; i < 5; ++i)
            c->onPoseReady(livePose(start + i * 200, angles[i]), token);
    }
  private slots:
    void realtimeStagesCountAndPageShareInput() {
        QTemporaryDir dir;
        MainWindow w(dir.path());
        const auto token = w.beginRealtimeInput("测试姿态输入");
        auto c = w.realtime();
        QSignalSpy reps(c, &RealtimeController::repCompleted);
        const float angles[] = {170, 140, 90, 140, 170};
        const motion::RealtimeHintCode codes[] = {motion::RealtimeHintCode::Ready,
            motion::RealtimeHintCode::Descending, motion::RealtimeHintCode::Bottom,
            motion::RealtimeHintCode::Rising, motion::RealtimeHintCode::RepCompleted};
        for (int i = 0; i < 5; ++i) {
            c->onPoseReady(livePose(5000 + i * 200, angles[i], i), token);
            QCOMPARE(c->result().code, codes[i]);
            QCOMPARE(c->pose().angle(core::JointAngle::LeftKnee), angles[i]);
            QCOMPARE(w.findChild<QLabel *>("realtimeHintText")->text(),
                     QString::fromStdString(c->result().message));
        }
        QCOMPARE(c->result().completedReps, std::uint64_t(1));
        QCOMPARE(w.findChild<QLabel *>("realtimeRepCount")->text(), QString("累计次数：1"));
        QCOMPARE(reps.size(), 1);
        c->poll(); c->poll();
        QCOMPARE(reps.size(), 1);
        QVERIFY(!w.findChild<QPushButton *>("toggleRecording")->isEnabled());
        const auto sessions = w.state()->sessions.size();
        w.detection()->toggleRecording();
        QCOMPARE(w.state()->sessions.size(), sessions);
        QVERIFY(!w.state()->recording);
        snapshot(w, "realtime-completed");
    }
    void realtimeTwelveReps() {
        QTemporaryDir dir;
        MainWindow w(dir.path());
        auto token = w.beginRealtimeInput("12次合成动作");
        QSignalSpy reps(w.realtime(), &RealtimeController::repCompleted);
        const float angles[] = {170, 140, 90, 140, 170};
        for (int rep = 0; rep < 12; ++rep)
            for (int i = rep ? 1 : 0; i < 5; ++i)
                w.realtime()->onPoseReady(livePose(rep * 800 + i * 200, angles[i]), token);
        QCOMPARE(w.realtime()->result().completedReps, std::uint64_t(12));
        QCOMPARE(reps.size(), 12);
    }
    void realtimeTimeoutClearsHudAndIncompleteCycle() {
        QTemporaryDir dir;
        MainWindow w(dir.path());
        auto token = w.beginRealtimeInput("超时测试");
        auto c = w.realtime();
        c->onPoseReady(livePose(900000, 170), token);
        c->onPoseReady(livePose(900200, 90), token);
        QTRY_COMPARE_WITH_TIMEOUT(c->result().code, motion::RealtimeHintCode::DataLost, 900);
        QVERIFY(!c->pose().detected);
        QCOMPARE(c->pose().validAngles, quint32(0));
        QVERIFY(w.findChild<QLabel *>("realtimeHintText")->text().contains("中断"));
        snapshot(w, "realtime-timeout");
        c->onPoseReady(livePose(900400, 170), token);
        QCOMPARE(c->result().code, motion::RealtimeHintCode::Ready);
        QCOMPARE(c->result().completedReps, std::uint64_t(0));
        completeRep(c, token, 901000);
        QCOMPARE(c->result().completedReps, std::uint64_t(1));
        QTRY_COMPARE_WITH_TIMEOUT(c->result().code, motion::RealtimeHintCode::DataLost, 900);
        QCOMPARE(c->result().completedReps, std::uint64_t(1));
    }
    void realtimeInitialSilenceIsVisible() {
        QTemporaryDir dir;
        MainWindow w(dir.path());
        w.beginRealtimeInput("无帧输入");
        QTRY_COMPARE_WITH_TIMEOUT(w.realtime()->result().code, motion::RealtimeHintCode::DataLost, 900);
        QVERIFY(w.findChild<QLabel *>("realtimeHintText")->text().contains("未收到"));
    }
    void realtimeStopDisconnectAndOldGeneration() {
        QTemporaryDir dir;
        MainWindow w(dir.path());
        auto token = w.beginRealtimeInput("会话测试");
        auto c = w.realtime();
        QSignalSpy ended(c, &RealtimeController::sessionEnded);
        completeRep(c, token);
        c->submitPose(livePose(1000, 90), token); // Pending callback from old generation.
        w.detection()->stop();
        QVERIFY(!c->active());
        QCOMPARE(c->result().completedReps, std::uint64_t(0));
        QCOMPARE(ended.size(), 1);
        QCOMPARE(ended[0][1].toULongLong(), qulonglong(1));
        snapshot(w, "realtime-stopped");
        w.detection()->toggleDetection();
        auto restarted = c->sessionId();
        QVERIFY(!restarted.isEmpty() && restarted != token);
        c->onPoseReady(livePose(1100, 170), token);
        QApplication::processEvents();
        QVERIFY(!c->pose().detected);
        completeRep(c, restarted);
        QCOMPARE(c->result().completedReps, std::uint64_t(1));
        w.endRealtimeInput(restarted);
        QVERIFY(!c->active());
        QVERIFY(c->sessionId().isEmpty());
        QCOMPARE(c->result().completedReps, std::uint64_t(0));
        QVERIFY(w.findChild<QLabel *>("realtimeHintText")->text().contains("断开"));
        snapshot(w, "realtime-disconnected");
        auto reconnected = w.beginRealtimeInput("重新绑定输入");
        w.endRealtimeInput(restarted);
        QVERIFY(c->active());
        QVERIFY(reconnected != restarted);
        c->onPoseReady(livePose(9000, 170), restarted);
        QVERIFY(!c->pose().detected);
        completeRep(c, reconnected);
        QCOMPARE(c->result().completedReps, std::uint64_t(1));
    }
    void realtimeInvalidPoseAndDuplicateDoNotCount() {
        QTemporaryDir dir;
        MainWindow w(dir.path());
        auto token = w.beginRealtimeInput("有效性测试");
        auto c = w.realtime();
        c->onPoseReady(livePose(0, 170, 0), token);
        auto absent = livePose(200, 90, 1);
        absent.detected = false;
        c->onPoseReady(absent, token);
        QCOMPARE(c->result().code, motion::RealtimeHintCode::DataLost);
        QCOMPARE(c->pose().validAngles, quint32(0));
        c->onPoseReady(livePose(800, 170, 2), token);
        QCOMPARE(c->result().completedReps, std::uint64_t(0));
        c->onPoseReady(livePose(1000, 90, 3), token);
        c->onPoseReady(livePose(1000, 90, 3), token);
        QCOMPARE(c->result().code, motion::RealtimeHintCode::InvalidInput);
        QVERIFY(!c->pose().detected);
        c->onPoseReady(livePose(1400, 170, 4), token);
        QCOMPARE(c->result().completedReps, std::uint64_t(0));
        c->onPoseReady(livePose(1600, 0, 5), token);
        QCOMPARE(c->result().code, motion::RealtimeHintCode::Bottom); // Zero is valid.
        c->onPoseReady(livePose(1800, std::numeric_limits<float>::quiet_NaN(), 6), token);
        QCOMPARE(c->result().code, motion::RealtimeHintCode::DataLost);
        QVERIFY(!c->pose().hasAngle(core::JointAngle::LeftKnee));
    }
    void realtimeWorkerMailboxIsBoundedAndUpdatesGuiThread() {
        QTemporaryDir dir;
        MainWindow w(dir.path());
        auto token = w.beginRealtimeInput("工作线程输入");
        auto c = w.realtime();
        bool guiOnly = true;
        connect(c, &RealtimeController::changed, c, [&] {
            guiOnly = guiOnly && QThread::currentThread() == qApp->thread();
        }, Qt::DirectConnection);
        std::thread worker([=] {
            for (int i = 0; i < 10000; ++i)
                c->onPoseReady(livePose(i, 170, i), token);
        });
        worker.join();
        QCOMPARE(c->droppedFrames(), quint64(9999));
        QVERIFY(!c->pose().detected);
        QTRY_COMPARE(c->pose().timestampMs, qint64(9999));
        QVERIFY(guiOnly);
        QCOMPARE(c->result().completedReps, std::uint64_t(0));
    }
    void realtimeQueuedStaleCompletionCannotCount() {
        QTemporaryDir dir;
        MainWindow w(dir.path());
        const auto token = w.beginRealtimeInput("过期帧测试");
        auto c = w.realtime();
        c->onPoseReady(livePose(0, 170), token);
        c->onPoseReady(livePose(200, 90), token);
        c->submitPose(livePose(800, 170), token);
        QThread::msleep(550); // Deliberately block delivery of the queued completion.
        QApplication::processEvents();
        QCOMPARE(c->result().code, motion::RealtimeHintCode::DataLost);
        QCOMPARE(c->result().completedReps, std::uint64_t(0));
        QVERIFY(!c->pose().detected);
    }
    void realtimeReplayRunsThroughPageAndRestarts() {
        QTemporaryDir dir;
        MainWindow w(dir.path());
        QString error;
        QVERIFY(w.startPoseReplay(":/fixtures/pose.json", error));
        auto token = w.realtime()->sessionId();
        QSignalSpy finished(w.replay(), &PoseReplay::finished);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 4000);
        QVERIFY(w.realtime()->pose().detected);
        QVERIFY(w.findChild<QLabel *>("realtimeSourceNote")->text().contains("回放"));
        w.detection()->stop();
        w.detection()->toggleDetection();
        QVERIFY(w.realtime()->sessionId() != token);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 2, 4000);
        // Rebinding an external producer must not restart the old file source.
        w.beginRealtimeInput("外部输入");
        QVERIFY(!w.replay()->loaded());
        QVERIFY(!w.realtime()->pose().detected);
        QVERIFY(!w.startPoseReplay(dir.filePath("absent.json"), error));
        QVERIFY(!error.isEmpty());
    }
    void sixPagesAndNavigation() {
        QTemporaryDir dir;
        MainWindow w(dir.path());
        w.show();
        QCOMPARE(w.pages()->count(), 6);
        auto group = w.findChild<QButtonGroup *>();
        QVERIFY(group);
        for (int i = 0; i < 6; i++) {
            QTest::mouseClick(group->button(i), Qt::LeftButton);
            QCOMPARE(int(w.currentPage()), i);
            QVERIFY(group->button(i)->isChecked());
        }
    }
    void legacyAndCurrentSamples() {
        QVector<core::PoseResult> current, legacy;
        QString error;
        QVERIFY2(PoseSampleReader::readPoses(":/fixtures/pose.json", current, error),
                 qPrintable(error));
        QVERIFY2(PoseSampleReader::readPoses(":/fixtures/legacy.json", legacy, error),
                 qPrintable(error));
        QCOMPARE(current.size(), 3);
        QCOMPARE(legacy.size(), 3);
        for (int i = 0; i < 3; i++) {
            QCOMPARE(current[i].timestampMs, legacy[i].timestampMs);
            QCOMPARE(current[i].validAngles, legacy[i].validAngles);
            for (int j = 0; j < 12; j++)
                QVERIFY(qAbs(current[i].angles[j] - legacy[i].angles[j]) < .001f);
        }
        QVERIFY(current[0].hasAngle(core::JointAngle::Trunk));
        QCOMPARE(current[0].angle(core::JointAngle::Trunk), 0.f);
    }
    void angleValidityAndConflicts() {
        std::array<float, 12> angles{};
        QString error;
        QJsonObject zero{{"trunk", 0}};
        QVERIFY(PoseSampleReader::parseAngles(zero, core::angleBit(core::JointAngle::Trunk), angles,
                                              error));
        QJsonObject missing{{"trunk", QJsonValue::Null}};
        QVERIFY(!PoseSampleReader::parseAngles(missing, core::angleBit(core::JointAngle::Trunk),
                                               angles, error));
        QVERIFY(PoseSampleReader::parseAngles(missing, 0, angles, error));
        QJsonObject conflict{{"leftKnee", 90}, {"左膝", 100}};
        QVERIFY(!PoseSampleReader::parseAngles(conflict, 1, angles, error));
        QVERIFY(!PoseSampleReader::parseAngles({}, 1u << 20, angles, error));
    }
    void malformedSeriesPreservesOutput() {
        QTemporaryDir dir;
        QString path = dir.filePath("invalid.jsonl");
        QFile f(path);
        QVERIFY(f.open(QFile::WriteOnly));
        f.write("{\"t\":0,\"validMask\":0,\"deg\":{}}\n{\"t\":0,\"validMask\":0,\"deg\":{}}\n");
        f.close();
        QVector<core::AngleSample> out(1);
        out[0].t = 99;
        QString error;
        QVERIFY(!PoseSampleReader::readSeries(path, out, error));
        QCOMPARE(out[0].t, 99);
        QVERIFY(!error.isEmpty());
    }
    void m0AdaptationIsExplicitlySimulated() {
        QVector<core::AngleSample> samples;
        QString error;
        QVERIFY(PoseSampleReader::readSeries(":/fixtures/series.jsonl", samples, error));
        QCOMPARE(samples.size(), 382);
        QCOMPARE(samples.last().t, 12573);
        AnalysisM0Adapter a;
        auto result = a.analyze(samples);
        QVERIFY(result.valid);
        QVERIFY(result.simulated);
        QCOMPARE(result.scorePercent, 80.);
        QCOMPARE(result.symmetryPercent, 95.);
        QVERIFY(!a.analyze({}).valid);
    }
    void draftDoesNotApplyUntilSaveAndPersists() {
        QTemporaryDir dir;
        {
            MainWindow w(dir.path());
            w.show();
            w.navigate(PageId::DetectionConfig);
            auto slider = w.findChild<QSlider *>("confidenceSlider");
            QVERIFY(slider);
            slider->setValue(73);
            QVERIFY(qAbs(w.state()->config.confidence - .5f) < .001f);
            QTest::mouseClick(w.findChild<QPushButton *>("savePoseConfig"), Qt::LeftButton);
            QVERIFY(qAbs(w.state()->config.confidence - .73f) < .001f);
            QVERIFY(QFile::exists(w.settings()->configFile()));
            w.navigate(PageId::Settings);
            auto fps = w.findChild<QComboBox *>("captureFps");
            fps->setCurrentIndex(fps->findData(60));
            QCOMPARE(w.state()->config.fps, 60);
            QVERIFY(qAbs(w.state()->config.confidence - .73f) < .001f);
        }
        MainWindow restarted(dir.path());
        QCOMPARE(restarted.state()->config.fps, 60);
        QVERIFY(qAbs(restarted.state()->config.confidence - .73f) < .001f);
    }
    void failedSavePreservesEffectiveAndDiskConfig() {
        QTemporaryDir dir;
        MainWindow w(dir.path());
        auto cfg = w.state()->config;
        cfg.confidence = .61f;
        QVERIFY(w.settings()->apply(cfg));
        w.settings()->setFailSave(true);
        cfg.confidence = .82f;
        QVERIFY(!w.settings()->apply(cfg));
        w.navigate(PageId::DetectionConfig);
        snapshot(w, "config-error");
        QVERIFY(qAbs(w.state()->config.confidence - .61f) < .001f);
        MainWindow restarted(dir.path());
        QVERIFY(qAbs(restarted.state()->config.confidence - .61f) < .001f);
    }
    void corruptConfigDoesNotGetOverwrittenOnStartup() {
        QTemporaryDir dir;
        QFile file(dir.filePath("ui-config.json"));
        QVERIFY(file.open(QFile::WriteOnly));
        file.write("{broken");
        file.close();
        MainWindow w(dir.path());
        QVERIFY(!w.settings()->error().isEmpty());
        QVERIFY(file.open(QFile::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("{broken"));
    }
    void deviceAsyncFailureRetryAndSharedState() {
        QTemporaryDir dir;
        MainWindow w(dir.path());
        w.setScenario("connection-error");
        QVERIFY(!w.state()->connected);
        w.devices()->connectDevice("xiaomi");
        QVERIFY(w.devices()->busy());
        w.navigate(PageId::Devices);
        snapshot(w, "connection-loading");
        QTRY_VERIFY_WITH_TIMEOUT(!w.devices()->busy(), 1500);
        QVERIFY(!w.state()->connected);
        QVERIFY(w.lastMessage().contains("失败"));
        snapshot(w, "connection-error");
        w.devices()->setFailConnection(false);
        w.devices()->connectDevice("xiaomi");
        QTRY_VERIFY_WITH_TIMEOUT(w.state()->connected, 1500);
        QCOMPARE(w.state()->deviceName, QString("Xiaomi 14 Ultra"));
        w.detection()->toggleDetection();
        QVERIFY(w.state()->detecting);
    }
    void disconnectCancelsPendingConnection() {
        QTemporaryDir dir;
        MainWindow w(dir.path());
        w.devices()->disconnectDevice();
        w.devices()->connectDevice("xiaomi");
        w.devices()->disconnectDevice();
        QTest::qWait(550);
        QVERIFY(!w.state()->connected);
        QVERIFY(!w.devices()->busy());
    }
    void recordingFailureAndDisconnectFinalization() {
        QTemporaryDir dir;
        MainWindow w(dir.path());
        int initial = w.state()->sessions.size();
        w.setScenario("recording-error");
        w.detection()->toggleRecording();
        QVERIFY(!w.state()->recording);
        snapshot(w, "recording-error");
        QCOMPARE(w.state()->sessions.size(), initial);
        w.detection()->setFailRecording(false);
        w.detection()->toggleRecording();
        QVERIFY(w.state()->recording);
        QTest::qWait(50);
        w.devices()->disconnectDevice();
        QVERIFY(!w.state()->recording);
        QVERIFY(!w.state()->detecting);
        QCOMPARE(w.state()->sessions.size(), initial + 1);
        QVERIFY(QFile::exists(dir.filePath("demo-sessions.json")));
        MainWindow restarted(dir.path());
        QCOMPARE(restarted.state()->sessions.size(), initial + 1);
    }
    void playbackSeekPauseRateAndSessionReset() {
        QTemporaryDir dir;
        MainWindow w(dir.path());
        w.navigate(PageId::Playback);
        auto c = w.sessions();
        c->seek(5000);
        auto chart = w.findChild<ChartView *>("playbackChart");
        QVERIFY(chart);
        QCOMPARE(chart->position(), 5000);
        c->setRate(2);
        c->togglePlayback();
        QTest::qWait(120);
        QVERIFY(c->position() > 5100);
        c->togglePlayback();
        qint64 paused = c->position();
        QTest::qWait(80);
        QCOMPARE(c->position(), paused);
        c->seek(999999);
        QCOMPARE(c->position(), c->current()->durationMs);
        c->select("sample-041");
        QCOMPARE(c->position(), 0);
        QVERIFY(!c->playing());
        QCOMPARE(chart->position(), 0);
    }
    void actualSampleExportsAndFailure() {
        QTemporaryDir dir;
        MainWindow w(dir.path());
        auto c = w.sessions();
        QString csv = dir.filePath("sample.csv"), html = dir.filePath("sample.html");
        QVERIFY(c->exportSample(csv, true));
        QVERIFY(c->exportSample(html, false));
        QFile f(csv);
        QVERIFY(f.open(QFile::ReadOnly));
        auto bytes = f.readAll();
        QVERIFY(bytes.contains("time_ms,left_knee_deg"));
        QVERIFY(bytes.contains("synthetic"));
        QCOMPARE(bytes.count('\n'), 383);
        QFile h(html);
        QVERIFY(h.open(QFile::ReadOnly));
        QVERIFY(QString::fromUtf8(h.readAll()).contains("M0 固定 Mock"));
        c->setFailExport(true);
        QString absent = dir.filePath("absent.csv");
        QVERIFY(!c->exportSample(absent, true));
        w.navigate(PageId::Analysis);
        snapshot(w, "export-error");
        QVERIFY(!QFile::exists(absent));
        c->setFailExport(false);
        QVERIFY(!c->exportSample(dir.filePath("missing/path.csv"), true));
    }
    void emptyStateCanRecoverAndCreateSample() {
        QTemporaryDir dir;
        MainWindow w(dir.path());
        w.setScenario("empty");
        QVERIFY(w.state()->sessions.isEmpty());
        w.navigate(PageId::Playback);
        snapshot(w, "empty-playback");
        w.navigate(PageId::Analysis);
        snapshot(w, "empty-analysis");
        QVERIFY(!w.findChild<QPushButton *>("exportCsv")->isEnabled());
        for (int i = 0; i < 6; i++)
            w.navigate(static_cast<PageId>(i));
        w.sessions()->addRecorded(3000);
        QCOMPARE(w.state()->sessions.size(), 1);
        w.sessions()->select(w.state()->sessions.first().id);
        QVERIFY(w.sessions()->current());
    }
    void handoffSampleAndKeypointSelectionReachRealtime() {
        QTemporaryDir dir;
        MainWindow w(dir.path());
        auto source = w.findChild<QComboBox *>("poseSampleSource");
        QVERIFY(source);
        QCOMPARE(source->count(), 4);
        source->setCurrentIndex(1);
        bool zeroFound = false;
        for (auto l : w.pages()->widget(0)->findChildren<QLabel *>())
            if (l->text() == "0.0°")
                zeroFound = true;
        QVERIFY(zeroFound);
        auto config = w.state()->config;
        config.enabledPoints.removeAll("左膝");
        QVERIFY(w.settings()->apply(config));
        bool missingFound = false;
        for (auto l : w.pages()->widget(0)->findChildren<QLabel *>())
            if (l->text() == "—")
                missingFound = true;
        QVERIFY(missingFound);
    }
    void keyboardAndMinimumWindow() {
        QTemporaryDir dir;
        MainWindow w(dir.path());
        w.show();
        auto toggle = w.findChild<ToggleField *>("showSkeleton");
        QVERIFY(toggle);
        bool before = toggle->isChecked();
        toggle->setFocus();
        QTest::keyClick(toggle, Qt::Key_Space);
        QCOMPARE(toggle->isChecked(), !before);
        QCOMPARE(w.state()->showSkeleton, !before);
        w.navigate(PageId::DetectionConfig);
        auto slider = w.findChild<QSlider *>("confidenceSlider");
        int value = slider->value();
        slider->setFocus();
        QTest::keyClick(slider, Qt::Key_Right);
        QCOMPARE(slider->value(), value + 1);
        w.resize(1100, 740);
        for (int i = 0; i < 6; i++) {
            w.navigate(static_cast<PageId>(i));
            QApplication::processEvents();
            QVERIFY(w.pages()->currentWidget()->width() <= 875);
        }
    }
    void videoMirrorUsesSameImageTransform() {
        VideoView video;
        video.resize(915, 795);
        auto p = video.mapPoint({.2, .5});
        video.setMirror(true);
        auto mirrored = video.mapPoint({.2, .5});
        QVERIFY(qAbs(p.x() + mirrored.x() - 915) < .001);
        QCOMPARE(p.y(), mirrored.y());
        video.setCover(false);
        auto r = video.imageRect();
        QVERIFY(r.left() >= -.001 && r.top() >= -.001 && r.right() <= 915.001 &&
                r.bottom() <= 795.001);
    }
};
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    ui::applyTheme(app);
    UiTests test;
    return QTest::qExec(&test, argc, argv);
}
#include "ui_tests.moc"
