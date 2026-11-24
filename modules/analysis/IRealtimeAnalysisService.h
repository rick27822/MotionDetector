#pragma once
#include <cstdint>
#include <string>
namespace motion {
// Independent UI boundary: no dependency on either version of core/types.h.
struct RealtimeAngle { double degrees = 0; bool valid = false; };
struct RealtimePoseInput {
    std::string sessionId;
    std::int64_t timestampMs = 0, frameId = -1;
    RealtimeAngle leftKnee, trunk;
};
enum class RealtimeStatus { Ok, NoData, InvalidArgument };
struct RealtimeCallResult {
    RealtimeStatus status = RealtimeStatus::Ok;
    std::string message;
    bool ok() const { return status == RealtimeStatus::Ok; }
};
enum class RealtimeHintCode { WaitingForStanding, Ready, Descending, Bottom, Rising,
                              RepCompleted, IncompleteRep, DataLost, InvalidInput };
struct RealtimeAnalysisConfig {
    double standingDeg = 160, bottomDeg = 120, directionDeltaDeg = 2;
    std::int64_t maxGapMs = 500, minCycleMs = 500;
    bool enableTrunkHint = false;
    double trunkLimitDeg = 45;
};
struct RealtimeAnalysisResult {
    RealtimeCallResult result;
    RealtimeHintCode code = RealtimeHintCode::WaitingForStanding;
    std::string message, sessionId;
    std::uint64_t completedReps = 0;
    bool repCompleted = false;
    bool trunkHintAvailable = false, forwardLean = false;
};
class IRealtimeAnalysisService {
public:
    virtual ~IRealtimeAnalysisService() = default;
    virtual RealtimeAnalysisResult processPose(const RealtimePoseInput& pose) = 0;
    // Same session-relative monotonic milliseconds as pose.timestampMs.
    virtual RealtimeAnalysisResult poll(std::int64_t nowMs) = 0;
    virtual void reset() = 0;
};
}
