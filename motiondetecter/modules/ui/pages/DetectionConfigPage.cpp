#include "DetectionConfigPage.h"
#include <QButtonGroup>
#include <QScrollArea>
#include <QSignalBlocker>
namespace ui {
DetectionConfigPage::DetectionConfigPage(AppState *s, SettingsController *c, QWidget *p)
    : QWidget(p), m_state(s) {
    setObjectName("page");
    auto outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto content = new QWidget;
    auto root = new QVBoxLayout(content);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(20);
    auto pageHeading = title("姿态估计模型");
    pageHeading->setFixedHeight(17);
    root->addWidget(pageHeading);
    auto modelRow = new QWidget;
    modelRow->setFixedHeight(65);
    auto models = new QHBoxLayout(modelRow);
    models->setContentsMargins(0, 0, 0, 0);
    models->setSpacing(12);
    auto group = new QButtonGroup(this);
    QStringList names = {"样例姿态源（当前）", "MoveNet Lightning", "HRNet-W48"};
    QStringList descriptions = {"33 槽位 · 合成样例 · 不执行模型推理",
                                "17 关键点 · 真实后端尚未接入", "高精度 · 真实后端尚未接入"};
    for (int i = 0; i < 3; i++) {
        auto b = button("", "model");
        b->setCheckable(true);
        b->setFixedHeight(65);
        group->addButton(b, i);
        auto v = new QVBoxLayout(b);
        v->setContentsMargins(14, 10, 14, 10);
        v->setSpacing(4);
        auto h = label((i == 0 ? "◉  " : "○  ") + names[i]);
        h->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto d = label(descriptions[i], true);
        d->setStyleSheet("color:#A1A1AA;font-size:11px;");
        d->setAttribute(Qt::WA_TransparentForMouseEvents);
        v->addWidget(h);
        v->addWidget(d);
        if (i == 0)
            b->setChecked(true);
        else
            b->setEnabled(false);
        b->setToolTip(i == 0
                          ? "BlazePose/MoveNet/HRNet 尚未作为真实后端接入；此卡片仅选择 UI 样例源"
                          : "真实模型尚未接入");
        models->addWidget(b, 1);
    }
    root->addWidget(modelRow);
    auto mid = new QHBoxLayout;
    mid->setSpacing(20);
    auto params = new CardWidget("检测参数", content);
    params->setFixedHeight(229);
    params->body->setSpacing(12);
    m_conf = new SliderField("置信度阈值（样例参数）", "confidenceSlider", 50);
    auto nms = new SliderField("NMS 阈值 · 后端未支持", "nmsSlider", 30);
    nms->setEnabled(false);
    nms->setToolTip("现有后端未提供 NMS 参数生效确认");
    m_smoothing = new SliderField("关键点 EMA 系数（越小越平滑）", "smoothingSlider", 60);
    m_smoothing->slider->setMinimum(5);
    m_interpolate = new ToggleField("低置信度关键点插值补全", true);
    m_interpolate->setObjectName("interpolateMissing");
    params->body->addWidget(m_conf);
    params->body->addWidget(nms);
    params->body->addWidget(m_smoothing);
    params->body->addWidget(row("低置信度关键点插值补全", m_interpolate));
    mid->addWidget(params, 2);
    auto points = new CardWidget({}, content);
    points->body->setSpacing(8);
    points->setMinimumWidth(300);
    points->setMaximumWidth(380);
    auto header = new QHBoxLayout;
    header->addWidget(title("参与角度计算的关键点"));
    header->addStretch();
    m_count = label("", true);
    m_count->setStyleSheet("color:#FF8800;font-size:12px;");
    header->addWidget(m_count);
    points->body->addLayout(header);
    const QStringList keys = {"左肩", "右肩", "左肘", "右肘", "左腕", "右腕",
                              "左髋", "右髋", "左膝", "右膝", "左踝", "右踝"};
    for (int line = 0; line < 2; line++) {
        auto r = new QHBoxLayout;
        r->setSpacing(7);
        for (int i = line * 6; i < line * 6 + 6; i++) {
            auto b = button(keys[i], "chip");
            b->setCheckable(true);
            b->setProperty("pointName", keys[i]);
            b->setProperty("emphasis", keys[i].contains("膝"));
            m_points[keys[i]] = b;
            r->addWidget(b);
            connect(b, &QPushButton::toggled, this, &DetectionConfigPage::updateCount);
        }
        r->addStretch();
        points->body->addLayout(r);
    }
    auto nose = button("鼻（样例未启用）", "noseUnavailable");
    nose->setFixedHeight(26);
    nose->setStyleSheet("padding:4px 9px;font-size:11px;");
    nose->setEnabled(false);
    nose->setToolTip("真实关键点预设将在姿态模块接入后启用");
    points->body->addWidget(nose, 0, Qt::AlignLeft);
    mid->addWidget(points, 1, Qt::AlignTop);
    root->addLayout(mid);
    auto actions = new QHBoxLayout;
    m_status = label("当前为样例草稿；保存仅验证配置交互与本机持久化。", true);
    m_status->setWordWrap(true);
    actions->addWidget(m_status, 1);
    auto reset = button("恢复默认", "resetPoseConfig");
    m_save = button("保存配置", "savePoseConfig", "primary");
    actions->addWidget(reset);
    actions->addWidget(m_save);
    root->addLayout(actions);
    root->addStretch();
    scroll->setWidget(content);
    outer->addWidget(scroll);
    connect(reset, &QPushButton::clicked, this, [this] {
        UiConfig defaults;
        load(defaults);
        m_status->setText("已恢复默认草稿，点击保存配置后生效。");
    });
    connect(m_save, &QPushButton::clicked, this, [this, c] { c->apply(draft()); });
    connect(c, &SettingsController::feedback, this, [this](const QString &m, bool ok) {
        m_status->setText(m);
        m_status->setStyleSheet(ok ? "color:#A1A1AA;" : "color:#FF6B80;");
    });
    connect(s, &AppState::configChanged, this, [this] { load(m_state->config); });
    connect(s, &AppState::detectionChanged, this,
            [this] { m_save->setEnabled(!m_state->recording); });
    load(s->config);
}
UiConfig DetectionConfigPage::draft() const {
    auto c = m_state->config;
    c.confidence = m_conf->slider->value() / 100.f;
    c.smoothing = m_smoothing->slider->value() / 100.f;
    c.interpolate = m_interpolate->isChecked();
    c.enabledPoints.clear();
    for (auto it = m_points.begin(); it != m_points.end(); ++it)
        if (it.value()->isChecked())
            c.enabledPoints.append(it.key());
    return c;
}
void DetectionConfigPage::load(const UiConfig &c) {
    m_conf->setValue(qRound(c.confidence * 100));
    m_smoothing->setValue(qRound(c.smoothing * 100));
    m_interpolate->setChecked(c.interpolate);
    for (auto it = m_points.begin(); it != m_points.end(); ++it) {
        QSignalBlocker blocker(it.value());
        it.value()->setChecked(c.enabledPoints.contains(it.key()));
    }
    updateCount();
}
void DetectionConfigPage::updateCount() {
    int count = 0;
    for (auto b : m_points)
        count += b->isChecked();
    m_count->setText(QString::number(count) + "/12");
}
} // namespace ui
