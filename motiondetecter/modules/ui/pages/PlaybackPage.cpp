#include "PlaybackPage.h"
#include "modules/ui/theme/Theme.h"
#include "modules/ui/widgets/Controls.h"
#include <QResizeEvent>
#include <QScrollArea>
#include <QSignalBlocker>
namespace ui {
PlaybackPage::PlaybackPage(AppState *s, SessionController *c, QWidget *p)
    : QWidget(p), m_state(s), m_controller(c) {
    setObjectName("page");
    auto root = new QHBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(20);
    auto left = new QVBoxLayout;
    left->setSpacing(14);
    video = new VideoView(true);
    video->setObjectName("playbackVideo");
    left->addWidget(video, 5);
    auto controls = new QHBoxLayout;
    controls->setSpacing(12);
    m_play = button("", "playPause");
    m_play->setIcon(icon("play", Qt::white));
    m_play->setFixedSize(24, 34);
    m_play->setStyleSheet("background:transparent;padding:0;border:0;");
    m_play->setAccessibleName("播放或暂停样例时间轴");
    m_progress = new FlatSlider;
    m_progress->setObjectName("playbackPosition");
    m_progress->setAccessibleName("样例播放进度");
    m_time = label("", true);
    m_time->setStyleSheet("font-family:'Menlo';font-size:10px;color:#A1A1AA;");
    auto speed = new QComboBox;
    speed->setFixedHeight(30);
    speed->setObjectName("playbackRate");
    for (double r : {.5, 1., 1.5, 2.})
        speed->addItem(QString::number(r, 'f', 1) + "x", r);
    speed->setCurrentIndex(1);
    speed->setFixedWidth(68);
    controls->addWidget(m_play);
    controls->addWidget(m_progress, 1);
    controls->addWidget(m_time);
    controls->addWidget(speed);
    left->addLayout(controls);
    auto curve = new CardWidget({}, this, 16);
    auto header = new QHBoxLayout;
    header->addWidget(title("关节角度 · 时间曲线"));
    header->addStretch();
    auto legend =
        label("<font color='#00E5FF'>●</font> 左膝　<font color='#77777D'>●</font> 右膝", true);
    header->addWidget(legend);
    curve->body->addLayout(header);
    chart = new ChartView;
    chart->setObjectName("playbackChart");
    curve->body->addWidget(chart, 1);
    left->addWidget(curve, 3);
    root->addLayout(left, 1);
    auto sessions = new CardWidget({}, this, 16);
    sessions->setFixedWidth(360);
    auto h = new QHBoxLayout;
    h->addWidget(title("录像会话"));
    h->addStretch();
    m_count = label("", true);
    h->addWidget(m_count);
    sessions->body->addLayout(h);
    auto scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto list = new QWidget;
    m_list = new QVBoxLayout(list);
    m_list->setContentsMargins(0, 0, 0, 0);
    m_list->setSpacing(10);
    scroll->setWidget(list);
    sessions->body->addWidget(scroll, 1);
    auto note = label("播放的是合成角度样例时间轴，画面为静态参考图。\n未解码视频；无连续关键点，回"
                      "放不绘制动态骨架。",
                      true);
    note->setWordWrap(true);
    note->setStyleSheet("font-size:11px;color:#A1A1AA;");
    sessions->body->addWidget(note);
    root->addWidget(sessions);
    // Static manual reference overlay; it never pretends to be a per-frame replay result.
    video->setOverlay(true, false);
    connect(m_play, &QPushButton::clicked, c, &SessionController::togglePlayback);
    connect(video, &VideoView::playRequested, c, &SessionController::togglePlayback);
    connect(speed, &QComboBox::currentIndexChanged, this,
            [c, speed] { c->setRate(speed->currentData().toDouble()); });
    connect(m_progress, &QSlider::sliderMoved, c, [c](int v) { c->seek(v); });
    connect(m_progress, &QSlider::sliderReleased, this,
            [this, c] { c->seek(m_progress->value()); });
    connect(m_progress, &QSlider::valueChanged, this, [this, c](int v) {
        if (!m_progress->isSliderDown() && v != c->position())
            c->seek(v);
    });
    connect(chart, &ChartView::seekRequested, c, &SessionController::seek);
    connect(c, &SessionController::positionChanged, this, [this](qint64 t, qint64 d) {
        if (!m_progress->isSliderDown()) {
            QSignalBlocker block(m_progress);
            m_progress->setRange(0, int(d));
            m_progress->setValue(int(t));
        }
        m_time->setText(core::formatDuration(t) + " / " + core::formatDuration(d));
        chart->setPosition(t);
        video->setPosition(t, d);
    });
    connect(c, &SessionController::playbackChanged, this, [this](bool playing) {
        m_play->setIcon(icon(playing ? "pause" : "play", Qt::white));
        video->setPlaying(playing);
    });
    connect(s, &AppState::sessionsChanged, this, &PlaybackPage::rebuild);
    connect(s, &AppState::sessionChanged, this, &PlaybackPage::refresh);
    rebuild();
    refresh();
}
void PlaybackPage::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    video->setMaximumHeight(qMax(260, qRound((width() - 428) * 461. / 787.)));
}
void PlaybackPage::rebuild() {
    while (auto i = m_list->takeAt(0)) {
        if (i->widget())
            i->widget()->deleteLater();
        delete i;
    }
    for (const auto &s : m_state->sessions) {
        auto b = button("", "session");
        b->setCheckable(true);
        b->setChecked(s.id == m_state->sessionId);
        b->setProperty("sessionId", s.id);
        b->setMinimumHeight(58);
        auto v = new QVBoxLayout(b);
        v->setContentsMargins(12, 8, 12, 8);
        v->setSpacing(4);
        auto name = label(s.name);
        auto detail =
            label(s.date + " · " + core::formatDuration(s.durationMs) + " · " + s.status, true);
        detail->setStyleSheet("font-size:10px;color:#A1A1AA;");
        name->setAttribute(Qt::WA_TransparentForMouseEvents);
        detail->setAttribute(Qt::WA_TransparentForMouseEvents);
        v->addWidget(name);
        v->addWidget(detail);
        m_list->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, id = s.id] { m_controller->select(id); });
    }
    if (m_state->sessions.isEmpty())
        m_list->addWidget(label("暂无会话\n可在实时检测页创建样例会话。", true));
    m_list->addStretch();
    m_count->setText(QString::number(m_state->sessions.size()) + " 条样例");
}
void PlaybackPage::refresh() {
    auto s = m_controller->current();
    chart->setSamples(s ? s->samples : QVector<core::AngleSample>{});
    chart->setPosition(m_controller->position());
    video->setAvailable(s != nullptr);
    m_play->setEnabled(s != nullptr);
    m_progress->setEnabled(s != nullptr);
    {
        QSignalBlocker b(m_progress);
        m_progress->setRange(0, s ? int(s->durationMs) : 0);
        m_progress->setValue(int(m_controller->position()));
    }
    m_time->setText(core::formatDuration(m_controller->position()) + " / " +
                    core::formatDuration(s ? s->durationMs : 0));
    video->setPosition(m_controller->position(), s ? s->durationMs : 0);
    for (auto b : findChildren<QPushButton *>())
        if (b->objectName() == "session")
            b->setChecked(b->property("sessionId").toString() == m_state->sessionId);
}
} // namespace ui
