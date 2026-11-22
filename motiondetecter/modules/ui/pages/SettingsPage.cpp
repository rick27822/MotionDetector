#include "SettingsPage.h"
#include <QDir>
#include <QFileDialog>
#include <QSaveFile>
#include <QScrollArea>
#include <QTemporaryFile>
namespace ui {
SettingsPage::SettingsPage(AppState *s, SettingsController *c, QWidget *p)
    : QWidget(p), m_state(s), m_controller(c) {
    setObjectName("page");
    auto outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto content = new QWidget;
    auto root = new QVBoxLayout(content);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(16);
    auto columns = new QHBoxLayout;
    columns->setSpacing(20);
    auto left = new QVBoxLayout;
    left->setSpacing(20);
    auto right = new QVBoxLayout;
    right->setContentsMargins(0, 0, 0, 0);
    right->setSpacing(20);
    auto general = new CardWidget("通用");
    general->setFixedHeight(211);
    general->body->setSpacing(4);
    auto lang = button("简体中文");
    lang->setEnabled(false);
    lang->setToolTip("阶段一提供简体中文界面");
    auto theme = button("深色（专业）");
    theme->setEnabled(false);
    auto boot = new ToggleField("开机自动启动");
    boot->setEnabled(false);
    boot->setToolTip("系统启动服务未实现");
    auto update = new ToggleField("有新版本时提醒我");
    update->setEnabled(false);
    update->setToolTip("更新服务未实现");
    general->body->addWidget(row("界面语言", lang));
    general->body->addWidget(row("界面主题", theme));
    general->body->addWidget(row("开机自动启动", boot));
    general->body->addWidget(row("有新版本时提醒我", update));
    left->addWidget(general);
    auto camera = new CardWidget("摄像头与推流");
    camera->setFixedHeight(246);
    camera->body->setSpacing(4);
    m_resolution = new QComboBox;
    m_resolution->setFixedHeight(28);
    m_resolution->setObjectName("captureResolution");
    m_resolution->addItems({"1920 × 1080", "1280 × 720", "640 × 480"});
    m_fps = new QComboBox;
    m_fps->setFixedHeight(28);
    m_fps->setObjectName("captureFps");
    for (int v : {15, 25, 30, 60})
        m_fps->addItem(QString::number(v) + " fps", v);
    auto protocol = button("WebRTC（待接入）");
    protocol->setEnabled(false);
    auto degrade = new ToggleField("弱网时自动降低分辨率");
    degrade->setEnabled(false);
    m_mirror = new ToggleField("镜像参考画面");
    m_mirror->setObjectName("mirrorVideo");
    camera->body->addWidget(row("采集分辨率（样例参数）", m_resolution));
    camera->body->addWidget(row("采集帧率（样例参数）", m_fps));
    camera->body->addWidget(row("推流协议", protocol));
    camera->body->addWidget(row("弱网时自动降低分辨率", degrade));
    camera->body->addWidget(row("镜像参考画面", m_mirror));
    left->addWidget(camera);
    left->addStretch();
    columns->addLayout(left, 1);
    auto inference = new CardWidget("模型与推理");
    inference->setFixedHeight(147);
    inference->body->setSpacing(4);
    auto backend = label("UI 样例 · 无真实推理");
    backend->setStyleSheet("font-size:11px;color:#00E5FF;");
    m_threads = new QSpinBox;
    m_threads->setFixedHeight(28);
    m_threads->setObjectName("inferenceThreads");
    m_threads->setRange(1, 16);
    m_threads->setFixedWidth(72);
    auto gpu = new ToggleField("GPU 硬件加速");
    gpu->setEnabled(false);
    gpu->setToolTip("没有 GPU 后端能力，不能启用");
    inference->body->addWidget(row("推理设备", backend));
    inference->body->addWidget(row("推理线程数（样例参数）", m_threads));
    inference->body->addWidget(row("GPU 硬件加速", gpu));
    right->addWidget(inference);
    auto storage = new CardWidget("数据与存储");
    storage->setFixedHeight(173);
    m_path = button("选择目录", "storageDirectory", "link");
    m_path->setFixedWidth(170);
    storage->body->addWidget(row("样例文件保存路径", m_path));
    storage->body->addWidget(label("样例导出格式", true));
    auto formats = new QHBoxLayout;
    auto csv = button("CSV", {}, "primary");
    auto json = button("JSON");
    auto mp4 = button("MP4 视频");
    csv->setEnabled(false);
    json->setEnabled(false);
    mp4->setEnabled(false);
    mp4->setToolTip("MP4 是视频格式，阶段一不提供真实录像");
    formats->addWidget(csv);
    formats->addWidget(json);
    formats->addWidget(mp4);
    storage->body->addLayout(formats);
    right->addWidget(storage);
    auto about = new CardWidget("关于");
    about->setFixedHeight(130);
    about->body->addWidget(row("版本", label("0.1.0 · UI Stage 1", true)));
    auto check = button("检查更新（尚未接入）");
    check->setEnabled(false);
    about->body->addWidget(check);
    right->addWidget(about);
    right->addStretch();
    auto rightWidget = new QWidget;
    rightWidget->setLayout(right);
    rightWidget->setFixedWidth(340);
    columns->addWidget(rightWidget);
    root->addLayout(columns, 1);
    m_status = label("设置自动保存到本机；设备与推理参数尚未下发真实服务。", true);
    m_status->setWordWrap(true);
    root->addWidget(m_status);
    scroll->setWidget(content);
    outer->addWidget(scroll);
    connect(m_resolution, &QComboBox::currentIndexChanged, this, [this] { apply(); });
    connect(m_fps, &QComboBox::currentIndexChanged, this, [this] { apply(); });
    connect(m_threads, &QSpinBox::valueChanged, this, [this] { apply(); });
    connect(m_mirror, &QAbstractButton::toggled, this, [this] { apply(); });
    connect(m_path, &QPushButton::clicked, this, [this] {
        auto path = QFileDialog::getExistingDirectory(this, "选择样例文件保存目录",
                                                      m_state->config.storagePath);
        if (path.isEmpty())
            return;
        QTemporaryFile probe(QDir(path).filePath(".poselab-write-XXXXXX"));
        if (!probe.open()) {
            m_status->setText("目录不可写，请选择其他路径。");
            return;
        }
        auto config = m_state->config;
        config.storagePath = path;
        m_controller->apply(config);
    });
    connect(c, &SettingsController::feedback, this, [this](const QString &m, bool ok) {
        m_status->setText(m);
        m_status->setStyleSheet(ok ? "color:#A1A1AA;" : "color:#FF6B80;");
        if (!ok)
            load();
    });
    connect(s, &AppState::configChanged, this, &SettingsPage::load);
    connect(s, &AppState::detectionChanged, this, [this] {
        bool enabled = !m_state->recording;
        m_resolution->setEnabled(enabled);
        m_fps->setEnabled(enabled);
        m_threads->setEnabled(enabled);
        m_mirror->setEnabled(enabled);
        m_path->setEnabled(enabled);
    });
    load();
}
void SettingsPage::load() {
    m_loading = true;
    m_resolution->setCurrentText(m_state->config.resolution);
    m_fps->setCurrentIndex(m_fps->findData(m_state->config.fps));
    m_threads->setValue(m_state->config.threads);
    m_mirror->setChecked(m_state->config.mirror);
    QString path = m_state->config.storagePath;
    m_path->setText(QFontMetrics(m_path->font()).elidedText(path, Qt::ElideMiddle, 145));
    m_path->setToolTip(path);
    m_loading = false;
}
void SettingsPage::apply() {
    if (m_loading)
        return;
    auto c = m_state->config;
    c.resolution = m_resolution->currentText();
    c.fps = m_fps->currentData().toInt();
    c.threads = m_threads->value();
    c.mirror = m_mirror->isChecked();
    m_controller->apply(c);
}
} // namespace ui
