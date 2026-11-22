// core/types.h — 公共类型定义（四人共担，先行冻结）
// 变更需组会确认；本文件不依赖任何模块，只依赖 Qt Core / Gui。
#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>
#include <array>
#include <cstdint>

namespace core {

// ---------------------------------------------------------------------------
// 常量
// ---------------------------------------------------------------------------
constexpr int kKeypointCount = 33;   // BlazePose 33 关键点（MoveNet/HRNet 映射到此索引）
constexpr int kAngleCount = 12;      // 关节角度通道数

// ---------------------------------------------------------------------------
// 关键点索引（BlazePose 33）
// ---------------------------------------------------------------------------
enum KeypointIndex : int {
    kpNose = 0,
    kpLeftEyeInner = 1,
    kpLeftEye = 2,
    kpLeftEyeOuter = 3,
    kpRightEyeInner = 4,
    kpRightEye = 5,
    kpRightEyeOuter = 6,
    kpLeftEar = 7,
    kpRightEar = 8,
    kpMouthLeft = 9,
    kpRightMouth = 10,
    kpLeftShoulder = 11,
    kpRightShoulder = 12,
    kpLeftElbow = 13,
    kpRightElbow = 14,
    kpLeftWrist = 15,
    kpRightWrist = 16,
    kpLeftPinky = 17,
    kpRightPinky = 18,
    kpLeftIndex = 19,
    kpRightIndex = 20,
    kpLeftThumb = 21,
    kpRightThumb = 22,
    kpLeftHip = 23,
    kpRightHip = 24,
    kpLeftKnee = 25,
    kpRightKnee = 26,
    kpLeftAnkle = 27,
    kpRightAnkle = 28,
    kpLeftHeel = 29,
    kpRightHeel = 30,
    kpLeftFootIndex = 31,
    kpRightFootIndex = 32
};

/// 关键点显示名（中文，用于骨架标签与配置页开关组）。
QString keypointName(int index);

/// 关键点分组（配置页「关键点开关组」按组折叠）。
QStringList keypointGroupNames();
QVector<int> keypointGroup(int groupIndex);

/// 预设（16 / 17 / 33）对应的关键点启用掩码。
quint64 presetKeypointMask(int preset);

// ---------------------------------------------------------------------------
// 关节角度通道
// ---------------------------------------------------------------------------
enum class JointAngle : int {
    LeftKnee = 0,
    RightKnee = 1,
    LeftHip = 2,
    RightHip = 3,
    Trunk = 4,
    LeftElbow = 5,
    RightElbow = 6,
    LeftShoulder = 7,
    RightShoulder = 8,
    Neck = 9,
    LeftAnkle = 10,
    RightAnkle = 11
};

constexpr int angleIndex(JointAngle j) { return static_cast<int>(j); }
constexpr quint32 angleBit(JointAngle j) { return 1u << angleIndex(j); }
constexpr quint32 kAllAngleBits = 0x0FFFu;

QString angleName(JointAngle j);          // 例如「左膝」
QString angleNameShort(JointAngle j);     // 例如「左膝角」
QString angleUnit(JointAngle j);          // 「°」

/// P1 右侧「关节角度实时卡」的显示顺序。
QVector<JointAngle> hudAngleOrder();

// ---------------------------------------------------------------------------
// 关键点 / 姿态结果
// ---------------------------------------------------------------------------
struct Keypoint {
    float x = 0.0f;           // 归一化 [0,1]，左→右
    float y = 0.0f;           // 归一化 [0,1]，上→下
    float confidence = 0.0f;  // [0,1]
    bool valid = false;
};

/// 单帧姿态结果。IPoseEstimator 的输出契约（张在庆 → 陈柯睿）。
/// 角度由 PoseModule 计算，UI 只做像素换算后绘制，保证 P1 HUD 与 P5 曲线同源。
struct PoseResult {
    qint64 timestampMs = 0;
    int frameId = 0;
    bool detected = false;
    int keypointCount = kKeypointCount;   // 当前预设：16/17/33
    QVector<Keypoint> keypoints;          // 固定 kKeypointCount 个，未启用者 valid=false
    std::array<float, kAngleCount> angles{};
    quint32 validAngles = 0;
    float inferenceMs = 0.0f;
    QString modelName;

