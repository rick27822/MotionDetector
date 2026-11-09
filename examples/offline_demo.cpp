#include "modules/analysis/AnalysisEngine.h"
#include <iostream>
int main() {
    std::vector<motion::PoseResult> poses;
    const double angles[] = {170,140,90,140,170};
    for (int i=0; i<5; ++i) {
        motion::PoseResult p; p.timestampMs=i*200;
        p.leftKnee.valid=true; p.leftKnee.degrees=angles[i]; poses.push_back(p);
    }
    motion::AnalysisEngine engine;
    const auto r=engine.analyze(poses);
    if (!r.result.ok()) { std::cerr << r.result.message; return 1; }
    std::cout << "ROM=" << r.metrics.kneeRomDeg
              << " peakDegPerSec=" << r.metrics.peakAngularSpeedDegPerSec
              << " cycles=" << r.metrics.cycles.size()
              << " grade=" << r.metrics.grade << '\n';
}
