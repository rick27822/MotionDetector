#include "modules/analysis/AnalysisEngine.h"
#include <iostream>
#include <stdexcept>
#include <limits>

static int count = 0;
void check(bool x, const char* name) {
    if (!x) throw std::runtime_error(name);
    ++count; std::cout << "PASS " << name << '\n';
}
motion::PoseResult sample(long long ms, double deg, bool valid = true) {
    motion::PoseResult p; p.timestampMs = ms; p.leftKnee.degrees = deg; p.leftKnee.valid = valid;
    return p;
}
bool near(double a, double b) { return std::abs(a-b) < 1e-8; }
int main() {
    using namespace motion;
    AnalysisEngine a;
    check(a.analyze({}).result.status == Status::NoData, "empty");
    auto r = a.analyze({sample(0,170),sample(100,170)});
    check(r.result.ok() && near(r.metrics.kneeRomDeg,0) && near(r.metrics.peakAngularSpeedDegPerSec,0), "constant");
    r = a.analyze({sample(0,170),sample(100,150),sample(300,110)});
    check(near(r.metrics.kneeMinDeg,110) && near(r.metrics.kneeMaxDeg,170) && near(r.metrics.kneeRomDeg,60), "known extrema and ROM");
    check(near(r.metrics.peakAngularSpeedDegPerSec,200), "irregular sample intervals");
    check(r.metrics.kneeCurve[2].timestampMs == 300 && !r.metrics.simulated, "curve and measured flag");
    check(!r.metrics.qualityScoreAvailable && !r.metrics.symmetryAvailable &&
          std::isfinite(r.metrics.qualityScore) && r.metrics.grade == "UNSCORED", "explicit unavailable scores");
    check(a.analyze({sample(0,170),sample(0,100)}).result.status == Status::InvalidArgument, "duplicate time");
    check(a.analyze({sample(100,170),sample(0,100)}).result.status == Status::InvalidArgument, "reversed time");
    check(a.analyze({sample(-1,170)}).result.status == Status::InvalidArgument, "negative time");
    check(a.analyze({sample(0,200)}).result.status == Status::NoData, "out of range angle");
    check(a.analyze({sample(0,std::numeric_limits<double>::quiet_NaN())}).result.status == Status::NoData, "NaN angle");
    r = a.analyze({sample(0,170),sample(100,0,false),sample(200,100)});
    check(near(r.metrics.peakAngularSpeedDegPerSec,0) && !r.metrics.kneeCurve[1].leftKnee.valid, "no derivative over invalid sample");
    r = a.analyze({sample(0,170),sample(1000,90)});
    check(near(r.metrics.peakAngularSpeedDegPerSec,0) && r.metrics.cycles.empty(), "no derivative across long gap");
    std::vector<PoseResult> cycle = {sample(0,170),sample(200,140),sample(400,90),sample(600,140),sample(800,170)};
    r = a.analyze(cycle);
    check(r.metrics.cycles.size() == 1 && r.metrics.cycles[0].startMs == 0 && r.metrics.cycles[0].endMs == 800, "one completed cycle");
    check(near(r.metrics.cycles[0].kneeRomDeg,80), "cycle ROM");
    auto twice = cycle;
    for (std::size_t i=1; i<cycle.size(); ++i) { auto p=cycle[i]; p.timestampMs+=800; twice.push_back(p); }
    check(a.analyze(twice).metrics.cycles.size() == 2, "two cycles");
    check(a.analyze({sample(0,170),sample(200,145),sample(600,170)}).metrics.cycles.empty(), "shallow movement");
    check(a.analyze({sample(0,170),sample(200,100)}).metrics.cycles.empty(), "unfinished movement");
    check(a.analyze({sample(0,100),sample(200,170)}).metrics.cycles.empty(), "must begin standing");
    check(a.analyze({sample(0,170),sample(50,100),sample(100,170)}).metrics.cycles.empty(), "too short cycle");
    auto broken=cycle; broken[2].leftKnee.valid=false;
    check(a.analyze(broken).metrics.cycles.empty(), "missing sample resets cycle");
    AnalysisConfig bad; bad.bottomDeg=170;
    check(AnalysisEngine(bad).analyze(cycle).result.status == Status::InvalidArgument, "invalid threshold config");
    auto right=sample(0,170,false); right.rightKnee.valid=true; right.rightKnee.degrees=100;
    check(a.analyze({right}).result.status == Status::NoData, "no silent side switching");
    auto s1=sample(0,170); s1.sessionId="a";
    auto s2=sample(100,150); s2.sessionId="b";
    check(a.analyze({s1,s2}).result.status == Status::InvalidArgument, "reject mixed sessions");
    check(!a.analyze({s1}).metrics.angularSpeedAvailable, "one sample has no velocity");
    check(a.analyze({sample(0,170),sample(100,170)}).metrics.angularSpeedAvailable, "measured zero velocity is available");
    check(!a.analyze({sample(0,170),sample(1000,150)}).metrics.angularSpeedAvailable, "gap does not fabricate zero velocity");
    check(a.evaluateHint(s1).code == HintCode::DataAvailable, "real hint is not demo state");
    std::cout << count << " analysis checks passed\n";
}