    bool hasAngle(JointAngle j) const { return (validAngles & angleBit(j)) != 0; }
    float angle(JointAngle j) const { return angles[static_cast<size_t>(angleIndex(j))]; }
    void setAngle(JointAngle j, float value) {
        angles[static_cast<size_t>(angleIndex(j))] = value;
        validAngles |= angleBit(j);
    }
    const Keypoint &kp(int index) const {
        static const Keypoint invalid;
        if (index < 0 || index >= keypoints.size()) return invalid;
        return keypoints.at(index);
    }
    bool hasKeypoint(int index) const {
        return index >= 0 && index < keypoints.size() && keypoints.at(index).valid;
    }
};

/// 平滑后的一帧角度采样，IDataManager 持久化、IAnalysisService 消费。
struct AngleSample {
    qint64 t = 0;                                  // 距会话开始的毫秒
    std::array<float, kAngleCount> deg{};
    quint32 valid = 0;

    bool hasAngle(JointAngle j) const { return (valid & angleBit(j)) != 0; }
    float angle(JointAngle j) const { return deg[static_cast<size_t>(angleIndex(j))]; }
};

/// 一次完整下蹲周期。
struct RepCycle {
    int index = 0;
    qint64 startMs = 0;
    qint64 endMs = 0;
    float minAngle = 0.0f;       // 谷值（蹲底膝角）
    float maxAngle = 0.0f;       // 峰值（站直膝角）
    float peakVelocity = 0.0f;   // 峰值角速度 °/s
    float depthRatio = 0.0f;     // 0~1 蹲深归一化
};

// ---------------------------------------------------------------------------
// 设备 / 采集
// ---------------------------------------------------------------------------
enum class DeviceProtocol { WebRTC, RTSP, RTMP, USB };

struct DeviceInfo {
    QString id;
    QString name;
    QString model;
    QString ip;
    DeviceProtocol protocol = DeviceProtocol::WebRTC;
    int battery = 100;        // %
    float rttMs = 0.0f;
    int signal = 4;           // 1~4 格
    bool connected = false;
    bool paired = true;
};

QString deviceProtocolName(DeviceProtocol p);
DeviceProtocol deviceProtocolFromName(const QString &name);

struct CaptureConfig {
    QString resolution = QStringLiteral("1280x720");
    int fps = 30;
    DeviceProtocol protocol = DeviceProtocol::WebRTC;
    QString codec = QStringLiteral("H.264");
    bool weakNetworkDegrade = true;
    bool mirror = true;

    QJsonObject toJson() const;
    static CaptureConfig fromJson(const QJsonObject &obj);
};

struct FrameStats {
    double fps = 0.0;
    double latencyMs = 0.0;
    int droppedFrames = 0;
    int width = 0;
    int height = 0;
    quint64 framesProduced = 0;
};

struct PoseStats {
    double fps = 0.0;
    float avgInferenceMs = 0.0f;
    int droppedFrames = 0;
    int queueDepth = 0;
};

// ---------------------------------------------------------------------------
// 模型 / 推理配置
// ---------------------------------------------------------------------------
enum class PoseModel { BlazePose, MoveNet, HRNet };

QString poseModelName(PoseModel model);
QString poseModelLabel(PoseModel model);       // 展示用「BlazePose (Heavy)」
QString poseModelDescription(PoseModel model); // 参数/精度简介
PoseModel poseModelFromName(const QString &name);
QVector<PoseModel> allPoseModels();

/// P3 检测配置页的下发配置（陈柯睿 → 张在庆）。
struct PoseConfig {
    PoseModel model = PoseModel::MoveNet;
    int keypointPreset = 17;            // 16 / 17 / 33
    float confidenceThreshold = 0.55f;
    float nmsThreshold = 0.40f;
    float smoothingAlpha = 0.35f;       // EMA 系数，越小越平滑
    bool interpolateMissing = true;     // 低置信度插值补全
    bool enableNose = true;             // 「鼻未启用」开关，联动颈部/躯干角度
    bool useGpu = true;
    int threads = 4;
    int inputWidth = 256;
    int inputHeight = 256;
    quint64 keypointMask = 0x1FFFFFFFFull; // 每关键点启用位

