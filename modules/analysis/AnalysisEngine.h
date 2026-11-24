#pragma once
#include "IAnalysisService.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace motion {
// Experimental thresholds are engineering defaults, NOT approved scoring standards.
// Opt-in engineering rule. Team must approve thresholds before production use.
struct QualityRule {
    bool enabled = false;
    std::string version;
    double targetRomDeg = 90;
    double trunkLimitDeg = 45;
    double romWeight = 0.5, symmetryWeight = 0.3, trunkWeight = 0.2;
    double gradeA = 8, gradeB = 6;
};
struct AnalysisConfig {
    QualityRule quality;
    double standingDeg = 160;
    double bottomDeg = 120;
    std::int64_t maxGapMs = 500;
    std::int64_t minCycleMs = 500;
};
class AnalysisEngine final : public IAnalysisService {
    AnalysisConfig config_;
    static bool usable(const AngleSample& a) {
        return a.valid && std::isfinite(a.degrees) && a.degrees >= 0 && a.degrees <= 180;
    }
    static void fail(AnalysisResponse& r, Status s, const char* text) {
        r.result.status = s; r.result.message = text;
    }
public:
    explicit AnalysisEngine(const AnalysisConfig& c = AnalysisConfig()) : config_(c) {}
    AnalysisResponse analyze(const std::vector<PoseResult>& poses) const override {
        AnalysisResponse out; out.metrics.simulated = false;
        if (!(config_.bottomDeg >= 0 && config_.bottomDeg < config_.standingDeg &&
              config_.standingDeg <= 180) || config_.maxGapMs <= 0 || config_.minCycleMs < 0) {
            fail(out, Status::InvalidArgument, "Invalid cycle configuration"); return out;
        }
        const auto& q = config_.quality;
        if (q.enabled && (q.version.empty() ||
            !std::isfinite(q.targetRomDeg) || q.targetRomDeg <= 0 || q.targetRomDeg > 180 ||
            !std::isfinite(q.trunkLimitDeg) || q.trunkLimitDeg <= 0 || q.trunkLimitDeg > 90 ||
            !std::isfinite(q.romWeight) || q.romWeight <= 0 ||
            !std::isfinite(q.symmetryWeight) || q.symmetryWeight <= 0 ||
            !std::isfinite(q.trunkWeight) || q.trunkWeight <= 0 ||
            !std::isfinite(q.romWeight + q.symmetryWeight + q.trunkWeight) ||
            !std::isfinite(q.gradeA) || !std::isfinite(q.gradeB) ||
            q.gradeB < 0 || q.gradeA <= q.gradeB || q.gradeA > 10)) {
            fail(out, Status::InvalidArgument, "Invalid quality rule"); return out;
        }
        if (poses.empty()) { fail(out, Status::NoData, "Empty sequence"); return out; }
        for (std::size_t i = 0; i < poses.size(); ++i) {
            if (poses[i].sessionId != poses.front().sessionId) {
                fail(out, Status::InvalidArgument, "Mixed sessions are not allowed"); return out;
            }
            if (poses[i].timestampMs < 0 || (i && poses[i].timestampMs <= poses[i-1].timestampMs)) {
                fail(out, Status::InvalidArgument, "Timestamps must be strictly increasing milliseconds");
                return out;
            }
        }
        // M1 primary metric: LEFT knee only. Never mix sides into one curve/derivative.
        bool have = false, previousValid = false;
        double lo = 180, hi = 0, speed = 0, previous = 0;
        std::int64_t previousTime = 0, start = 0;
        enum Phase { Unarmed, Standing, Descending, Bottom };
        Phase phase = Unarmed;
        double cycleLo = 180, cycleHi = 0;
        std::size_t pairedCount = 0;
        double leftLo = 180, leftHi = 0, rightLo = 180, rightHi = 0;
        bool completeCoverage = true;
        double maxTrunk = 0;
        for (const auto& p : poses) {
            CurvePoint point; point.timestampMs = p.timestampMs;
            point.leftKnee = p.leftKnee; point.rightKnee = p.rightKnee;
            point.leftKnee.valid = usable(p.leftKnee); point.rightKnee.valid = usable(p.rightKnee);
            out.metrics.kneeCurve.push_back(point);
            if (point.leftKnee.valid && point.rightKnee.valid) {
                ++pairedCount;
                leftLo = std::min(leftLo, p.leftKnee.degrees);
                leftHi = std::max(leftHi, p.leftKnee.degrees);
                rightLo = std::min(rightLo, p.rightKnee.degrees);
                rightHi = std::max(rightHi, p.rightKnee.degrees);
            }
            const bool trunkUsable = usable(p.trunk) && p.trunk.degrees <= 90;
            completeCoverage = completeCoverage && point.leftKnee.valid && point.rightKnee.valid &&
                trunkUsable && (!previousValid || p.timestampMs-previousTime <= config_.maxGapMs);
            if (trunkUsable) maxTrunk = std::max(maxTrunk, p.trunk.degrees);
            if (!point.leftKnee.valid) { previousValid = false; phase = Unarmed; continue; }
            const double angle = p.leftKnee.degrees;
            have = true; lo = std::min(lo, angle); hi = std::max(hi, angle);
            const bool connected = previousValid && p.timestampMs - previousTime <= config_.maxGapMs;
            if (connected) {
                speed = std::max(speed, std::abs(angle-previous)*1000.0/(p.timestampMs-previousTime));
                out.metrics.angularSpeedAvailable = true;
            }
            if (!connected) phase = Unarmed;
            if (phase == Unarmed) {
                if (angle >= config_.standingDeg) { phase = Standing; start = p.timestampMs; cycleLo = cycleHi = angle; }
            } else if (phase == Standing) {
                if (angle >= config_.standingDeg) { start = p.timestampMs; cycleLo = cycleHi = angle; }
                else {
                    phase = angle <= config_.bottomDeg ? Bottom : Descending;
                    cycleLo = std::min(cycleLo, angle); cycleHi = std::max(cycleHi, angle);
                }
            } else {
                cycleLo = std::min(cycleLo, angle); cycleHi = std::max(cycleHi, angle);
                if (angle <= config_.bottomDeg) phase = Bottom;
                if (angle >= config_.standingDeg) {
                    if (phase == Bottom && p.timestampMs-start >= config_.minCycleMs) {
                        CycleMetrics c; c.startMs = start; c.endMs = p.timestampMs;
                        c.kneeMinDeg = cycleLo; c.kneeMaxDeg = cycleHi; c.kneeRomDeg = cycleHi-cycleLo;
                        out.metrics.cycles.push_back(c);
                    }
                    phase = Standing; start = p.timestampMs; cycleLo = cycleHi = angle;
                }
            }
            previousValid = true; previousTime = p.timestampMs; previous = angle;
        }
        if (!have) { fail(out, Status::NoData, "No usable LEFT knee samples"); return out; }
        auto& m = out.metrics;
        m.valid = true; m.kneeMinDeg = lo; m.kneeMaxDeg = hi; m.kneeRomDeg = hi-lo;
        m.peakAngularSpeedDegPerSec = speed;
        // Descriptive ROM symmetry on matched valid frames, not a posture judgement.
        const double leftRom = leftHi-leftLo, rightRom = rightHi-rightLo;
        if (pairedCount >= 3 && std::max(leftRom, rightRom) > 1e-6) {
            m.symmetryAvailable = true;
            m.symmetryScore = std::min(leftRom, rightRom) / std::max(leftRom, rightRom);
        }
        m.grade = "UNSCORED";
        m.ruleVersion = q.enabled ? q.version : "M1-metrics-v0.2-unscored";
        // Require complete input and a completed cycle before producing any quality score.
        if (q.enabled && completeCoverage && !m.cycles.empty() && m.symmetryAvailable) {
            double meanCycleRom = 0;
            for (const auto& cycle : m.cycles) meanCycleRom += cycle.kneeRomDeg;
            meanCycleRom /= static_cast<double>(m.cycles.size());
            const double romPart = std::clamp(meanCycleRom/q.targetRomDeg, 0.0, 1.0);
            const double trunkPart = std::clamp(1.0-maxTrunk/q.trunkLimitDeg, 0.0, 1.0);
            const double totalWeight = q.romWeight+q.symmetryWeight+q.trunkWeight;
            m.qualityScore = std::clamp(10.0*((q.romWeight/totalWeight)*romPart +
                (q.symmetryWeight/totalWeight)*m.symmetryScore +
                (q.trunkWeight/totalWeight)*trunkPart), 0.0, 10.0);
            m.qualityScoreAvailable = true;
            m.grade = m.qualityScore >= q.gradeA ? "A" : (m.qualityScore >= q.gradeB ? "B" : "C");
        }
        out.result.message = m.qualityScoreAvailable ? "Computed using caller-supplied quality rule" :
            "Measured metrics; quality rule disabled or insufficient complete data";
        return out;
    }
    ActionHint evaluateHint(const PoseResult& p) const override {
        ActionHint h; h.simulated = false;
        h.code = usable(p.leftKnee) ? HintCode::DataAvailable : HintCode::NoPose;
        h.message = usable(p.leftKnee) ? "Left-knee data available; no posture judgement" : "No usable left-knee data";
        return h;
    }
};
}
