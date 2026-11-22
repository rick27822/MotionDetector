#include "types.h"

#include <QJsonValue>
#include <QStringList>
#include <QtMath>

namespace core {

namespace {

struct KeypointMeta {
    const char *name;
};

const KeypointMeta kKeypointMeta[kKeypointCount] = {
    {"鼻"},         {"左眼内"},     {"左眼"},       {"左眼外"},
    {"右眼内"},     {"右眼"},       {"右眼外"},     {"左耳"},
    {"右耳"},       {"左嘴角"},     {"右嘴角"},     {"左肩"},
    {"右肩"},       {"左肘"},       {"右肘"},       {"左腕"},
    {"右腕"},       {"左小指"},     {"右小指"},     {"左食指"},
    {"右食指"},     {"左拇指"},     {"右拇指"},     {"左髋"},
    {"右髋"},       {"左膝"},       {"右膝"},       {"左踝"},
    {"右踝"},       {"左脚跟"},     {"右脚跟"},     {"左脚尖"},
    {"右脚尖"},
};

const char *kAngleNames[kAngleCount] = {
    "左膝", "右膝", "左髋", "右髋", "躯干", "左肘",
    "右肘", "左肩", "右肩", "颈部", "左踝", "右踝",
};

QJsonValue toJson(QString v) { return QJsonValue(v); }

} // namespace

QString keypointName(int index)
{
    if (index < 0 || index >= kKeypointCount) return QStringLiteral("-");
    return QString::fromUtf8(kKeypointMeta[index].name);
}

QStringList keypointGroupNames()
{
    return {QStringLiteral("头部"), QStringLiteral("肩臂"), QStringLiteral("手部"),
            QStringLiteral("下肢")};
}

QVector<int> keypointGroup(int groupIndex)
{
    QVector<int> ids;
    switch (groupIndex) {
    case 0: // 头部
        for (int i = 0; i <= 10; ++i) ids << i;
        break;
    case 1: // 肩臂
        for (int i = 11; i <= 16; ++i) ids << i;
        break;
    case 2: // 手部
        for (int i = 17; i <= 22; ++i) ids << i;
        break;
    case 3: // 下肢
        for (int i = 23; i <= 32; ++i) ids << i;
        break;
    default:
        break;
    }
    return ids;
}

quint64 presetKeypointMask(int preset)
{
    // COCO-17 在 BlazePose 33 中的索引
    static const int coco[17] = {0, 2, 5, 7, 8, 11, 12, 13, 14,
                                 15, 16, 23, 24, 25, 26, 27, 28};
    quint64 mask = 0;
    if (preset >= 33) return 0x1FFFFFFFFull;
    const int count = preset <= 16 ? 16 : 17;
    for (int i = 0; i < 17; ++i) {
        if (count == 16 && coco[i] == 0) continue; // 16 点不含鼻
        mask |= (1ull << coco[i]);
    }
    return mask;
}

QString angleName(JointAngle j)
{
    const int i = angleIndex(j);
    if (i < 0 || i >= kAngleCount) return QStringLiteral("-");
    return QString::fromUtf8(kAngleNames[i]);
}

QString angleNameShort(JointAngle j)
{
    return angleName(j) + QStringLiteral("角");
}

QString angleUnit(JointAngle) { return QStringLiteral("°"); }

QVector<JointAngle> hudAngleOrder()
{
    return {JointAngle::LeftKnee, JointAngle::RightKnee, JointAngle::LeftHip,
            JointAngle::RightHip, JointAngle::Trunk,     JointAngle::LeftElbow,
            JointAngle::RightElbow, JointAngle::Neck};
}

QString deviceProtocolName(DeviceProtocol p)
{
    switch (p) {
    case DeviceProtocol::WebRTC: return QStringLiteral("WebRTC");
    case DeviceProtocol::RTSP:   return QStringLiteral("RTSP");
    case DeviceProtocol::RTMP:   return QStringLiteral("RTMP");
    case DeviceProtocol::USB:    return QStringLiteral("USB");
    }
    return QStringLiteral("WebRTC");
}

DeviceProtocol deviceProtocolFromName(const QString &name)
{
    if (name.compare(QStringLiteral("RTSP"), Qt::CaseInsensitive) == 0) return DeviceProtocol::RTSP;
    if (name.compare(QStringLiteral("RTMP"), Qt::CaseInsensitive) == 0) return DeviceProtocol::RTMP;
    if (name.compare(QStringLiteral("USB"), Qt::CaseInsensitive) == 0) return DeviceProtocol::USB;
    return DeviceProtocol::WebRTC;
}

// ---------------------------------------------------------------------------
QJsonObject CaptureConfig::toJson() const
{
    QJsonObject o;
    o["resolution"] = resolution;
    o["fps"] = fps;
    o["protocol"] = deviceProtocolName(protocol);
    o["codec"] = codec;
    o["weakNetworkDegrade"] = weakNetworkDegrade;
    o["mirror"] = mirror;
    return o;
}

CaptureConfig CaptureConfig::fromJson(const QJsonObject &obj)
{
    CaptureConfig c;
    c.resolution = obj.value("resolution").toString(c.resolution);
    c.fps = obj.value("fps").toInt(c.fps);
    c.protocol = deviceProtocolFromName(obj.value("protocol").toString());
    c.codec = obj.value("codec").toString(c.codec);
    c.weakNetworkDegrade = obj.value("weakNetworkDegrade").toBool(c.weakNetworkDegrade);
    c.mirror = obj.value("mirror").toBool(c.mirror);
    return c;
}

// ---------------------------------------------------------------------------
QString poseModelName(PoseModel model)
{
    switch (model) {
    case PoseModel::BlazePose: return QStringLiteral("BlazePose");
    case PoseModel::MoveNet:   return QStringLiteral("MoveNet");
    case PoseModel::HRNet:     return QStringLiteral("HRNet");
    }
    return QStringLiteral("MoveNet");
}

QString poseModelLabel(PoseModel model)
{
    switch (model) {
    case PoseModel::BlazePose: return QStringLiteral("BlazePose · Full");
    case PoseModel::MoveNet:   return QStringLiteral("MoveNet · Thunder");
    case PoseModel::HRNet:     return QStringLiteral("HRNet · W48");
    }
    return QString();
}

QString poseModelDescription(PoseModel model)
{
    switch (model) {
    case PoseModel::BlazePose:
        return QStringLiteral("33 关键点 · 3D 输出 · 中等算力，适合全身评估");
    case PoseModel::MoveNet:
        return QStringLiteral("17 关键点 · 轻量高速 · 默认模型，实时 30fps+");
    case PoseModel::HRNet:
        return QStringLiteral("17 关键点 · 高精度重模型 · 离线复核优先");
    }
    return QString();
}

PoseModel poseModelFromName(const QString &name)
{
    if (name.compare(QStringLiteral("BlazePose"), Qt::CaseInsensitive) == 0) return PoseModel::BlazePose;
    if (name.compare(QStringLiteral("HRNet"), Qt::CaseInsensitive) == 0) return PoseModel::HRNet;
    return PoseModel::MoveNet;
}

QVector<PoseModel> allPoseModels()
{
    return {PoseModel::MoveNet, PoseModel::BlazePose, PoseModel::HRNet};
}

int PoseConfig::enabledKeypointCount() const
{
    int n = 0;
    for (int i = 0; i < kKeypointCount; ++i)
        if (keypointEnabled(i)) ++n;
    return n;
}

PoseConfig PoseConfig::defaults()
{
    PoseConfig c;
    c.model = PoseModel::MoveNet;
    c.keypointPreset = 17;
    c.keypointMask = presetKeypointMask(17);
    c.confidenceThreshold = 0.55f;
    c.nmsThreshold = 0.40f;
    c.smoothingAlpha = 0.35f;
    c.interpolateMissing = true;
    c.enableNose = true;
    c.useGpu = true;
    c.threads = 4;
    c.inputWidth = 256;
    c.inputHeight = 256;
    return c;
}

QJsonObject PoseConfig::toJson() const
{
    QJsonObject o;
    o["model"] = poseModelName(model);
    o["keypointPreset"] = keypointPreset;
    o["confidenceThreshold"] = confidenceThreshold;
    o["nmsThreshold"] = nmsThreshold;
    o["smoothingAlpha"] = smoothingAlpha;
    o["interpolateMissing"] = interpolateMissing;
    o["enableNose"] = enableNose;
    o["useGpu"] = useGpu;
    o["threads"] = threads;
    o["inputWidth"] = inputWidth;
    o["inputHeight"] = inputHeight;
    o["keypointMask"] = double(keypointMask);
    return o;
}

PoseConfig PoseConfig::fromJson(const QJsonObject &obj)
{
    PoseConfig c = PoseConfig::defaults();
    c.model = poseModelFromName(obj.value("model").toString());
    c.keypointPreset = obj.value("keypointPreset").toInt(c.keypointPreset);
    c.confidenceThreshold = float(obj.value("confidenceThreshold").toDouble(c.confidenceThreshold));
    c.nmsThreshold = float(obj.value("nmsThreshold").toDouble(c.nmsThreshold));
    c.smoothingAlpha = float(obj.value("smoothingAlpha").toDouble(c.smoothingAlpha));
    c.interpolateMissing = obj.value("interpolateMissing").toBool(c.interpolateMissing);
    c.enableNose = obj.value("enableNose").toBool(c.enableNose);
    c.useGpu = obj.value("useGpu").toBool(c.useGpu);
    c.threads = obj.value("threads").toInt(c.threads);
    c.inputWidth = obj.value("inputWidth").toInt(c.inputWidth);
    c.inputHeight = obj.value("inputHeight").toInt(c.inputHeight);
    const quint64 mask = quint64(obj.value("keypointMask").toDouble(double(c.keypointMask)));
    c.keypointMask = mask;
    if (!c.enableNose) c.keypointMask &= ~(1ull << kpNose);
    return c;
}

// ---------------------------------------------------------------------------
QString reportFormatName(ReportFormat f)
{
    switch (f) {
    case ReportFormat::Csv:      return QStringLiteral("CSV 数据");
    case ReportFormat::Html:     return QStringLiteral("HTML 报告");
    case ReportFormat::Markdown: return QStringLiteral("Markdown 报告");
    case ReportFormat::Pdf:      return QStringLiteral("PDF 报告");
    }
    return QString();
}

QString reportFormatSuffix(ReportFormat f)
{
    switch (f) {
    case ReportFormat::Csv:      return QStringLiteral("csv");
    case ReportFormat::Html:     return QStringLiteral("html");
    case ReportFormat::Markdown: return QStringLiteral("md");
    case ReportFormat::Pdf:      return QStringLiteral("pdf");
    }
    return QStringLiteral("txt");
}

// ---------------------------------------------------------------------------
QJsonObject AppSettings::toJson() const
{
    QJsonObject o;
    o["language"] = language;
    o["startupPage"] = startupPage;
    o["autoConnect"] = autoConnect;
    o["showFpsOverlay"] = showFpsOverlay;
    o["savePath"] = savePath;
    o["autoRecord"] = autoRecord;
    o["keepOriginalVideo"] = keepOriginalVideo;
    o["lastStreamUrl"] = lastStreamUrl;
    o["gpuAcceleration"] = gpuAcceleration;
    o["inferenceThreads"] = inferenceThreads;
    o["cacheModel"] = cacheModel;
    o["maxFps"] = maxFps;
    o["retentionDays"] = retentionDays;
    o["autoCleanup"] = autoCleanup;
    o["anonymousStats"] = anonymousStats;
    o["version"] = version;
    return o;
}

AppSettings AppSettings::fromJson(const QJsonObject &obj)
{
    AppSettings s;
    s.language = obj.value("language").toString(s.language);
    s.startupPage = obj.value("startupPage").toInt(s.startupPage);
    s.autoConnect = obj.value("autoConnect").toBool(s.autoConnect);
    s.showFpsOverlay = obj.value("showFpsOverlay").toBool(s.showFpsOverlay);
    s.savePath = obj.value("savePath").toString(s.savePath);
    s.autoRecord = obj.value("autoRecord").toBool(s.autoRecord);
    s.keepOriginalVideo = obj.value("keepOriginalVideo").toBool(s.keepOriginalVideo);
    s.lastStreamUrl = obj.value("lastStreamUrl").toString(s.lastStreamUrl);
    s.gpuAcceleration = obj.value("gpuAcceleration").toBool(s.gpuAcceleration);
    s.inferenceThreads = obj.value("inferenceThreads").toInt(s.inferenceThreads);
    s.cacheModel = obj.value("cacheModel").toBool(s.cacheModel);
    s.maxFps = obj.value("maxFps").toInt(s.maxFps);
    s.retentionDays = obj.value("retentionDays").toInt(s.retentionDays);
    s.autoCleanup = obj.value("autoCleanup").toBool(s.autoCleanup);
    s.anonymousStats = obj.value("anonymousStats").toBool(s.anonymousStats);
    s.version = obj.value("version").toString(s.version);
    return s;
}

// ---------------------------------------------------------------------------
Keypoint midpoint(const Keypoint &a, const Keypoint &b)
{
    Keypoint k;
    if (!a.valid || !b.valid) return k;
    k.x = (a.x + b.x) * 0.5f;
    k.y = (a.y + b.y) * 0.5f;
    k.confidence = qMin(a.confidence, b.confidence);
    k.valid = true;
    return k;
}

float angleBetween(const Keypoint &a, const Keypoint &b, const Keypoint &c)
{
    const float v1x = a.x - b.x;
    const float v1y = a.y - b.y;
    const float v2x = c.x - b.x;
    const float v2y = c.y - b.y;
    const float n1 = std::sqrt(v1x * v1x + v1y * v1y);
    const float n2 = std::sqrt(v2x * v2x + v2y * v2y);
    if (n1 < 1e-6f || n2 < 1e-6f) return 0.0f;
    float cosv = (v1x * v2x + v1y * v2y) / (n1 * n2);
    cosv = clampf(cosv, -1.0f, 1.0f);
    return qRadiansToDegrees(std::acos(cosv));
}

float angleFromVertical(const Keypoint &upper, const Keypoint &lower)
{
    const float dx = upper.x - lower.x;
    const float dy = upper.y - lower.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-6f) return 0.0f;
    const float cosv = clampf(std::fabs(dy) / len, -1.0f, 1.0f);
    return qRadiansToDegrees(std::acos(cosv));
}

QString formatDuration(qint64 ms, bool withMillis)
{
    if (ms < 0) ms = 0;
    const qint64 totalSec = ms / 1000;
    const qint64 mm = totalSec / 60;
    const qint64 ss = totalSec % 60;
    const qint64 mmm = ms % 1000;
    QString s = QStringLiteral("%1:%2").arg(mm, 2, 10, QLatin1Char('0')).arg(ss, 2, 10, QLatin1Char('0'));
    if (withMillis) s += QStringLiteral(".%1").arg(mmm, 3, 10, QLatin1Char('0'));
    return s;
}

QString formatDate(const QDateTime &dt)
{
    if (!dt.isValid()) return QStringLiteral("-");
    return dt.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

} // namespace core
