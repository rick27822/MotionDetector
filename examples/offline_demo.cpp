#include "modules/analysis/AnalysisEngine.h"
#include <iostream>
int main(int argc, char** argv) {
    motion::AnalysisConfig config;
    if (argc == 2 && std::string(argv[1]) == "--experimental-score") {
        config.quality.enabled = true;
        config.quality.version = "experimental-demo-v1-not-approved";
        std::cout << "EXPERIMENTAL rule, awaiting team approval\n";
    } else if (argc != 1) {
        std::cerr << "Usage: offline_demo [--experimental-score]\n";
        return 2;
    }
    std::vector<motion::PoseResult> poses;
    const double angles[] = {170,140,90,140,170};
    for (int rep=0; rep<12; ++rep) {
        for (int i=rep ? 1 : 0; i<5; ++i) {
            motion::PoseResult p; p.timestampMs=rep*800+i*200;
            p.leftKnee={angles[i],true}; p.rightKnee={angles[i],true};
            p.trunk={0,true}; poses.push_back(p);
        }
    }
    const auto r=motion::AnalysisEngine(config).analyze(poses);
    if (!r.result.ok()) { std::cerr << r.result.message; return 1; }
    const auto& m=r.metrics;
    std::cout << "Input=synthetic LEFT-knee analysis\n"
              << "samples=" << poses.size() << " minDeg=" << m.kneeMinDeg
              << " maxDeg=" << m.kneeMaxDeg << " ROM=" << m.kneeRomDeg
              << " peakDegPerSec=" << m.peakAngularSpeedDegPerSec
              << " cycles=" << m.cycles.size() << '\n';
    if (m.symmetryAvailable) std::cout << "pairedRomSymmetry=" << m.symmetryScore << '\n';
    if (m.qualityScoreAvailable) std::cout << "quality=" << m.qualityScore << "/10 grade=" << m.grade << '\n';
    else std::cout << "quality=unavailable grade=" << m.grade << '\n';
    std::cout << "rule=" << m.ruleVersion << '\n';
}
