#include "mainwindow.h"
#include "modules/ui/pages/AnalysisPage.h"
#include "modules/ui/pages/DetectionConfigPage.h"
#include "modules/ui/pages/DevicePage.h"
#include "modules/ui/pages/PlaybackPage.h"
#include "modules/ui/pages/RealtimePage.h"
#include "modules/ui/pages/SettingsPage.h"
#include "modules/ui/shell/Sidebar.h"
#include "modules/ui/widgets/Controls.h"
#include <QCloseEvent>
#include <QMenu>
#include <QFileDialog>
#include <QStandardPaths>
#include <QTimer>
using namespace ui;
MainWindow::MainWindow(const QString &directory, QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("PoseLab · 动作捕捉检测系统 · UI 样例");
    resize(1440, 900);
    setMinimumSize(1100, 740);
    QString path = directory.isEmpty()
                       ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                       : directory;
    m_state = new AppState(this);
    m_settings = new SettingsController(m_state, path, this);
    m_devices = new DeviceController(m_state, this);
    m_detection = new DetectionController(m_state, this);
    m_realtime = new RealtimeController(m_state, this);
    m_replay = new PoseReplay(m_realtime, this);
    m_sessions = new SessionController(m_state, path, this);
    auto central = new QWidget;
    central->setFocusPolicy(Qt::StrongFocus);
    setCentralWidget(central);
    auto root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    m_sidebar = new Sidebar(m_state);
    root->addWidget(m_sidebar);
    auto right = new QVBoxLayout;
    right->setContentsMargins(0, 0, 0, 0);
    right->setSpacing(0);
    auto top = new QWidget;
    top->setObjectName("topbar");
    top->setFixedHeight(48);
    auto header = new QHBoxLayout(top);
    header->setContentsMargins(12, 8, 12, 8);
    header->setSpacing(8);
    m_title = title("");
    header->addWidget(m_title);
    header->addStretch();
    m_recordStatus = label("");
    m_recordStatus->setStyleSheet(
        "color:#FF4767;font-family:'SF "
        "Mono',monospace;font-size:11px;background:#1C1C1E;border-radius:6px;padding:6px;");
    header->addWidget(m_recordStatus);
    m_recordStatus->hide();
    m_backendStatus = label("参考图 · 未接入推理");
    m_backendStatus->setStyleSheet(
        "color:#00D776;font-size:11px;background:#151718;border-radius:6px;padding:6px;");
    header->addWidget(m_backendStatus);
    auto replayButton = button("姿态回放 ▾", "poseReplayMenu");
    auto replayMenu = new QMenu(replayButton);
    connect(replayMenu->addAction("打开姿态文件…"), &QAction::triggered, this, [this] {
        const auto path = QFileDialog::getOpenFileName(this, "选择姿态或角度文件", {},
                                                      "姿态数据 (*.json *.jsonl)");
        if (path.isEmpty())
            return;
        QString error;
        if (!startPoseReplay(path, error))
            showFeedback("姿态回放失败：" + error, false);
    });
    connect(replayMenu->addAction("12次动作合成样例"), &QAction::triggered, this, [this] {
        QString error;
        if (!startPoseReplay(":/fixtures/realtime-12reps.jsonl", error))
            showFeedback("姿态回放失败：" + error, false);
    });
    replayButton->setMenu(replayMenu);
    header->addWidget(replayButton);
    auto mode = button("UI 样例 ▾", "scenarioMenu");
    mode->setObjectName("scenarioMenu");
    mode->setFixedHeight(28);
    mode->setStyleSheet("font-size:11px;color:#FFAB40;background:#241A06;padding:3px "
                        "9px;border:0;border-radius:6px;");
    mode->setToolTip("当前只验证界面与样例数据，不连接真实手机或模型");
    auto menu = new QMenu(mode);
    QList<QPair<QString, QString>> scenarios = {{"正常样例", "normal"},
                                                {"无设备 / 断流", "disconnected"},
                                                {"无会话", "empty"},
                                                {"设备连接失败", "connection-error"},
                                                {"配置保存失败", "config-error"},
                                                {"样例录制失败", "recording-error"},
                                                {"样例导出失败", "export-error"}};
    for (const auto &s : scenarios) {
        auto action = menu->addAction(s.first);
        connect(action, &QAction::triggered, this, [this, id = s.second] {
            setScenario(id);
            showFeedback("已切换验收场景：" + id, true);
        });
    }
    mode->setMenu(menu);
    header->addWidget(mode);
    right->addWidget(top);
    m_pages = new QStackedWidget;
    m_pages->setObjectName("pages");
    m_pages->addWidget(new RealtimePage(m_state, m_detection, m_realtime));
    m_pages->addWidget(new DevicePage(m_state, m_devices));
    m_pages->addWidget(new DetectionConfigPage(m_state, m_settings));
    m_pages->addWidget(new PlaybackPage(m_state, m_sessions));
    m_pages->addWidget(new AnalysisPage(m_state, m_sessions));
    m_pages->addWidget(new SettingsPage(m_state, m_settings));
    right->addWidget(m_pages, 1);
    root->addLayout(right, 1);
    m_toast = new QLabel(central);
    m_toast->setWordWrap(true);
    m_toast->setMargin(12);
    m_toast->setObjectName("feedbackToast");
    m_toast->hide();
    m_toastTimer = new QTimer(this);
    m_toastTimer->setSingleShot(true);
    connect(m_toastTimer, &QTimer::timeout, m_toast, &QWidget::hide);
    connect(m_sidebar, &Sidebar::pageRequested, this, &MainWindow::navigate);
    connect(m_settings, &SettingsController::feedback, this, &MainWindow::showFeedback);
    connect(m_devices, &DeviceController::feedback, this, &MainWindow::showFeedback);
    connect(m_detection, &DetectionController::feedback, this, &MainWindow::showFeedback);
    connect(m_sessions, &SessionController::feedback, this, &MainWindow::showFeedback);
    connect(m_detection, &DetectionController::sampleRecorded, m_sessions,
            &SessionController::addRecorded);
    connect(m_devices, &DeviceController::busyChanged, this, [this](bool busy) {
        m_backendStatus->setText(busy ? "正在处理样例请求…" : "参考图 · 未接入推理");
    });
    connect(m_state, &AppState::deviceChanged, this, [this] {
        if (!m_state->connected)
            m_detection->stop();
    });
    connect(m_state, &AppState::detectionChanged, this, [this] {
        m_recordStatus->setVisible(m_state->recording);
        if (m_state->recording)
            m_recordStatus->setText("● 样例 REC 00:00");
    });
    connect(m_detection, &DetectionController::elapsedChanged, this, [this](qint64 ms) {
        m_recordStatus->setText("● 样例 REC " + core::formatDuration(ms));
    });
    connect(m_state, &AppState::sessionChanged, this, [this] {
        if (currentPage() == PageId::Analysis)
            navigate(PageId::Analysis);
    });
    connect(m_realtime, &RealtimeController::changed, this, [this] {
        if (m_realtime->enabled())
            m_backendStatus->setText(m_realtime->sourceName() + " · 实时提示已接入");
    });
    navigate(PageId::Realtime);
    central->setFocus();
    if (!m_settings->error().isEmpty())
        QTimer::singleShot(0, this,
                           [this] { showFeedback("配置读取失败：" + m_settings->error(), false); });
    if (!m_sessions->loadError().isEmpty())
        QTimer::singleShot(
            0, this, [this] { showFeedback("样例读取失败：" + m_sessions->loadError(), false); });
}
QString MainWindow::beginRealtimeInput(const QString &sourceName) {
    m_replay->clear();
    return activateRealtimeInput(sourceName);
}
QString MainWindow::activateRealtimeInput(const QString &sourceName) {
    m_detection->stop();
    m_devices->disconnectDevice(); // Cancel any outstanding sample-device connection.
    m_detection->setRealtimeMode();
    auto mode = findChild<QPushButton *>("scenarioMenu");
    mode->setEnabled(false);
    mode->setText("姿态输入");
    setWindowTitle("PoseLab · P1 实时提示联调");
    m_realtime->beginInput(sourceName);
    return m_realtime->sessionId();
}
void MainWindow::endRealtimeInput(const QString &sessionToken) {
    if (sessionToken.isEmpty() || sessionToken != m_realtime->sessionId())
        return;
    m_devices->disconnectDevice();
}
bool MainWindow::startPoseReplay(const QString &path, QString &error) {
    if (!m_replay->load(path, error))
        return false;
    activateRealtimeInput(path.startsWith(":/fixtures/") ? "合成姿态回放" : "姿态文件回放");
    return true;
}
void MainWindow::navigate(PageId page) {
    int index = int(page);
    if (index < 0 || index >= 6)
        return;
    m_pages->setCurrentIndex(index);
    m_sidebar->setCurrent(page);
    const QStringList names = {"实时检测 · 深蹲动作分析",
                               "设备连接 · 手机摄像头接入",
                               "检测配置 · 模型与参数",
                               "录像回放 · 会话管理",
                               "数据分析",
                               "系统设置"};
    QString name = names[index];
    if (page == PageId::Analysis && m_sessions->current())
        name += " · " + m_sessions->current()->name;
    m_title->setText(name);
    m_backendStatus->setVisible(page == PageId::Realtime);
    m_toast->raise();
}
PageId MainWindow::currentPage() const {
    return static_cast<PageId>(m_pages->currentIndex());
}
void MainWindow::setScenario(const QString &scenario) {
    if (m_realtime->enabled())
        return;
    m_settings->setFailSave(scenario == "config-error");
    m_devices->setFailConnection(scenario == "connection-error");
    m_detection->setFailRecording(scenario == "recording-error");
    m_sessions->setFailExport(scenario == "export-error");
    if (scenario == "empty")
        m_sessions->setEmpty();
    else if (scenario == "disconnected" || scenario == "connection-error") {
        m_detection->stop();
        m_devices->disconnectDevice();
    } else if (scenario == "loading")
        m_devices->refresh();
    else if (scenario == "normal") {
        m_sessions->restoreSamples();
        m_state->connected = true;
        m_state->detecting = true;
        emit m_state->deviceChanged();
        emit m_state->detectionChanged();
    }
}
void MainWindow::showFeedback(const QString &message, bool success) {
    m_lastMessage = message;
    m_toast->setText(message);
    m_toast->setStyleSheet(
        QString("background:%1;color:%2;border:1px solid %3;border-radius:9px;font-size:12px;")
            .arg(success ? "#18241B" : "#32191E", success ? "#D4EED8" : "#FFB4C0",
                 success ? "#315A3B" : "#803047"));
    m_toast->setFixedWidth(qMin(660, width() - 260));
    m_toast->adjustSize();
    m_toast->move(width() - m_toast->width() - 20, height() - m_toast->height() - 20);
    m_toast->show();
    m_toast->raise();
    m_toastTimer->start(6000);
}
void MainWindow::resizeEvent(QResizeEvent *e) {
    QMainWindow::resizeEvent(e);
    if (m_toast && m_toast->isVisible())
        m_toast->move(width() - m_toast->width() - 20, height() - m_toast->height() - 20);
}
void MainWindow::closeEvent(QCloseEvent *event) {
    m_detection->stop();
    QMainWindow::closeEvent(event);
}
