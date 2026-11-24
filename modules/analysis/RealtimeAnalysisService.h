#pragma once
#include "IRealtimeAnalysisService.h"
#include <cmath>
namespace motion {
// One instance per live stream, owned by one thread. O(1) time and memory per frame.
class RealtimeAnalysisService final : public IRealtimeAnalysisService {
    RealtimeAnalysisConfig config_;
    enum class Phase { Unarmed, Standing, Moving };
    Phase phase_ = Phase::Unarmed;
    bool hasSession_ = false, bottomSeen_ = false;
    std::string sessionId_;
    std::int64_t lastMs_ = -1, lastFrame_ = -1, startMs_ = 0;
    double lastAngle_ = 0;
    std::uint64_t count_ = 0;
    RealtimeHintCode direction_ = RealtimeHintCode::Descending;
    RealtimeAnalysisResult current_;
    static bool usable(const RealtimeAngle& a, double max = 180) {
        return a.valid && std::isfinite(a.degrees) && a.degrees >= 0 && a.degrees <= max;
    }
    bool configValid() const {
        return std::isfinite(config_.standingDeg) && std::isfinite(config_.bottomDeg) &&
            config_.bottomDeg >= 0 && config_.standingDeg <= 180 &&
            config_.bottomDeg < config_.standingDeg && config_.maxGapMs > 0 &&
            config_.minCycleMs >= 0 && std::isfinite(config_.directionDeltaDeg) &&
            config_.directionDeltaDeg > 0 && config_.directionDeltaDeg <= 180 &&
            (!config_.enableTrunkHint || (std::isfinite(config_.trunkLimitDeg) &&
             config_.trunkLimitDeg > 0 && config_.trunkLimitDeg <= 90));
    }
    void interrupt() { phase_ = Phase::Unarmed; bottomSeen_ = false; }
    RealtimeAnalysisResult output(RealtimeHintCode code, const char* message,
                                  RealtimeStatus status = RealtimeStatus::Ok) {
        RealtimeAnalysisResult r;
        r.code = code; r.message = message; r.sessionId = sessionId_;
        r.completedReps = count_; r.repCompleted = code == RealtimeHintCode::RepCompleted;
        r.result.status = status; r.result.message = message;
        current_ = r; return r;
    }
public:
    explicit RealtimeAnalysisService(const RealtimeAnalysisConfig& config = {}) : config_(config) {}
    void reset() override {
        interrupt(); hasSession_ = false; sessionId_.clear(); count_ = 0;
        lastMs_ = -1; lastFrame_ = -1; startMs_ = 0; lastAngle_ = 0;
        direction_ = RealtimeHintCode::Descending; current_ = {};
    }
    RealtimeAnalysisResult processPose(const RealtimePoseInput& pose) override {
        if (!configValid()) return output(RealtimeHintCode::InvalidInput,
            "提示参数无效", RealtimeStatus::InvalidArgument);
        // Reject malformed first frames without assigning a new session.
        if (pose.timestampMs < 0 || pose.frameId < -1) {
            interrupt(); return output(RealtimeHintCode::InvalidInput,
                "帧时间或编号无效", RealtimeStatus::InvalidArgument);
        }
        if (!hasSession_ || pose.sessionId != sessionId_) {
            reset(); hasSession_ = true; sessionId_ = pose.sessionId;
        }
        if (pose.timestampMs <= lastMs_ ||
            (pose.frameId >= 0 && lastFrame_ >= 0 && pose.frameId <= lastFrame_)) {
            interrupt(); return output(RealtimeHintCode::InvalidInput,
                "收到重复或倒序帧，请重新站立", RealtimeStatus::InvalidArgument);
        }
        if (lastMs_ >= 0 && pose.timestampMs-lastMs_ > config_.maxGapMs) interrupt();
        lastMs_ = pose.timestampMs;
        // Keep last known ID even when an intermediate frame has no frame identity.
        if (pose.frameId >= 0) lastFrame_ = pose.frameId;
        if (!usable(pose.leftKnee)) {
            interrupt(); return output(RealtimeHintCode::DataLost,
                "左膝数据不可用，请回到检测范围", RealtimeStatus::NoData);
        }
        const double angle = pose.leftKnee.degrees;
        RealtimeHintCode code = RealtimeHintCode::WaitingForStanding;
        const char* message = "请先站立，准备开始";
        if (phase_ == Phase::Unarmed) {
            if (angle >= config_.standingDeg) {
                phase_ = Phase::Standing; startMs_ = pose.timestampMs;
                code = RealtimeHintCode::Ready; message = "已就绪，可以开始下蹲";
            }
        } else if (phase_ == Phase::Standing) {
            if (angle >= config_.standingDeg) {
                startMs_ = pose.timestampMs;
                code = RealtimeHintCode::Ready; message = "已就绪，可以开始下蹲";
            } else {
                phase_ = Phase::Moving; bottomSeen_ = angle <= config_.bottomDeg;
                direction_ = RealtimeHintCode::Descending;
                code = bottomSeen_ ? RealtimeHintCode::Bottom : direction_;
                message = bottomSeen_ ? "已到下蹲阈值，可以起身" : "正在下蹲";
            }
        } else if (angle >= config_.standingDeg) {
            if (bottomSeen_ && pose.timestampMs-startMs_ >= config_.minCycleMs) {
                ++count_; code = RealtimeHintCode::RepCompleted; message = "已完成一次";
            } else {
                code = RealtimeHintCode::IncompleteRep; message = "本次未满足计次条件，请重新开始";
            }
            phase_ = Phase::Standing; bottomSeen_ = false; startMs_ = pose.timestampMs;
        } else if (angle <= config_.bottomDeg) {
            bottomSeen_ = true; code = RealtimeHintCode::Bottom;
            message = "已到下蹲阈值，可以起身";
        } else {
            const double delta = angle-lastAngle_;
            if (delta >= config_.directionDeltaDeg) direction_ = RealtimeHintCode::Rising;
            else if (delta <= -config_.directionDeltaDeg) direction_ = RealtimeHintCode::Descending;
            code = direction_;
            message = code == RealtimeHintCode::Rising ? "正在起身" : "正在下蹲";
        }
        lastAngle_ = angle;
        auto r = output(code, message);
        if (config_.enableTrunkHint && usable(pose.trunk, 90)) {
            r.trunkHintAvailable = true; r.forwardLean = pose.trunk.degrees > config_.trunkLimitDeg;
        }
        current_ = r; return r;
    }
    RealtimeAnalysisResult poll(std::int64_t nowMs) override {
        if (!configValid() || nowMs < 0 || (lastMs_ >= 0 && nowMs < lastMs_))
            return output(RealtimeHintCode::InvalidInput, "检查时间或参数无效", RealtimeStatus::InvalidArgument);
        if (lastMs_ < 0) return output(RealtimeHintCode::WaitingForStanding, "等待姿态数据", RealtimeStatus::NoData);
        if (nowMs-lastMs_ > config_.maxGapMs) {
            interrupt(); return output(RealtimeHintCode::DataLost,
                "姿态数据已中断，恢复后请重新站立", RealtimeStatus::NoData);
        }
        auto r = current_; r.repCompleted = false; return r;
    }
};
}
