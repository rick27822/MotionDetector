#include "modules/analysis/MockServices.h"
#include <iostream>
#include <limits>
#include <stdexcept>

void check(bool ok, const char* name) {
    if (!ok) throw std::runtime_error(name);
    std::cout << "PASS " << name << '\n';
}
int main() {
    using namespace motion;
    MockAnalysisService a; MockDataManager d;
    check(a.analyze({}).result.status == Status::NoData, "empty input");
    PoseResult p; p.leftKnee.valid = true; p.leftKnee.degrees = 170;
    auto response = a.analyze({p});
    check(response.result.ok() && response.metrics.simulated && response.metrics.qualityScore == 8,
          "fixed mock response is explicitly simulated");
    PoseResult q = p; q.timestampMs = 1000; q.leftKnee.degrees = 90;
    response = a.analyze({p,q});
    check(response.metrics.kneeCurve.size() == 2 &&
          response.metrics.kneeCurve[1].timestampMs == 1000 &&
          response.metrics.kneeCurve[1].leftKnee.degrees == 90, "curve preserves sample values and time");
    check(a.analyze({p,p}).result.status == Status::InvalidArgument, "duplicate timestamp");
    check(a.analyze({q,p}).result.status == Status::InvalidArgument, "reversed timestamp");
    PoseResult invalid;
    check(a.analyze({invalid}).result.status == Status::NoData, "missing angles");
    invalid.leftKnee.valid = true; invalid.leftKnee.degrees = std::numeric_limits<double>::quiet_NaN();
    check(a.analyze({invalid}).result.status == Status::NoData, "NaN is not usable");
    check(a.evaluateHint(invalid).code == HintCode::NoPose &&
          a.evaluateHint(p).code == HintCode::DataAvailable, "hint states");
    SessionRecord s; s.id = "s1"; s.poses = {p,q};
    check(d.saveSession(s).ok() && d.listSessions().size() == 1 &&
          d.listSessions()[0].poses.empty(), "save and metadata-only list");
    s.durationMs = 1000; d.saveSession(s);
    SessionRecord loaded;
    check(d.listSessions().size() == 1 && d.loadSession("s1", loaded).ok() &&
          loaded.durationMs == 1000 && loaded.poses.size() == 2, "upsert and load sequence");
    check(d.loadSession("absent", loaded).status == Status::NotFound, "unknown session");
    s.id.clear(); check(d.saveSession(s).status == Status::InvalidArgument, "empty ID");
    auto config = d.loadConfig(); config.pose.smoothing = 0.8;
    check(d.saveConfig(config).ok() && d.loadConfig().pose.smoothing == 0.8, "config round trip");
    config.pose.smoothing = 2;
    check(d.saveConfig(config).status == Status::InvalidArgument &&
          d.loadConfig().pose.smoothing == 0.8, "invalid config preserves previous state");
    d.resetConfig(); check(d.loadConfig().pose.smoothing == 0.5, "reset defaults");
    MockDataManager fresh;
    check(fresh.listSessions().empty(), "new instance has no persisted sessions");
    check(d.exportCsv("s1", "x.csv").status == Status::NotImplemented &&
          d.exportReport("s1", "x.pdf", ReportFormat::Pdf).status == Status::NotImplemented,
          "exports never report false success");
    std::cout << "17 checks passed\n";
}
