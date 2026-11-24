#include "modules/analysis/RealtimeAnalysisService.h"
#include <iostream>
int main() {
    motion::RealtimeAnalysisService service;
    const double angles[]={170,140,90,140,170};
    std::cout<<"Synthetic input, no camera or UI\n";
    for(int i=0;i<5;++i) {
        motion::RealtimePoseInput p; p.sessionId="demo"; p.timestampMs=i*200;
        p.leftKnee={angles[i],true};
        auto r=service.processPose(p);
        std::cout<<p.timestampMs<<"ms | "<<r.message<<" | reps="<<r.completedReps<<'\n';
    }
    auto stale=service.poll(1400);
    std::cout<<"1400ms | "<<stale.message<<'\n';
}
