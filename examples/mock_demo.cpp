#include "modules/analysis/MockServices.h"
#include <iostream>

int main() {
    motion::MockAnalysisService analysis;
    motion::MockDataManager data;
    std::vector<motion::PoseResult> poses(3);
    const double angles[] = {170, 90, 170};
    for (int i = 0; i < 3; ++i) {
        poses[i].timestampMs = i * 1000;
        poses[i].leftKnee.valid = poses[i].rightKnee.valid = true;
        poses[i].leftKnee.degrees = poses[i].rightKnee.degrees = angles[i];
    }
    const auto response = analysis.analyze(poses);
    if (!response.result.ok()) return 1;
    motion::SessionRecord session;
    session.id = "demo-001"; session.startedAtUtc = "2026-11-05T00:00:00Z";
    session.durationMs = 2000; session.poses = poses; session.metrics = response.metrics;
    if (!data.saveSession(session).ok()) return 2;
    std::cout << "SIMULATED score=" << response.metrics.qualityScore
              << " curvePoints=" << response.metrics.kneeCurve.size()
              << " sessions=" << data.listSessions().size() << '\n';
    std::cout << data.exportCsv(session.id, "demo.csv").message << '\n';
    return 0;
}
