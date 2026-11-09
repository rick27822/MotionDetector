#pragma once
#include "core/types.h"

namespace motion {
class IAnalysisService {
public:
    virtual ~IAnalysisService() = default;
    // Synchronous M0 contract: UI must schedule long-running real analysis off-thread.
    virtual AnalysisResponse analyze(const std::vector<PoseResult>& poses) const = 0;
    virtual ActionHint evaluateHint(const PoseResult& pose) const = 0;
};
}
