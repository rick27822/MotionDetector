#pragma once
#include <cstdint>
#include <string>
#include <vector>

// M0 proposal v0.1: team approval required before freezing.
namespace motion {
enum class Status { Ok, NoData, NotFound, InvalidArgument, NotImplemented };
struct Result {
    Status status = Status::Ok;
    std::string message;
    bool ok() const { return status == Status::Ok; }
};
struct AngleSample {
    double degrees = 0.0;
    bool valid = false; // Do not use zero as a missing-value marker.
};
struct PoseResult {
    std::string sessionId; // Empty allowed for offline synthetic data only.
    std::int64_t frameId = -1; // Unknown=-1; preserve camera identity when available.
    std::int64_t timestampMs = 0; // Monotonic milliseconds since session start.
    AngleSample leftKnee, rightKnee, leftHip, rightHip, trunk, leftElbow, rightElbow;
};
struct CurvePoint {
    std::int64_t timestampMs = 0;
    AngleSample leftKnee, rightKnee;
};
struct CycleMetrics {
    std::int64_t startMs = 0, endMs = 0;
    double kneeMinDeg = 0, kneeMaxDeg = 0, kneeRomDeg = 0;
};
struct SessionMetrics {
    bool valid = false;
    bool simulated = true;
    double kneeMinDeg = 0, kneeMaxDeg = 0, kneeRomDeg = 0;
    double peakAngularSpeedDegPerSec = 0;
    bool angularSpeedAvailable = false;
    bool symmetryAvailable = false, qualityScoreAvailable = false;
    double symmetryScore = 0, qualityScore = 0; // Read only when corresponding flag is true.
    std::string grade, ruleVersion;
    std::vector<CycleMetrics> cycles;
    std::vector<CurvePoint> kneeCurve;
};
struct AnalysisResponse {
    Result result;
    SessionMetrics metrics;
};
enum class HintCode { NoPose, DataAvailable };
struct ActionHint {
    HintCode code = HintCode::NoPose;
    bool simulated = true;
    std::string message;
};
struct SessionRecord {
    std::string id, startedAtUtc, videoPath;
    std::int64_t durationMs = 0;
    int schemaVersion = 1;
    std::vector<PoseResult> poses;
    SessionMetrics metrics;
};
struct PoseConfig {
    std::string model = "MoveNet"; // Proposed default only.
    double confidenceThreshold = 0.5, nmsThreshold = 0.5, smoothing = 0.5;
    std::vector<int> enabledKeypoints; // Model-specific indices: confirm with Pose owner.
};
struct AppConfig {
    int schemaVersion = 1;
    PoseConfig pose;
    std::string storageDirectory = "data";
};
enum class ReportFormat { Html, Markdown, Pdf };
} // namespace motion
