#include "AnalysisPage.h"
#include <QDir>
#include <QFileDialog>
#include <QProgressBar>
namespace ui {
AnalysisPage::AnalysisPage(AppState *s, SessionController *c, QWidget *p)
    : QWidget(p), m_state(s), m_controller(c) {
    setObjectName("page");
    auto root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(20);
    auto metrics = new QHBoxLayout;
    metrics->setSpacing(16);
    QStringList names = {"最大膝关节角度", "活动度 ROM", "峰值角速度", "左右对称性"};
    for (int i = 0; i < 4; i++) {
        auto card = new CardWidget({}, this, 16);
        card->setFixedHeight(104);
        card->body->setSpacing(5);
        card->body->addWidget(label(names[i], true));
        auto value = label("—");
        value->setProperty("role", "metric");
        if (i == 3)
            value->setStyleSheet("color:#00E5FF;font-size:27px;font-weight:600;");
        m_metrics.append(value);
        card->body->addWidget(value);
        auto delta = label("M0 固定模拟指标", true);
        delta->setStyleSheet("font-size:10px;color:#00D776;");
        card->body->addWidget(delta);
        metrics->addWidget(card, 1);
    }
    root->addLayout(metrics);
    auto columns = new QHBoxLayout;
    columns->setSpacing(20);
    auto main = new CardWidget({}, this, 18);
    auto h = new QHBoxLayout;
    m_heading = title("角度 · 时间曲线（合成样例）");
    h->addWidget(m_heading);
    h->addStretch();
    auto legend =
        label("<font color='#00E5FF'>●</font> 左膝　<font color='#FF8800'>●</font> 躯干倾角", true);
    legend->setStyleSheet("font-size:10px;");
    h->addWidget(legend);
    main->body->addLayout(h);
    m_chart = new ChartView;
    m_chart->setSecondChannel(core::JointAngle::Trunk);
    m_chart->setPosition(9100);
    main->body->addWidget(m_chart, 1);
    m_status = label("", true);
    m_status->setWordWrap(true);
    m_status->setStyleSheet("font-size:11px;color:#A1A1AA;");
    main->body->addWidget(m_status);
    columns->addWidget(main, 1);
    auto right = new QVBoxLayout;
    right->setContentsMargins(0, 0, 0, 0);
    right->setSpacing(20);
    auto score = new CardWidget({}, this, 18);
    score->setFixedHeight(225);
    auto st = title("动作质量评分");
    st->setAlignment(Qt::AlignCenter);
    score->body->addWidget(st);
    m_ring = new ScoreRing;
    m_ring->setMinimumHeight(130);
    score->body->addWidget(m_ring, 1);
    auto desc = label("模拟评分 · 由 10 分制换算\n未接入真实动作评判", true);
    desc->setStyleSheet("font-size:10px;color:#A1A1AA;");
    desc->setAlignment(Qt::AlignCenter);
    score->body->addWidget(desc);
    right->addWidget(score);
    auto history = new CardWidget("历史会话对比 · 样例", this, 18);
    history->body->setSpacing(12);
    QStringList ids = {"#040", "#041", "#042 本次"};
    QList<int> values = {96, 104, 112};
    for (int i = 0; i < 3; i++) {
        auto r = new QHBoxLayout;
        auto name = label(ids[i], true);
        if (i == 2)
            name->setStyleSheet("color:#FF8800;font-size:11px;");
        r->addWidget(name);
        r->addStretch();
        r->addWidget(label(QString::number(values[i]) + "°", true));
        history->body->addLayout(r);
        auto bar = new QProgressBar;
        bar->setRange(0, 130);
        bar->setValue(values[i]);
        bar->setTextVisible(false);
        bar->setFixedHeight(6);
        bar->setStyleSheet(QString("QProgressBar{background:#1C1C1E;border:0;border-radius:3px;}"
                                   "QProgressBar::chunk{background:%1;border-radius:3px;}")
                               .arg(i == 2 ? "#FF8800" : "#626269"));
        history->body->addWidget(bar);
    }
    auto note = label("固定视觉样例，非当前会话的真实历史。", true);
    note->setWordWrap(true);
    note->setStyleSheet("font-size:10px;color:#71717A;");
    history->body->addWidget(note);
    history->body->addStretch();
    right->addWidget(history, 1);
    auto actions = new QHBoxLayout;
    actions->setSpacing(10);
    m_report = button("导出样例报告", "exportReport", "primary");
    m_csv = button("导出样例 CSV", "exportCsv");
    actions->addWidget(m_report);
    actions->addWidget(m_csv);
    right->addLayout(actions);
    auto rightWidget = new QWidget;
    rightWidget->setFixedWidth(300);
    rightWidget->setLayout(right);
    columns->addWidget(rightWidget);
    root->addLayout(columns, 1);
    connect(m_csv, &QPushButton::clicked, this, [this] { exportFile(true); });
    connect(m_report, &QPushButton::clicked, this, [this] { exportFile(false); });
    connect(s, &AppState::sessionChanged, this, &AnalysisPage::refresh);
    connect(s, &AppState::sessionsChanged, this, &AnalysisPage::refresh);
    connect(c, &SessionController::feedback, this, [this](const QString &m, bool ok) {
        m_status->setText(m);
        m_status->setStyleSheet(ok ? "color:#A1A1AA;font-size:11px;"
                                   : "color:#FF6B80;font-size:11px;");
    });
    refresh();
}
void AnalysisPage::refresh() {
    auto a = m_controller->analysis();
    auto s = m_controller->current();
    m_chart->setSamples(s ? s->samples : QVector<core::AngleSample>{});
    QStringList values = {QString::number(a.maximum, 'f', 0) + "°",
                          QString::number(a.rom, 'f', 0) + "°",
                          QString::number(a.peakSpeed, 'f', 0) + "°/s",
                          QString::number(a.symmetryPercent, 'f', 0) + "%"};
    for (int i = 0; i < 4; i++)
        m_metrics[i]->setText(a.valid ? values[i] : "—");
    m_ring->setScore(a.valid ? a.scorePercent : -1, a.valid ? a.grade : "暂无数据");
    m_status->setText(s ? "会话：" + s->name +
                              " · 合成角度序列；指标来自 M0 固定 Mock，不构成真实分析结果。"
                        : "暂无会话，请在实时检测页创建样例会话。");
    m_csv->setEnabled(s != nullptr);
    m_report->setEnabled(s != nullptr);
}
void AnalysisPage::exportFile(bool csv) {
    auto s = m_controller->current();
    if (!s)
        return;
    QString dir = m_state->config.storagePath;
    QDir().mkpath(dir);
    auto path = QFileDialog::getSaveFileName(
        this, csv ? "导出合成角度样例 CSV" : "导出 UI 样例 HTML 报告",
        QDir(dir).filePath("PoseLab-" + s->id + (csv ? ".csv" : ".html")),
        csv ? "CSV 文件 (*.csv)" : "HTML 报告 (*.html)");
    if (!path.isEmpty())
        m_controller->exportSample(path, csv);
}
} // namespace ui
