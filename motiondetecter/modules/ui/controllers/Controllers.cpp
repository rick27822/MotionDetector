#include "Controllers.h"
#include "modules/ui/adapters/AnalysisM0Adapter.h"
#include "modules/ui/adapters/PoseSampleReader.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <cmath>
namespace ui {
QJsonObject UiConfig::toJson() const {
    return {{"schemaVersion", 1},
            {"confidence", confidence},
            {"smoothing", smoothing},
            {"interpolate", interpolate},
            {"nose", nose},
            {"mirror", mirror},
            {"resolution", resolution},
            {"fps", fps},
            {"threads", threads},
            {"storagePath", storagePath},
            {"enabledPoints", QJsonArray::fromStringList(enabledPoints)}};
}
bool UiConfig::fromJson(const QJsonObject &o, UiConfig &out, QString &error) {
    if (o.value("schemaVersion").toInt() != 1) {
        error = "不支持的配置版本";
        return false;
    }
    UiConfig c;
    c.confidence = float(o.value("confidence").toDouble(-1));
    c.smoothing = float(o.value("smoothing").toDouble(-1));
    c.interpolate = o.value("interpolate").toBool(true);
    c.nose = o.value("nose").toBool(false);
    c.mirror = o.value("mirror").toBool(false);
    c.resolution = o.value("resolution").toString();
    c.fps = o.value("fps").toInt();
    c.threads = o.value("threads").toInt();
    c.storagePath = o.value("storagePath").toString();
    c.enabledPoints.clear();
    for (auto v : o.value("enabledPoints").toArray())
        c.enabledPoints.append(v.toString());
    if (!std::isfinite(c.confidence) || !std::isfinite(c.smoothing) || c.confidence < 0 ||
        c.confidence > 1 || c.smoothing < .05f || c.smoothing > 1 ||
        !QStringList{"1920 × 1080", "1280 × 720", "640 × 480"}.contains(c.resolution) ||
        !QList<int>{15, 25, 30, 60}.contains(c.fps) || c.threads < 1 || c.threads > 16 ||
        c.storagePath.isEmpty()) {
        error = "配置值无效，原配置已保留";
        return false;
    }
    const QStringList allowed = {"左肩", "右肩", "左肘", "右肘", "左腕", "右腕",
                                 "左髋", "右髋", "左膝", "右膝", "左踝", "右踝"};
    for (const auto &p : c.enabledPoints)
        if (!allowed.contains(p)) {
            error = "未知关键点";
            return false;
        }
    c.enabledPoints.removeDuplicates();
    out = c;
    error.clear();
    return true;
}
SettingsController::SettingsController(AppState *s, QString d, QObject *p)
    : QObject(p), m_state(s), m_directory(std::move(d)) {
    m_state->config.storagePath = m_directory + "/demo-sessions";
    QFile f(configFile());
    if (f.exists()) {
        if (!f.open(QFile::ReadOnly)) {
            m_error = f.errorString();
            return;
        }
        QJsonParseError pe;
        auto doc = QJsonDocument::fromJson(f.readAll(), &pe);
        UiConfig c;
        if (pe.error != QJsonParseError::NoError || !UiConfig::fromJson(doc.object(), c, m_error)) {
            if (m_error.isEmpty())
                m_error = "配置文件损坏，使用默认值";
            return;
        }
        m_state->config = c;
    }
}
QString SettingsController::configFile() const {
    return m_directory + "/ui-config.json";
}
bool SettingsController::apply(const UiConfig &config) {
    UiConfig validated;
    QString err;
    if (!UiConfig::fromJson(config.toJson(), validated, err)) {
        m_error = err;
        emit feedback(err, false);
        return false;
    }
    if (m_state->recording) {
        m_error = "请先停止样例录制，再修改配置";
        emit feedback(m_error, false);
        return false;
    }
    if (m_fail) {
        m_error = "模拟保存失败：配置未改变";
        emit feedback(m_error, false);
        return false;
    }
    if (!QDir().mkpath(m_directory)) {
        m_error = "无法创建配置目录";
        emit feedback(m_error, false);
        return false;
    }
    QSaveFile f(configFile());
    if (!f.open(QFile::WriteOnly)) {
        m_error = f.errorString();
        emit feedback(m_error, false);
        return false;
    }
    auto bytes = QJsonDocument(validated.toJson()).toJson();
    if (f.write(bytes) != bytes.size() || !f.commit()) {
        m_error = f.errorString();
        emit feedback(m_error, false);
        return false;
    }
    m_state->config = validated;
    m_error.clear();
    emit m_state->configChanged();
    emit feedback("界面样例配置已保存，重启后保留；尚未下发真实设备或模型", true);
    return true;
}
DeviceController::DeviceController(AppState *s, QObject *p) : QObject(p), m_state(s) {}
void DeviceController::connectDevice(const QString &id) {
    if (m_state->realtimeInput) {
        emit feedback("当前姿态输入由主程序管理，请重新绑定输入源", false);
        return;
    }
    if (m_busy)
        return;
    if (id != "iphone" && id != "xiaomi") {
        emit feedback("该样例设备离线", false);
        return;
    }
    m_busy = true;
    emit busyChanged(true);
    int generation = ++m_generation;
    QTimer::singleShot(450, this, [this, id, generation] {
        if (generation != m_generation)
            return;
        m_busy = false;
        emit busyChanged(false);
        if (m_fail) {
            emit feedback("模拟连接失败，请重试", false);
            return;
        }
        m_state->deviceId = id;
        m_state->deviceName = id == "iphone" ? "iPhone 15 Pro" : "Xiaomi 14 Ultra";
        m_state->connected = true;
        emit m_state->deviceChanged();
        emit feedback("样例设备已连接；未连接真实手机", true);
    });
}
void DeviceController::disconnectDevice() {
    ++m_generation;
    m_busy = false;
    emit busyChanged(false);
    m_state->connected = false;
    m_state->detecting = false;
    emit m_state->deviceChanged();
    emit m_state->detectionChanged();
    emit feedback("样例设备已断开", true);
}
void DeviceController::refresh() {
    if (m_busy)
        return;
    m_busy = true;
    emit busyChanged(true);
    int g = ++m_generation;
    QTimer::singleShot(350, this, [this, g] {
        if (g != m_generation)
            return;
        m_busy = false;
        emit busyChanged(false);
        emit feedback("样例设备列表已刷新：2 台在线，1 台离线", true);
    });
}
DetectionController::DetectionController(AppState *s, QObject *p) : QObject(p), m_state(s) {
    m_tick.setInterval(100);
    connect(&m_tick, &QTimer::timeout, this, [this] { emit elapsedChanged(recordingMs()); });
}
void DetectionController::toggleDetection() {
    if (!m_state->connected) {
        emit feedback(m_realtimeMode ? "请先重新绑定姿态输入" : "请先在设备连接页连接样例设备", false);
        return;
    }
    if (m_state->detecting) {
        stop();
        return;
    }
    m_state->detecting = true;
    emit m_state->detectionChanged();
    emit feedback(m_realtimeMode ? "实时提示已开始，等待姿态输入"
                                : "样例展示已开始：静态参考图与人工标注，不运行真实推理", true);
}
void DetectionController::stop() {
    if (m_state->recording)
        toggleRecording();
    m_state->detecting = false;
    emit m_state->detectionChanged();
}
void DetectionController::toggleRecording() {
    if (m_realtimeMode) {
        emit feedback("当前仅联调实时提示，真实视频录制服务尚未接入", false);
        return;
    }
    if (m_state->recording) {
        auto duration = qMax<qint64>(1, recordingMs());
        m_state->recording = false;
        m_tick.stop();
        emit m_state->detectionChanged();
        emit sampleRecorded(duration);
        emit feedback("样例会话已保存（角度数据，无 MP4 视频）", true);
        return;
    }
    if (!m_state->connected || !m_state->detecting) {
        emit feedback("请先连接样例设备并开始样例检测", false);
        return;
    }
    if (m_fail) {
        emit feedback("模拟录制启动失败，未创建会话", false);
        return;
    }
    m_state->recording = true;
    m_clock.start();
    m_tick.start();
    emit m_state->detectionChanged();
    emit elapsedChanged(0);
}
qint64 DetectionController::recordingMs() const {
    return m_state->recording && m_clock.isValid() ? m_clock.elapsed() : 0;
}
SessionController::SessionController(AppState *s, QString d, QObject *p)
    : QObject(p), m_state(s), m_directory(std::move(d)),
      m_analysis(std::make_unique<AnalysisM0Adapter>()) {
    QVector<core::AngleSample> samples;
    if (!PoseSampleReader::readSeries(":/fixtures/series.jsonl", samples, m_loadError))
        return;
    for (int i = 0; i < 3; i++) {
        DemoSession a;
        a.id = QString("sample-%1").arg(42 - i, 3, 10, QChar('0'));
        a.name = i == 2 ? "引体向上 #018" : "深蹲训练 #0" + QString::number(42 - i);
        a.date = i == 0 ? "10-16 14:20" : "10-15 09:12";
        a.samples = samples;
        if (i == 2)
            a.status = "角度样例";
        m_state->sessions.append(a);
    }
    QFile saved(m_directory + "/demo-sessions.json");
    if (saved.open(QFile::ReadOnly)) {
        auto doc = QJsonDocument::fromJson(saved.readAll());
        for (auto value : doc.array()) {
            auto o = value.toObject();
            if (!o.value("id").toString().startsWith("demo-"))
                continue;
            DemoSession a;
            a.id = o.value("id").toString();
            a.name = o.value("name").toString();
            a.date = o.value("date").toString();
            a.durationMs = qBound<qint64>(qint64(1), o.value("durationMs").toVariant().toLongLong(),
                                          qint64(12573));
            a.status = "样例录制 · 无视频";
            for (const auto &v : samples)
                if (v.t <= a.durationMs)
                    a.samples.append(v);
            m_state->sessions.prepend(a);
        }
    }
    m_seed = m_state->sessions;
    m_timer.setInterval(33);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        auto s = current();
        if (!s) {
            m_timer.stop();
            return;
        }
        qint64 delta = m_tick.restart();
        m_position = qMin(s->durationMs, m_position + qRound64(delta * m_rate));
        emit positionChanged(m_position, s->durationMs);
        if (m_position >= s->durationMs) {
            m_playing = false;
            m_timer.stop();
            emit playbackChanged(false);
        }
    });
}
const DemoSession *SessionController::current() const {
    for (const auto &s : m_state->sessions)
        if (s.id == m_state->sessionId)
            return &s;
    return nullptr;
}
void SessionController::select(const QString &id) {
    if (id == m_state->sessionId)
        return;
    bool found = false;
    for (const auto &s : m_state->sessions)
        found |= s.id == id;
    if (!found) {
        emit feedback("样例会话不存在", false);
        return;
    }
    m_timer.stop();
    m_playing = false;
    m_position = 0;
    m_state->sessionId = id;
    emit m_state->sessionChanged();
    emit playbackChanged(false);
    emit positionChanged(0, current()->durationMs);
}
void SessionController::togglePlayback() {
    auto s = current();
    if (!s) {
        emit feedback("没有可播放的样例会话", false);
        return;
    }
    m_playing = !m_playing;
    if (m_playing) {
        if (m_position >= s->durationMs)
            m_position = 0;
        m_tick.start();
        m_timer.start();
    } else
        m_timer.stop();
    emit playbackChanged(m_playing);
    emit positionChanged(m_position, s->durationMs);
}
void SessionController::seek(qint64 t) {
    auto s = current();
    if (!s)
        return;
    m_position = qBound<qint64>(qint64(0), t, s->durationMs);
    if (m_playing)
        m_tick.restart();
    emit positionChanged(m_position, s->durationMs);
}
void SessionController::setRate(double r) {
    if (r == .5 || r == 1 || r == 1.5 || r == 2)
        m_rate = r;
}
void SessionController::addRecorded(qint64 duration) {
    if (m_seed.isEmpty())
        return;
    DemoSession a;
    a.id = "demo-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    a.name = "样例录制 " + QDateTime::currentDateTime().toString("HH:mm:ss");
    a.date = QDateTime::currentDateTime().toString("MM-dd HH:mm");
    a.durationMs = qBound<qint64>(qint64(1), duration, qint64(12573));
    a.status = "样例录制 · 无视频";
    const auto source = m_seed.last().samples;
    for (const auto &s : source)
        if (s.t <= a.durationMs)
            a.samples.append(s);
    bool wasEmpty = m_state->sessions.isEmpty();
    m_state->sessions.prepend(a);
    if (wasEmpty)
        m_state->sessionId = a.id;
    saveDemoSessions();
    emit m_state->sessionsChanged();
    if (wasEmpty)
        emit m_state->sessionChanged();
}
void SessionController::saveDemoSessions() {
    QJsonArray a;
    for (const auto &s : m_state->sessions)
        if (s.id.startsWith("demo-"))
            a.append(QJsonObject{
                {"id", s.id}, {"name", s.name}, {"date", s.date}, {"durationMs", s.durationMs}});
    QDir().mkpath(m_directory);
    QSaveFile f(m_directory + "/demo-sessions.json");
    if (!f.open(QFile::WriteOnly)) {
        emit feedback("样例会话仅在本次运行可用：" + f.errorString(), false);
        return;
    }
    auto data = QJsonDocument(a).toJson();
    if (f.write(data) != data.size() || !f.commit())
        emit feedback("样例会话持久化失败", false);
}
AnalysisViewData SessionController::analysis() const {
    auto s = current();
    return s ? m_analysis->analyze(s->samples) : AnalysisViewData{};
}
bool SessionController::exportSample(const QString &path, bool csv) {
    auto s = current();
    if (!s) {
        emit feedback("没有可导出的样例会话", false);
        return false;
    }
    if (m_failExport) {
        emit feedback("模拟导出失败：未生成文件", false);
        return false;
    }
    if (path.isEmpty())
        return false;
    QByteArray data;
    if (csv) {
        data = "\xEF\xBB\xBF";
        data += "time_ms,left_knee_deg,right_knee_deg,trunk_deg,source\n";
        for (const auto &p : s->samples) {
            QString line = QString::number(p.t);
            for (auto j :
                 {core::JointAngle::LeftKnee, core::JointAngle::RightKnee, core::JointAngle::Trunk})
                line += "," + (p.hasAngle(j) ? QString::number(p.angle(j), 'f', 3) : QString());
            data += line.toUtf8() + ",synthetic-ui-sample\n";
        }
    } else {
        auto a = analysis();
        QString html =
            QString("<!doctype html><html lang='zh-CN'><meta charset='utf-8'><title>PoseLab "
                    "界面样例报告</title><style>body{font:16px "
                    "-apple-system,sans-serif;max-width:800px;margin:60px "
                    "auto;line-height:1.8;color:#222}h1{color:#ce6b00}.note{background:#fff0d9;"
                    "padding:16px}table{border-collapse:collapse}td{padding:8px 30px 8px "
                    "0}</style><h1>PoseLab · 界面样例报告</h1><p class='note'>本报告仅验证 UI "
                    "导出流程。数据来自合成角度样例；指标来自 M0 固定 "
                    "Mock；不代表真人动作评估。</p><h2>%1</h2><p>会话：%2 · "
                    "采样点：%3</p><table><tr><td>样例评分（10分制换算）</td><td>%4 / "
                    "100</td></tr><tr><td>样例最大膝角</td><td>%5°</td></tr><tr><td>样例活动度</"
                    "td><td>%6°</td></tr><tr><td>样例峰值角速度</td><td>%7°/s</td></tr></"
                    "table><p>当前未接入真实采集、真实推理或真实评分服务。</p></html>")
                .arg(s->name.toHtmlEscaped(), s->id.toHtmlEscaped())
                .arg(s->samples.size())
                .arg(a.scorePercent)
                .arg(a.maximum)
                .arg(a.rom)
                .arg(a.peakSpeed);
        data = html.toUtf8();
    }
    QSaveFile file(path);
    if (!file.open(QFile::WriteOnly)) {
        emit feedback("导出失败：" + file.errorString(), false);
        return false;
    }
    if (file.write(data) != data.size() || !file.commit()) {
        emit feedback("导出失败：" + file.errorString(), false);
        return false;
    }
    emit feedback("样例文件已导出：" + path, true);
    return true;
}
void SessionController::restoreSamples() {
    if (!m_state->sessions.isEmpty())
        return;
    m_state->sessions = m_seed;
    m_state->sessionId = "sample-042";
    m_position = 3300;
    emit m_state->sessionsChanged();
    emit m_state->sessionChanged();
    emit positionChanged(m_position, current() ? current()->durationMs : 0);
}
void SessionController::setEmpty() {
    m_timer.stop();
    m_playing = false;
    m_position = 0;
    m_state->sessions.clear();
    m_state->sessionId.clear();
    emit m_state->sessionsChanged();
    emit m_state->sessionChanged();
    emit playbackChanged(false);
    emit positionChanged(0, 0);
}
} // namespace ui
