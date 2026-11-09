#pragma once
#include "core/types.h"
#include <cmath>
#include <limits>

namespace motion {
// Metadata helper only: NOT a Python runtime bridge and does not perform inference.
// Caller supplies already inferred angles with the SAME capture timestamp/frame ID.
class CameraTimeAdapter {
    std::string session_;
    bool started_ = false;
    double origin_ = 0;
    std::int64_t lastMs_ = -1, lastFrame_ = -1;
    static Result error(const char* text) {
        Result r; r.status = Status::InvalidArgument; r.message = text; return r;
    }
public:
    Result beginSession(const std::string& id, double originUnixSeconds) {
        if (id.empty() || !std::isfinite(originUnixSeconds) || originUnixSeconds < 0)
            return error("Nonempty session ID and valid Unix seconds required");
        session_ = id; origin_ = originUnixSeconds; started_ = true;
        lastMs_ = lastFrame_ = -1; return Result();
    }
    Result convert(const std::string& id, std::int64_t frameId, double receivedAt,
                   const PoseResult& inferredAngles, PoseResult& output) {
        if (!started_ || id != session_) return error("Explicit matching beginSession required");
        if (frameId < 0 || frameId <= lastFrame_) return error("Frame IDs must increase within session");
        if (!std::isfinite(receivedAt) || receivedAt < origin_) return error("Invalid capture timestamp");
        const long double rounded = std::round((static_cast<long double>(receivedAt)-origin_)*1000.0L);
        if (!std::isfinite(rounded) || rounded >= static_cast<long double>(std::numeric_limits<std::int64_t>::max()))
            return error("Timestamp overflow");
        const auto ms = static_cast<std::int64_t>(rounded);
        if (ms <= lastMs_) return error("Duplicate millisecond or backward clock; sample rejected");
        PoseResult converted = inferredAngles;
        converted.sessionId = id; converted.frameId = frameId; converted.timestampMs = ms;
        output = converted; lastMs_ = ms; lastFrame_ = frameId;
        return Result();
    }
};
}
