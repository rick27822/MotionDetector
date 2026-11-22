#include "PoseSampleReader.h"
#include <QFile>
#include <QJsonDocument>
#include <QSet>
#include <cmath>
namespace ui {
static const QStringList ascii = {"leftKnee",      "rightKnee", "leftHip",    "rightHip",
                                  "trunk",         "leftElbow", "rightElbow", "leftShoulder",
                                  "rightShoulder", "neck",      "leftAnkle",  "rightAnkle"};
bool PoseSampleReader::parseAngles(const QJsonObject &v, quint32 mask,
                                   std::array<float, core::kAngleCount> &out, QString &error) {
    if (mask & ~core::kAllAngleBits) {
        error = "角度掩码包含未知通道";
        return false;
    }
    for (int i = 0; i < core::kAngleCount; i++) {
        QString cn = core::angleName(static_cast<core::JointAngle>(i));
        auto a = v.value(ascii[i]), b = v.value(cn);
        if (!a.isUndefined() && !b.isUndefined() && a != b) {
            error = "中英文角度键冲突：" + ascii[i];
            return false;
        }
        auto value = a.isUndefined() ? b : a;
        bool valid = (mask & (1u << i)) != 0;
        if (valid && (!value.isDouble() || !std::isfinite(value.toDouble()) ||
                      value.toDouble() < 0 || value.toDouble() > 180)) {
            error = "有效角度缺失或越界：" + ascii[i];
            return false;
        }
        out[i] = valid ? float(value.toDouble()) : 0;
    }
    return true;
}
bool PoseSampleReader::readPoses(const QString &path, QVector<core::PoseResult> &out,
                                 QString &error) {
    QFile f(path);
    if (!f.open(QFile::ReadOnly)) {
        error = f.errorString();
        return false;
    }
    QJsonParseError pe;
    auto doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError) {
        error = pe.errorString();
        return false;
    }
    QVector<core::PoseResult> parsed;
    auto frames = doc.object().value("frames").toArray();
    if (frames.isEmpty()) {
        error = "姿态样例没有 frames";
        return false;
    }
    for (auto v : frames) {
        auto o = v.toObject();
        core::PoseResult r;
        r.timestampMs = o.value("timestampMs").toVariant().toLongLong();
        r.frameId = o.value("frameId").toInt();
        r.validAngles = quint32(o.value("validAngleMask").toDouble());
        r.detected = o.value("detected").toBool();
        r.keypointCount = o.value("keypointCount").toInt(33);
        r.modelName = o.value("modelName").toString();
        r.inferenceMs = float(o.value("inferenceMs").toDouble());
        if (r.timestampMs < 0 ||
            (!parsed.isEmpty() && r.timestampMs <= parsed.last().timestampMs)) {
            error = "时间戳必须严格递增";
            return false;
        }
        if (!parseAngles(o.value("angles").toObject(), r.validAngles, r.angles, error))
            return false;
        r.keypoints.resize(33);
        QSet<int> indices;
        for (auto k : o.value("keypoints").toArray()) {
            auto p = k.toObject();
            int i = p.value("index").toInt(-1);
            if (i < 0 || i >= 33 || indices.contains(i)) {
                error = "关键点索引无效或重复";
                return false;
            }
            indices.insert(i);
            auto &kp = r.keypoints[i];
            kp.x = float(p.value("x").toDouble());
            kp.y = float(p.value("y").toDouble());
            kp.confidence = float(p.value("confidence").toDouble());
            kp.valid = p.value("valid").toBool();
            if (kp.valid && (!std::isfinite(kp.x) || !std::isfinite(kp.y) || kp.x < 0 || kp.x > 1 ||
                             kp.y < 0 || kp.y > 1 || kp.confidence < 0 || kp.confidence > 1)) {
                error = "关键点坐标或置信度越界";
                return false;
            }
        }
        parsed.append(r);
    }
    out = parsed;
    error.clear();
    return true;
}
bool PoseSampleReader::readSeries(const QString &path, QVector<core::AngleSample> &out,
                                  QString &error) {
    QFile f(path);
    if (!f.open(QFile::ReadOnly)) {
        error = f.errorString();
        return false;
    }
    QVector<core::AngleSample> parsed;
    int line = 0;
    while (!f.atEnd()) {
        auto data = f.readLine();
        ++line;
        if (data.trimmed().isEmpty())
            continue;
        QJsonParseError pe;
        auto doc = QJsonDocument::fromJson(data, &pe);
        if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
            error = QString("第%1行JSON无效").arg(line);
            return false;
        }
        auto o = doc.object();
        core::AngleSample s;
        s.t = o.value("t").toVariant().toLongLong();
        s.valid = quint32(o.value("validMask").toDouble());
        if (s.t < 0 || (!parsed.isEmpty() && s.t <= parsed.last().t)) {
            error = "时间戳必须严格递增";
            return false;
        }
        if (!parseAngles(o.value("deg").toObject(), s.valid, s.deg, error))
            return false;
        parsed.append(s);
    }
    if (parsed.isEmpty()) {
        error = "角度序列为空";
        return false;
    }
    out = parsed;
    error.clear();
    return true;
}
} // namespace ui
