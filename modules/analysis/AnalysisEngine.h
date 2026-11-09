#pragma once
#include "IAnalysisService.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace motion {
// Experimental thresholds are engineering defaults, NOT approved scoring standards.
struct AnalysisConfig {
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
        for (const auto& p : poses) {
            CurvePoint point; point.timestampMs = p.timestampMs;
            point.leftKnee = p.leftKnee; point.rightKnee = p.rightKnee;
            point.leftKnee.valid = usable(p.leftKnee); point.rightKnee.valid = usable(p.rightKnee);
            out.metrics.kneeCurve.push_back(point);
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
        // Scoring is deliberately unavailable until team approves rules.
        m.qualityScoreAvailable = false;
        m.symmetryAvailable = false;
        m.grade = "UNSCORED"; m.ruleVersion = "M1-left-knee-v0.1";
        out.result.message = "Measured left-knee metrics; scoring and symmetry unavailable";
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
