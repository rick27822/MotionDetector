#include "RealtimePage.h"
#include "modules/ui/adapters/PoseSampleReader.h"
#include "modules/ui/theme/Theme.h"
#include "modules/ui/widgets/Controls.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QProgressBar>
#include <QSignalBlocker>
namespace ui {
RealtimePage::RealtimePage(AppState *s, DetectionController *controller,
                           RealtimeController *realtime, QWidget *parent)
    : QWidget(parent), m_state(s), m_realtime(realtime) {
    QString sampleError;
    PoseSampleReader::readPoses(":/fixtures/pose.json", m_samples, sampleError);
    setObjectName("page");
    auto root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    auto body = new QHBoxLayout;
    body->setSpacing(0);
    video = new VideoView;
    video->setObjectName("realtimeVideo");
    body->addWidget(video, 1);
    auto right = new QWidget;
    right->setFixedWidth(300);
    auto col = new QVBoxLayout(right);
    col->setContentsMargins(16, 31, 16, 16);
    col->setSpacing(14);
    auto angles = new CardWidget("关节角度实时值", right, 14);
    angles->setFixedHeight(193);
    angles->body->setSpacing(6);
    auto p = VideoView::referencePose();
    for (auto j : {core::JointAngle::LeftHip, core::JointAngle::RightHip, core::JointAngle::Trunk,
                   core::JointAngle::LeftShoulder, core::JointAngle::LeftKnee}) {
        auto v =
            label(QString::number(p.angle(j), 'f', j == core::JointAngle::Trunk ? 1 : 0) + "°");
        v->setStyleSheet(QString("font-family:'Menlo';font-size:12px;color:%1;")
                             .arg(j == core::JointAngle::LeftKnee ? "#00E5FF" : "white"));
        m_angleValues.append({j, v});
        auto r = row(core::angleName(j) + (j == core::JointAngle::LeftKnee ? "（关注）" : ""), v);
        angles->body->addWidget(r);
    }
    auto bar = m_kneeProgress = new QProgressBar;
    bar->setObjectName("realtimeKneeProgress");
    bar->setRange(0, 180);
    bar->setValue(87);
    bar->setTextVisible(false);
    bar->setFixedHeight(4);
    bar->setStyleSheet("QProgressBar{background:#1C1C1E;border:0;border-radius:2px;}QProgressBar::"
                       "chunk{background:#00E5FF;border-radius:2px;}");
    angles->body->addWidget(bar);
    col->addWidget(angles);
    auto params = new CardWidget("检测参数", right, 14);
    params->setFixedHeight(168);
    params->body->setSpacing(5);
    m_confidence = label("");
    params->body->addWidget(row("置信度阈值", m_confidence));
    auto confidenceBar = m_confidenceProgress = new QProgressBar;
    confidenceBar->setRange(0, 100);
    confidenceBar->setTextVisible(false);
    confidenceBar->setFixedHeight(4);
    confidenceBar->setStyleSheet("QProgressBar{background:#1C1C1E;border:0;border-radius:2px;}"
                                 "QProgressBar::chunk{background:#FF8800;border-radius:2px;}");
    params->body->addWidget(confidenceBar);
    auto skeleton = new ToggleField("骨架叠加显示", true);
    skeleton->setObjectName("showSkeleton");
    auto angleSwitch = new ToggleField("关节角度标注", true);
    angleSwitch->setObjectName("showAngles");
    auto original = new ToggleField("录制原始视频");
    original->setEnabled(false);
    original->setToolTip("真实录制服务未接入，阶段一不生成 MP4");
    params->body->addWidget(row("骨架叠加显示", skeleton));
    params->body->addWidget(row("关节角度标注", angleSwitch));
    params->body->addWidget(row("录制原始视频", original));
    connect(skeleton, &QAbstractButton::toggled, this, [this](bool v) {
        m_state->showSkeleton = v;
        refresh();
    });
    connect(angleSwitch, &QAbstractButton::toggled, this, [this](bool v) {
        m_state->showAngles = v;
        refresh();
    });
    col->addWidget(params);
    auto hint = new CardWidget({}, right, 12);
    hint->setMinimumHeight(116);
    hint->setStyleSheet("QFrame#card{background:#241A06;border-radius:12px;}");
    hint->body->setSpacing(4);
    auto h = m_hintTitle = label("⚠  动作提示 · 样例");
    h->setObjectName("realtimeHintTitle");
    h->setStyleSheet("color:#FFCF00;font-size:12px;font-weight:600;");
    hint->body->addWidget(h);
    auto text = m_hintText = label("人工标注仅用于界面验收。\n未接入真实推理与动作评判服务。", true);
    text->setObjectName("realtimeHintText");
    text->setWordWrap(true);
    text->setStyleSheet("font-size:11px;color:#D4D4D8;");
    hint->body->addWidget(text);
    m_repCount = label("累计次数：—");
    m_repCount->setObjectName("realtimeRepCount");
    hint->body->addWidget(m_repCount);
    col->addWidget(hint);
    col->addStretch();
    body->addWidget(right);
    root->addLayout(body, 1);
    auto footer = new QWidget;
    footer->setFixedHeight(57);
    footer->setObjectName("realtimeFooter");
    footer->setStyleSheet("QWidget#realtimeFooter {background:#0A0A0B;}");
    auto controls = new QHBoxLayout(footer);
    controls->setContentsMargins(12, 10, 12, 10);
    controls->setSpacing(8);
    m_detect = button("停止样例", "toggleDetection", "primary");
    m_detect->setIcon(icon("stop", Qt::white));
    m_record = button("开始样例录制", "toggleRecording");
    m_record->setToolTip("保存最多12.57秒的合成角度样例会话，不录制视频");
    controls->addWidget(m_detect);
    controls->addWidget(m_record);
    auto source = m_source = new QComboBox;
    source->setObjectName("poseSampleSource");
    source->setFixedHeight(30);
    source->setAccessibleName("姿态样例来源");
    source->addItem("参考图标注", -1);
    QStringList states = {"站立", "蹲底", "起身"};
    for (int i = 0; i < m_samples.size(); i++)
        source->addItem("交接样例 · " + states.value(i), i);
    controls->addWidget(source);
    connect(source, &QComboBox::currentIndexChanged, this, [this, source] {
        m_sampleIndex = source->currentData().toInt();
        refresh();
    });
    controls->addStretch();
    auto note = m_sourceNote = label("静态参考图 · 无真实视频流", true);
    note->setObjectName("realtimeSourceNote");
    controls->addWidget(note);
    auto fit = button("", "videoFit");
    fit->setIcon(icon("expand"));
    fit->setToolTip("切换完整画面 / 填满裁剪");
    fit->setCheckable(true);
    fit->setFixedWidth(32);
    connect(fit, &QPushButton::toggled, video, [this](bool v) { video->setCover(!v); });
    controls->addWidget(fit);
    root->addWidget(footer);
    connect(m_detect, &QPushButton::clicked, controller, &DetectionController::toggleDetection);
    connect(m_record, &QPushButton::clicked, controller, &DetectionController::toggleRecording);
    connect(s, &AppState::deviceChanged, this, &RealtimePage::refresh);
    connect(s, &AppState::detectionChanged, this, &RealtimePage::refresh);
    connect(s, &AppState::configChanged, this, [this, confidenceBar] {
        refresh();
        confidenceBar->setValue(qRound(m_state->config.confidence * 100));
    });
    confidenceBar->setValue(qRound(s->config.confidence * 100));
    connect(realtime, &RealtimeController::changed, this, &RealtimePage::refresh);
    refresh();
}
void RealtimePage::refresh() {
    core::PoseResult pose = m_sampleIndex >= 0 && m_sampleIndex < m_samples.size()
                                ? m_samples[m_sampleIndex]
                                : VideoView::referencePose();
    const bool live = m_realtime->enabled();
    if (live)
        pose = m_realtime->pose();
    if (!live) {
        for (int i = 0; i < pose.keypoints.size(); i++) {
            QString name = core::keypointName(i);
            if (QStringList{"左肩", "右肩", "左肘", "右肘", "左腕", "右腕", "左髋", "右髋", "左膝",
                            "右膝", "左踝", "右踝"}
                    .contains(name) &&
                !m_state->config.enabledPoints.contains(name))
                pose.keypoints[i].valid = false;
        }
        const QVector<QVector<int>> deps = {{23, 25, 27},        {24, 26, 28},     {11, 23, 25},
                                            {12, 24, 26},        {11, 12, 23, 24}, {11, 13, 15},
                                            {12, 14, 16},        {13, 11, 23},     {14, 12, 24},
                                            {0, 11, 12, 23, 24}, {25, 27, 31},     {26, 28, 32}};
        for (int i = 0; i < deps.size(); i++)
            for (int kp : deps[i])
                if (!pose.hasKeypoint(kp))
                    pose.validAngles &= ~(1u << i);
    } // Live angles retain the producer's validity; preview masks apply only to samples.
    const bool kneeValid = pose.hasAngle(core::JointAngle::LeftKnee);
    m_kneeProgress->setValue(kneeValid ? qRound(pose.angle(core::JointAngle::LeftKnee)) : 0);
    m_kneeProgress->setEnabled(kneeValid);
    video->setPose(pose);
    video->setReferenceImageEnabled(!live && m_sampleIndex < 0);
    video->setSourceNote(live ? m_realtime->sourceName() + " · 无对应视频帧" : QString());
    for (auto pair : m_angleValues)
        pair.second->setText(pose.hasAngle(pair.first)
                                 ? QString::number(pose.angle(pair.first), 'f',
                                                   pair.first == core::JointAngle::Trunk ? 1 : 0) +
                                       "°"
                                 : "—");
    video->setAvailable(m_state->connected && m_state->detecting);
    video->setOverlay(m_state->showSkeleton, m_state->showAngles);
    video->setMirror(m_state->config.mirror);
    m_detect->setText(live ? (m_state->detecting ? "停止检测" : "开始检测")
                           : (m_state->detecting ? "停止样例" : "开始样例检测"));
    m_detect->setEnabled(m_state->connected);
    m_record->setText(m_state->recording ? "停止样例录制" : "开始样例录制");
    m_record->setEnabled(!live && m_state->connected && m_state->detecting);
    if (live) {
        m_record->setText("视频录制未接入");
        m_record->setToolTip("当前只接入姿态与实时提示，不生成视频或录制会话");
    }
    m_source->setEnabled(!live);
    if (live && m_source->currentText() != m_realtime->sourceName()) {
        QSignalBlocker blocker(m_source);
        m_source->clear();
        m_source->addItem(m_realtime->sourceName());
    }
    m_confidenceProgress->setVisible(!live);
    if (live) {
        const auto &r = m_realtime->result();
        m_hintTitle->setText("动作提示");
        m_hintText->setText(QString::fromStdString(r.message));
        m_hintText->setTextFormat(Qt::PlainText);
        m_repCount->setText(QString("累计次数：%1").arg(qulonglong(r.completedReps)));
        m_sourceNote->setText(m_realtime->sourceName());
        const bool warning = r.code == motion::RealtimeHintCode::DataLost ||
                             r.code == motion::RealtimeHintCode::InvalidInput;
        m_hintTitle->setStyleSheet(warning ? "color:#FFCF00;font-size:12px;font-weight:600;"
                                          : "color:#00D776;font-size:12px;font-weight:600;");
    }
    m_record->setProperty("variant", m_state->recording ? "danger" : "");
    polish(m_record);
    m_confidence->setText(live ? "由姿态源配置" : QString::number(m_state->config.confidence, 'f', 2));
}
} // namespace ui
