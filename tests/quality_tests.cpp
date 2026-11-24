#include "modules/analysis/AnalysisEngine.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace motion;
static int checks = 0;
static void check(bool value, const char* name) {
    if (!value) throw std::runtime_error(name);
    ++checks; std::cout << "PASS " << name << '\n';
}
static bool near(double a, double b) { return std::abs(a-b)<1e-8; }
static std::vector<PoseResult> cycle(double rightRom=80, double trunk=0) {
    std::vector<PoseResult> rows;
    const double left[]={170,140,90,140,170};
    for (int i=0;i<5;++i) {
        PoseResult p; p.timestampMs=i*200;
        p.leftKnee={left[i],true};
        p.rightKnee={170-(170-left[i])*rightRom/80,true};
        p.trunk={trunk,true}; rows.push_back(p);
    }
    return rows;
}
int main() {
    AnalysisConfig config;
    config.quality.enabled=true; config.quality.version="experimental-test-v1";
    config.quality.targetRomDeg=80;
    AnalysisEngine score(config), metrics;
    auto result=metrics.analyze(cycle());
    check(result.metrics.symmetryAvailable && near(result.metrics.symmetryScore,1), "equal bilateral ROM");
    check(!result.metrics.qualityScoreAvailable && result.metrics.grade=="UNSCORED", "default has no score");
    result=metrics.analyze(cycle(40));
    check(near(result.metrics.symmetryScore,0.5), "known ROM ratio");
    result=metrics.analyze(cycle(0));
    check(result.metrics.symmetryAvailable && near(result.metrics.symmetryScore,0), "motionless right side is measured zero symmetry");
    auto rows=cycle(); for(auto& p:rows) p.rightKnee.valid=false;
    check(!metrics.analyze(rows).metrics.symmetryAvailable, "missing right knee unavailable");
    rows=cycle(); for(auto& p:rows) p.leftKnee.degrees=p.rightKnee.degrees=170;
    check(!metrics.analyze(rows).metrics.symmetryAvailable, "static sequence has undefined ROM ratio");
    rows=cycle(); rows[2].rightKnee.valid=false; rows[2].rightKnee.degrees=0;
    check(near(metrics.analyze(rows).metrics.symmetryScore,1), "compare only matched valid frames");
    rows=cycle(); rows.resize(2);
    check(!metrics.analyze(rows).metrics.symmetryAvailable, "too few paired samples");
    result=score.analyze(cycle());
    check(result.metrics.qualityScoreAvailable && near(result.metrics.qualityScore,10), "known maximum score");
    check(result.metrics.grade=="A" && result.metrics.ruleVersion=="experimental-test-v1", "grade and provenance");
    result=score.analyze(cycle(40,22.5));
    check(near(result.metrics.qualityScore,7.5) && result.metrics.grade=="B", "weighted score and grade B");
    auto lower=config; lower.quality.targetRomDeg=160;
    result=AnalysisEngine(lower).analyze(cycle(0,90));
    check(near(result.metrics.qualityScore,2.5) && result.metrics.grade=="C", "bounded trunk penalty and grade C");
    rows=cycle(); rows[1].trunk.valid=false;
    check(!score.analyze(rows).metrics.qualityScoreAvailable, "missing trunk cannot fabricate score");
    rows=cycle(); rows[2].rightKnee.degrees=std::numeric_limits<double>::quiet_NaN();
    check(!score.analyze(rows).metrics.qualityScoreAvailable, "NaN right knee blocks scoring");
    rows=cycle(); rows[1].trunk.degrees=91;
    check(!score.analyze(rows).metrics.qualityScoreAvailable, "invalid trunk blocks scoring");
    rows=cycle(); rows.pop_back();
    check(!score.analyze(rows).metrics.qualityScoreAvailable, "incomplete cycle unscored");
    rows=cycle(); for(std::size_t i=2;i<rows.size();++i) rows[i].timestampMs+=1000;
    check(!score.analyze(rows).metrics.qualityScoreAvailable, "gap prevents score");
    auto invalid=config; invalid.quality.version.clear();
    check(AnalysisEngine(invalid).analyze(cycle()).result.status==Status::InvalidArgument, "require rule version");
    invalid=config; invalid.quality.romWeight=-1;
    check(AnalysisEngine(invalid).analyze(cycle()).result.status==Status::InvalidArgument, "negative weight");
    invalid=config; invalid.quality.targetRomDeg=0;
    check(AnalysisEngine(invalid).analyze(cycle()).result.status==Status::InvalidArgument, "zero divisor");
    invalid=config; invalid.quality.gradeB=invalid.quality.gradeA;
    check(AnalysisEngine(invalid).analyze(cycle()).result.status==Status::InvalidArgument, "invalid grade thresholds");
    invalid=config; invalid.quality.trunkWeight=std::numeric_limits<double>::infinity();
    check(AnalysisEngine(invalid).analyze(cycle()).result.status==Status::InvalidArgument, "nonfinite rule");
    // Equal component values must remain 5/10 even for very large finite weights.
    auto large=config;
    large.quality.targetRomDeg=160;
    large.quality.romWeight=large.quality.symmetryWeight=large.quality.trunkWeight=5e307;
    result=AnalysisEngine(large).analyze(cycle(40,22.5));
    check(result.metrics.qualityScoreAvailable && near(result.metrics.qualityScore,5), "large weights do not overflow scoring");
    auto boundary=config;
    boundary.quality.gradeA=7.5;
    check(AnalysisEngine(boundary).analyze(cycle(40,22.5)).metrics.grade=="A", "grade A inclusive boundary");
    boundary.quality.gradeA=9; boundary.quality.gradeB=7.5;
    check(AnalysisEngine(boundary).analyze(cycle(40,22.5)).metrics.grade=="B", "grade B inclusive boundary");
    rows=cycle(); rows.resize(3);
    for(int i=0;i<3;++i) {
        rows[i].timestampMs=i*250;
        rows[i].leftKnee.degrees=rows[i].rightKnee.degrees=i==1?120:160;
    }
    result=score.analyze(rows);
    check(result.metrics.cycles.size()==1 && result.metrics.cycles[0].endMs==500,
          "standing bottom duration thresholds inclusive");
    rows=cycle(); for(auto& p:rows) p.timestampMs=p.timestampMs*5/2;
    result=score.analyze(rows);
    check(result.metrics.cycles.size()==1 && result.metrics.qualityScoreAvailable,
          "500ms gap permitted");
    rows=cycle(); rows[2].leftKnee.valid=false;
    check(!score.analyze(rows).metrics.qualityScoreAvailable, "invalid left knee blocks quality");
    invalid=config; invalid.quality.romWeight=invalid.quality.symmetryWeight=invalid.quality.trunkWeight=1e308;
    check(AnalysisEngine(invalid).analyze(cycle()).result.status==Status::InvalidArgument,
          "overflowed weight sum rejected");
    rows.clear();
    for(int rep=0;rep<12;++rep) {
        auto part=cycle();
        for(std::size_t i=rep?1:0;i<part.size();++i) {
            part[i].timestampMs+=rep*800; rows.push_back(part[i]);
        }
    }
    result=score.analyze(rows);
    check(result.metrics.cycles.size()==12, "twelve completed cycles");
    for(const auto& c:result.metrics.cycles)
        check(c.endMs-c.startMs==800 && near(c.kneeRomDeg,80), "cycle boundaries and extrema");
    check(near(result.metrics.qualityScore,10), "twelve-cycle score");
    std::cout<<checks<<" quality checks passed\n";
}
