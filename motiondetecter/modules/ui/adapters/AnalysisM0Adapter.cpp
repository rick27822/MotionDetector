#include "AnalysisM0Adapter.h"
#include "vendor/analysis_m0/modules/analysis/MockServices.h"
namespace ui {
AnalysisViewData AnalysisM0Adapter::analyze(const QVector<core::AngleSample> &samples) const {
    std::vector<motion::PoseResult> input;
    input.reserve(samples.size());
    for (const auto &s : samples) {
        motion::PoseResult p;
        p.timestampMs = s.t;
        auto a = [&](core::JointAngle j) {
            motion::AngleSample v;
            v.valid = s.hasAngle(j);
            v.degrees = s.angle(j);
            return v;
        };
        p.leftKnee = a(core::JointAngle::LeftKnee);
        p.rightKnee = a(core::JointAngle::RightKnee);
        p.leftHip = a(core::JointAngle::LeftHip);
        p.rightHip = a(core::JointAngle::RightHip);
        p.trunk = a(core::JointAngle::Trunk);
        p.leftElbow = a(core::JointAngle::LeftElbow);
        p.rightElbow = a(core::JointAngle::RightElbow);
        input.push_back(p);
    }
    motion::MockAnalysisService service;
    auto response = service.analyze(input);
    AnalysisViewData out;
    if (!response.result.ok()) {
        out.error = QString::fromStdString(response.result.message);
        return out;
    }
    auto &m = response.metrics;
    out.valid = m.valid;
    out.simulated = m.simulated;
    out.maximum = m.kneeMaxDeg;
    out.rom = m.kneeRomDeg;
    out.peakSpeed = m.peakAngularSpeedDegPerSec;
    out.symmetryPercent = m.symmetryScore * 100;
    out.scorePercent = m.qualityScore * 10;
    out.grade = "良好 · 模拟";
    return out;
}
} // namespace ui
