// Edge-correct, pixel-bounded Digital Timing/Protocol decimation.
#include "../src/plot/DigitalEdgeIndex.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>
int main(){
    constexpr std::size_t n=250000;
    std::vector<double> x(n),y(n,0);
    for(std::size_t i=0;i<n;++i){
        x[i]=i*0.000001;
        y[i]=(i/5)%2; // dense pulses: five samples per half-cycle
    }
    // Single-sample glitch must survive even when width < 1 screen pixel.
    y[100003]=1-y[100003];
    DigitalEdgeIndex index;index.build(x,y);
    assert(index.valid()&&index.firstX()==x.front()&&index.lastX()==x.back());
    assert(index.eventCount()>20000);
    auto cols=index.columns(0,x.back(),480);
    assert(cols.size()<=480);
    bool hasBoth=false;
    for(const auto& c:cols)if(c.high&&c.low)hasBoth=true;
    assert(hasBoth);
    auto close=index.columns(x[100000],x[100020],100);
    assert(!close.empty());
    assert(index.transitionsIn(x[100000],x[100020])>0);
    for(auto c:close)assert(c.x0<=c.x1);
    const std::size_t old=x.size();
    for(std::size_t i=n;i<n+100;++i){x.push_back(i*.000001);y.push_back((i/5)%2);}
    index.append(x,y,old);
    assert(index.valid()&&index.lastX()==x.back());
    // An isolated 1-sample pulse in a low signal must remain visible.
    std::vector<double> gx(100000),gy(100000,0);
    for(std::size_t i=0;i<gx.size();++i)gx[i]=double(i)*.000001;
    gy[65432]=1;
    DigitalEdgeIndex glitch;glitch.build(gx,gy);
    bool pulse=false;
    for(const auto& c:glitch.columns(gx.front(),gx.back(),450))
        if(c.high && c.low)pulse=true;
    assert(pulse && glitch.eventCount()==3);
    // Backwards or nonfinite data deliberately remains on the old renderer.
    std::vector<double> badX={0,2,1},badY={0,1,0};
    DigitalEdgeIndex bad;bad.build(badX,badY);assert(!bad.valid());
    badX={0,1,std::numeric_limits<double>::quiet_NaN()};
    bad.build(badX,badY);assert(!bad.valid());
    // Multiple digital edges sharing an X coordinate must use the last
    // state at that timestamp, without becoming an interpolation slope.
    badX={0,1,1,2};badY={0,0,1,1};
    bad.build(badX,badY);assert(bad.valid());
    assert(bad.stateAt(0)==false && bad.stateAt(1)==true);
    assert(bad.transitionsIn(0,2)==2);
    const auto start=std::chrono::steady_clock::now();
    std::size_t count=0;
    for(int i=0;i<120;++i){
        double from=i*.0003;
        auto bins=index.columns(from,from+.05,500);
        assert(bins.size()<=500);count+=bins.size();
    }
    auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    std::cout<<"Digital 250k, 120 viewport reductions, "<<count<<" bins, "<<ms<<" ms PASS\n";
}
