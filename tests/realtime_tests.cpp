#include "modules/analysis/RealtimeAnalysisService.h"
#include "modules/analysis/RealtimePoseAdapter.h"
#include "modules/analysis/AnalysisEngine.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace motion;
static int checks=0;
static void check(bool pass,const char* name) {
    if(!pass) throw std::runtime_error(name);
    ++checks; std::cout<<"PASS "<<name<<'\n';
}
static RealtimePoseInput frame(std::int64_t t,double angle,const std::string& session="s") {
    RealtimePoseInput p; p.timestampMs=t; p.leftKnee={angle,true}; p.sessionId=session; return p;
}
// Shape mock only: actual Qt UI assembly remains a separate integration acceptance.
enum class Joint { LeftKnee, Trunk };
struct PoseShape {
    bool detected=true; int frameId=7; std::int64_t timestampMs=4200;
    bool hasAngle(Joint j) const { return j==Joint::LeftKnee; }
    float angle(Joint) const { return 0; }
};
int main() {
    PoseShape shape;
    auto adapted=adaptRealtimePose<Joint>(shape,"live-session");
    check(adapted.sessionId=="live-session" && adapted.timestampMs==4200 && adapted.frameId==7,
          "adapter keeps external session capture time and frame identity");
    check(adapted.leftKnee.valid && adapted.leftKnee.degrees==0 && !adapted.trunk.valid,
          "adapter preserves zero and invalid channel");
    shape.detected=false;
    check(!adaptRealtimePose<Joint>(shape,"live-session").leftKnee.valid,"undetected pose invalidates stale angles");
    RealtimeAnalysisService service;
    check(service.poll(0).result.status==RealtimeStatus::NoData,"initial wait");
    check(service.processPose(frame(0,100)).code==RealtimeHintCode::WaitingForStanding,"start at bottom not armed");
    check(service.processPose(frame(100,170)).code==RealtimeHintCode::Ready,"standing arms cycle");
    check(service.processPose(frame(300,140)).code==RealtimeHintCode::Descending,"descending cue");
    check(service.processPose(frame(500,90)).code==RealtimeHintCode::Bottom,"bottom cue");
    check(service.processPose(frame(700,140)).code==RealtimeHintCode::Rising,"rising cue");
    auto result=service.processPose(frame(900,170));
    check(result.repCompleted && result.completedReps==1,"completed once");
    check(!service.poll(900).repCompleted,"poll does not repeat completion event");
    check(service.processPose(frame(1000,170)).completedReps==1,"standing does not count again");
    check(service.poll(1500).code==RealtimeHintCode::Ready,"500ms still current");
    check(service.poll(1501).code==RealtimeHintCode::DataLost,"stale without new frame");
    check(service.processPose(frame(1600,100)).code==RealtimeHintCode::WaitingForStanding,"stale cancels cycle");
    check(service.processPose(frame(1700,170)).completedReps==1,"recover keeps prior count");
    service.processPose(frame(1900,140));
    result=service.processPose(frame(2300,170));
    check(result.code==RealtimeHintCode::IncompleteRep && result.completedReps==1,"shallow does not count");
    service.reset();
    service.processPose(frame(0,170)); service.processPose(frame(50,90));
    check(service.processPose(frame(100,170)).code==RealtimeHintCode::IncompleteRep,"too short does not count");
    service.reset(); service.processPose(frame(0,170));
    auto missing=frame(200,90); missing.leftKnee.valid=false;
    check(service.processPose(missing).code==RealtimeHintCode::DataLost,"missing knee resets movement");
    check(service.processPose(frame(400,100)).code==RealtimeHintCode::WaitingForStanding,"missing cannot bridge into cycle");
    service.reset(); service.processPose(frame(0,170)); service.processPose(frame(200,90));
    check(service.processPose(frame(200,100)).result.status==RealtimeStatus::InvalidArgument,"duplicate timestamp rejected");
    check(service.processPose(frame(800,170)).completedReps==0,"duplicate breaks cycle");
    check(service.processPose(frame(700,170)).result.status==RealtimeStatus::InvalidArgument,"reversed timestamp rejected");
    check(service.processPose(frame(-1,170)).result.status==RealtimeStatus::InvalidArgument,"negative time rejected");
    auto nan=frame(1000,std::numeric_limits<double>::quiet_NaN());
    check(service.processPose(nan).code==RealtimeHintCode::DataLost,"nonfinite knee missing");
    service.reset(); auto id=frame(0,170); id.frameId=5;
    service.processPose(id); id.timestampMs=100;
    check(service.processPose(id).result.status==RealtimeStatus::InvalidArgument,"duplicate known frame identity");
    service.reset(); id=frame(0,170); id.frameId=5; service.processPose(id);
    service.processPose(frame(100,140)); id=frame(200,90); id.frameId=4;
    check(service.processPose(id).result.status==RealtimeStatus::InvalidArgument,"unknown identity does not erase last known ID");
    RealtimeAnalysisConfig bad; bad.bottomDeg=bad.standingDeg;
    check(RealtimeAnalysisService(bad).processPose(frame(0,170)).result.status==RealtimeStatus::InvalidArgument,"invalid thresholds");
    bad={}; bad.directionDeltaDeg=0;
    check(RealtimeAnalysisService(bad).poll(0).result.status==RealtimeStatus::InvalidArgument,"invalid direction threshold");
    check(service.poll(-1).result.status==RealtimeStatus::InvalidArgument,"invalid clock");
    service.reset(); service.processPose(frame(0,170));
    check(service.poll(0).code==RealtimeHintCode::Ready,"clock same timestamp permitted");
    service.processPose(frame(100,140));
    check(service.processPose(frame(200,139)).code==RealtimeHintCode::Descending,"small change holds direction");
    service.reset(); std::vector<PoseResult> sequence;
    const double angles[]={170,140,90,140,170};
    for(int rep=0;rep<12;++rep)
        for(int i=rep?1:0;i<5;++i) {
            auto p=frame(rep*800+i*200,angles[i]);
            PoseResult offline; offline.timestampMs=p.timestampMs; offline.sessionId=p.sessionId;
            offline.leftKnee={p.leftKnee.degrees,p.leftKnee.valid}; sequence.push_back(offline);
            result=service.processPose(p);
        }
    check(result.completedReps==12,"twelve streamed repetitions");
    check(result.completedReps==AnalysisEngine().analyze(sequence).metrics.cycles.size(),"streamed and offline counts agree");
    check(service.processPose(frame(0,170,"new")).completedReps==0,"new session resets count and clock");
    service.reset(); check(service.poll(0).completedReps==0,"explicit reset on disconnect");
    RealtimeAnalysisConfig trunk; trunk.enableTrunkHint=true;
    RealtimeAnalysisService lean(trunk);
    auto p=frame(0,170); p.trunk={46,true};
    check(!RealtimeAnalysisService().processPose(p).trunkHintAvailable,"posture threshold disabled by default");
    result=lean.processPose(p);
    check(result.trunkHintAvailable && result.forwardLean,"optional trunk threshold");
    p.timestampMs=100; p.trunk.degrees=45;
    result=lean.processPose(p);
    check(result.trunkHintAvailable && !result.forwardLean,"trunk equality boundary");
    p.timestampMs=200; p.trunk.valid=false;
    check(!lean.processPose(p).trunkHintAvailable,"missing trunk not good posture");
    service.reset(); service.processPose(frame(0,160)); service.processPose(frame(250,120));
    check(service.processPose(frame(500,160)).repCompleted,"count exact thresholds");
    std::cout<<checks<<" realtime checks passed\n";
}
