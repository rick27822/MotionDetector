#pragma once
#include "IRealtimeAnalysisService.h"
namespace motion {
// Instantiate with team core::JointAngle/core::PoseResult in the UI boundary.
// No camera frame, inference SDK or landmark geometry is accessed here.
template<class CoreJoint, class CorePose>
RealtimePoseInput adaptRealtimePose(const CorePose& pose, const std::string& sessionId) {
    RealtimePoseInput out;
    out.sessionId=sessionId; out.timestampMs=pose.timestampMs; out.frameId=pose.frameId;
    if (pose.detected) {
        out.leftKnee.valid=pose.hasAngle(CoreJoint::LeftKnee);
        if(out.leftKnee.valid) out.leftKnee.degrees=pose.angle(CoreJoint::LeftKnee);
        out.trunk.valid=pose.hasAngle(CoreJoint::Trunk);
        if(out.trunk.valid) out.trunk.degrees=pose.angle(CoreJoint::Trunk);
    }
    return out;
}
}