    int enabledKeypointCount() const;
    bool keypointEnabled(int index) const { return (keypointMask >> index) & 1ull; }

    QJsonObject toJson() const;
    static PoseConfig fromJson(const QJsonObject &obj);
    /// 与 P3 控件联动的默认值（「恢复默认」按钮）。
    static PoseConfig defaults();
};

// ---------------------------------------------------------------------------
// 分析 / 会话
// ---------------------------------------------------------------------------
struct SessionMetrics {
    int repCount = 0;
    float maxKneeAngle = 0.0f;         // 峰值膝角（站直）
    float minKneeAngle = 0.0f;         // 谷值膝角（蹲底）
    float rom = 0.0f;                  // 活动度 = max - min
    float peakAngularVelocity = 0.0f;  // °/s
    float symmetry = 0.0f;             // 左右对称性 0~100
    float stability = 0.0f;            // 躯干稳定性 0~100
    float tempoSeconds = 0.0f;         // 平均单次周期
    float score = 0.0f;                // 综合质量分 0~10
    QString grade;                     // 优秀/良好/合格/待改进

    bool valid() const { return repCount > 0; }
};

struct SessionRecord {
    qint64 id = -1;
    QString name;
    QDateTime startedAt;
    int durationMs = 0;
    QString model;
    QString videoPath;
    int keypointCount = 33;
    SessionMetrics metrics;
};

enum class ReportFormat { Csv, Html, Markdown, Pdf };

QString reportFormatName(ReportFormat f);
QString reportFormatSuffix(ReportFormat f);

// ---------------------------------------------------------------------------
// 应用设置（P6 五块设置区）
// ---------------------------------------------------------------------------
struct AppSettings {
    // 常规
    QString language = QStringLiteral("简体中文");
    int startupPage = 0;                 // 启动页索引
    bool autoConnect = true;
    bool showFpsOverlay = true;
    // 采集与录制
    QString savePath;                    // 空 = 默认记录目录
    bool autoRecord = false;
    bool keepOriginalVideo = true;
    QString lastStreamUrl;               // 上次使用的手机推流地址（RTSP / HTTP MJPEG）
    // 推理与性能
    bool gpuAcceleration = true;
    int inferenceThreads = 4;
    bool cacheModel = true;
    int maxFps = 30;
    // 数据与隐私
    int retentionDays = 30;
    bool autoCleanup = false;
    bool anonymousStats = false;
    // 关于
    QString version = QStringLiteral("1.0.0");

    QJsonObject toJson() const;
    static AppSettings fromJson(const QJsonObject &obj);
};

// ---------------------------------------------------------------------------
// 工具函数
// ---------------------------------------------------------------------------
/// 中位点（两端点均有效时返回有效结果）。
Keypoint midpoint(const Keypoint &a, const Keypoint &b);
/// 三点夹角（b 为顶点），单位度，范围 0~180。
float angleBetween(const Keypoint &a, const Keypoint &b, const Keypoint &c);
/// 与竖直方向的夹角（0~90），用于躯干。
float angleFromVertical(const Keypoint &upper, const Keypoint &lower);
/// 数值裁剪
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
/// 由 t 毫秒生成 mm:ss / mm:ss.mmm
QString formatDuration(qint64 ms, bool withMillis = false);
QString formatDate(const QDateTime &dt);

} // namespace core

Q_DECLARE_METATYPE(core::PoseResult)
Q_DECLARE_METATYPE(core::AngleSample)
Q_DECLARE_METATYPE(core::DeviceInfo)
Q_DECLARE_METATYPE(core::FrameStats)
Q_DECLARE_METATYPE(core::PoseStats)
Q_DECLARE_METATYPE(core::CaptureConfig)
Q_DECLARE_METATYPE(core::PoseConfig)
Q_DECLARE_METATYPE(core::SessionRecord)
Q_DECLARE_METATYPE(core::SessionMetrics)
Q_DECLARE_METATYPE(core::AppSettings)
Q_DECLARE_METATYPE(core::RepCycle)
