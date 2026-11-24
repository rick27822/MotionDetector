#pragma once
#include "IAnalysisService.h"
#include "IDataManager.h"
#include <cmath>
#include <map>
#include <utility>

namespace motion {
inline Result resultOf(Status status, const std::string& message) {
    Result r; r.status = status; r.message = message; return r;
}
inline bool usable(const AngleSample& a) {
    return a.valid && std::isfinite(a.degrees) && a.degrees >= 0 && a.degrees <= 180;
}
class MockAnalysisService final : public IAnalysisService {
public:
    AnalysisResponse analyze(const std::vector<PoseResult>& poses) const override {
        AnalysisResponse out;
        if (poses.empty()) {
            out.result = resultOf(Status::NoData, "No pose samples"); return out;
        }
        bool anyValid = false;
        for (std::size_t i = 0; i < poses.size(); ++i) {
            if (poses[i].sessionId != poses.front().sessionId) {
                out.result = resultOf(Status::InvalidArgument, "Mixed sessions are not allowed"); return out;
            }
            if (poses[i].timestampMs < 0 ||
                (i && poses[i].timestampMs <= poses[i-1].timestampMs)) {
                out.result = resultOf(Status::InvalidArgument,
                                      "Timestamps must be nonnegative and strictly increasing");
                return out;
            }
            anyValid = anyValid || usable(poses[i].leftKnee) || usable(poses[i].rightKnee);
        }
        if (!anyValid) {
            out.result = resultOf(Status::NoData, "No valid knee samples"); return out;
        }
        auto& m = out.metrics;
        m.valid = true; m.simulated = true;
        m.kneeMinDeg = 90; m.kneeMaxDeg = 170; m.kneeRomDeg = 80;
        m.peakAngularSpeedDegPerSec = 60;
        m.symmetryScore = 0.95; m.qualityScore = 8;
        m.angularSpeedAvailable = m.symmetryAvailable = m.qualityScoreAvailable = true;
        m.grade = "DEMO_GOOD"; m.ruleVersion = "MOCK-0.1";
        // Preserve input timestamps and validity so UI can exercise real curve plumbing.
        for (const auto& p : poses) {
            CurvePoint c; c.timestampMs = p.timestampMs;
            c.leftKnee = p.leftKnee; c.rightKnee = p.rightKnee;
            if (!usable(c.leftKnee)) c.leftKnee.valid = false;
            if (!usable(c.rightKnee)) c.rightKnee.valid = false;
            m.kneeCurve.push_back(c);
        }
        out.result = resultOf(Status::Ok, "MOCK: fixed metrics; not a real analysis");
        return out;
    }
    ActionHint evaluateHint(const PoseResult& pose) const override {
        ActionHint h;
        if (usable(pose.leftKnee) || usable(pose.rightKnee)) {
            h.code = HintCode::DataAvailable; h.message = "MOCK: pose data available";
        } else {
            h.code = HintCode::NoPose; h.message = "MOCK: no usable knee data";
        }
        return h;
    }
};

class MockDataManager final : public IDataManager {
    std::map<std::string, SessionRecord> sessions_;
    AppConfig config_;
public:
    Result saveSession(const SessionRecord& record) override {
        if (record.id.empty() || record.durationMs < 0)
            return resultOf(Status::InvalidArgument, "Nonempty ID and nonnegative duration required");
        sessions_[record.id] = record;
        return resultOf(Status::Ok, "MOCK: saved in memory only");
    }
    std::vector<SessionRecord> listSessions() const override {
        std::vector<SessionRecord> list;
        for (const auto& item : sessions_) {
            SessionRecord metadata;
            metadata.id = item.second.id; metadata.startedAtUtc = item.second.startedAtUtc;
            metadata.videoPath = item.second.videoPath; metadata.durationMs = item.second.durationMs;
            metadata.schemaVersion = item.second.schemaVersion;
            list.push_back(metadata);
        }
        return list;
    }
    Result loadSession(const std::string& id, SessionRecord& out) const override {
        const auto it = sessions_.find(id);
        if (it == sessions_.end()) return resultOf(Status::NotFound, "Session not found");
        out = it->second; return resultOf(Status::Ok, "MOCK: loaded from memory");
    }
    Result saveConfig(const AppConfig& config) override {
        const auto inRange = [](double x) { return std::isfinite(x) && x >= 0 && x <= 1; };
        if (!inRange(config.pose.confidenceThreshold) || !inRange(config.pose.nmsThreshold) ||
            !inRange(config.pose.smoothing) || config.schemaVersion != 1 ||
            config.storageDirectory.empty() ||
            (config.pose.model != "MoveNet" && config.pose.model != "BlazePose" && config.pose.model != "HRNet"))
            return resultOf(Status::InvalidArgument, "Unsupported or invalid configuration");
        config_ = config; return resultOf(Status::Ok, "MOCK: config is not persisted");
    }
    AppConfig loadConfig() const override { return config_; }
    Result resetConfig() override {
        config_ = AppConfig(); return resultOf(Status::Ok, "MOCK: reset in memory");
    }
    Result exportCsv(const std::string&, const std::string&) const override {
        return resultOf(Status::NotImplemented, "M0 Mock does not write CSV files");
    }
    Result exportReport(const std::string&, const std::string&, ReportFormat) const override {
        return resultOf(Status::NotImplemented, "M0 Mock does not write reports");
    }
};
}
