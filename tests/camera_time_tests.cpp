#include "modules/analysis/CameraTimeAdapter.h"
#include <iostream>
#include <stdexcept>
#include <limits>
int main() {
    using namespace motion;
    int count=0;
    const auto check=[&](bool ok, const char* name) {
        if (!ok) throw std::runtime_error(name);
        ++count; std::cout << "PASS " << name << '\n';
    };
    CameraTimeAdapter a; PoseResult angles,out;
    angles.leftKnee.valid=true; angles.leftKnee.degrees=170;
    check(!a.convert("s",0,1000,angles,out).ok(), "explicit start required");
    check(!a.beginSession("",1000).ok(), "empty session rejected");
    check(a.beginSession("s",1000).ok() && a.convert("s",0,1000,angles,out).ok() &&
          out.timestampMs==0 && out.sessionId=="s" && out.leftKnee.degrees==170, "origin and angle preservation");
    check(a.convert("s",3,1000.2,angles,out).ok() && out.timestampMs==200 && out.frameId==3, "seconds to milliseconds with dropped frames");
    check(!a.convert("s",3,1000.3,angles,out).ok(), "duplicate frame rejected");
    check(!a.convert("s",4,1000.2001,angles,out).ok() && out.frameId==3, "duplicate millisecond preserves output");
    check(!a.convert("s",4,1000.1,angles,out).ok(), "clock reversal rejected");
    check(!a.convert("other",4,1000.3,angles,out).ok(), "implicit session switch rejected");
    check(!a.convert("s",4,std::numeric_limits<double>::infinity(),angles,out).ok(), "infinite timestamp");
    check(a.beginSession("new",2000).ok() && a.convert("new",0,2000,angles,out).ok(), "new session resets counters");
    check(!a.convert("new",1,1e30,angles,out).ok(), "timestamp overflow");
    std::cout << count << " adapter checks passed\n";
}
