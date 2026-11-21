// RC1.5.2: no Qt dependency. Verifies irregular-X columns, spikes, log
// extents, incremental append and screen-width view selection.
#include "../src/plot/LegacyXYIndex.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

static bool near(double a,double b,double tolerance=1e-9){return std::abs(a-b)<tolerance;}
int main(){
    using clock=std::chrono::steady_clock;
    std::vector<double> x,y;
    x.reserve(250100);y.reserve(250100);
    constexpr std::size_t n=250000;
    for(std::size_t i=0;i<n;++i){
        // Irregular X spacing: every ninth interval is deliberately longer.
        const double dx=(i%9==0)?0.00023:0.000065;
        x.push_back((x.empty()?0.0:x.back())+dx);
        y.push_back(1.8+std::sin(double(i)*0.03)*0.5);
    }
    const std::size_t spike=123456;
    y[spike]=90.0;
    LegacyXYIndex index;index.build(x,y);
    assert(index.ascending()&&index.allFinite()&&index.count()==n);
    assert(index.allXPositive()); // starts at a positive X coordinate
    assert(index.range(false,false).valid);
    assert(near(index.range(false,false).ymax,90.0));
    assert(index.range(true,true).valid);
    assert(index.range(true,true).xmin>0);
    assert(near(index.range(false,false).xmin,x.front()));
    assert(near(index.range(false,false).xmax,x.back()));
    auto whole=index.columns(x,y,x.front(),x.back(),810,false);
    assert(whole.size()<=810);
    bool preserved=false;
    for(const auto& col:whole)if(col.envelope.high.index==spike){preserved=true;break;}
    assert(preserved);
    auto zoom=index.visibleSpan(x,x[spike-40],x[spike+40]);
    assert(zoom.first==spike-40 && zoom.second==spike+41);
    assert(index.columns(x,y,x[spike-40],x[spike+40],810).size()<=81);
    auto logPlot=index.columns(x,y,x[1000],x[170000],810,true);
    assert(logPlot.size()<=810);
    // Streaming a small append should update the last block, not rebuild N blocks.
    double oldMin=index.range(false,false).xmin;
    std::size_t before=x.size();
    for(int i=0;i<100;++i){x.push_back(x.back()+0.000077);y.push_back(1.5);}
    index.append(x,y,before);
    assert(index.count()==n+100);
    assert(near(index.range(false,false).xmin,oldMin));
    assert(near(index.range(false,false).xmax,x.back()));
    assert(near(index.range(false,false).ymax,90.0));
    double rightLow=0,rightHigh=0;
    assert(index.rightRange(rightLow,rightHigh));
    assert(near(rightHigh,90.0));
    // Unsorted / nonfinite data remains on the legacy safe fallback renderer.
    std::vector<double> badX={0,2,1};std::vector<double> badY={1,2,3};
    LegacyXYIndex bad;bad.build(badX,badY);
    assert(!bad.ascending());
    badY[1]=std::numeric_limits<double>::quiet_NaN();
    bad.build(badX,badY);assert(!bad.allFinite());
    assert(near(bad.range(false,false).ymin,1));
    // Explicit sparse-data right-axis semantics: Y bounds do not depend on X.
    badX={std::numeric_limits<double>::quiet_NaN()};badY={-42};
    bad.build(badX,badY);
    assert(!bad.range(false,false).valid && bad.rightRange(rightLow,rightHigh));
    assert(near(rightLow,-42)&&near(rightHigh,-42));

    const auto start=clock::now();
    std::size_t total=0;
    for(int pass=0;pass<150;++pass){
        const double xmin=pass%3==0?x.front():x[1000+pass*5];
        const double xmax=pass%3==0?x.back():x[100000+pass*10];
        auto cols=index.columns(x,y,xmin,xmax,pass%4==0?410:810);
        assert(cols.size()<=810);
        total+=cols.size();
    }
    const double ms=std::chrono::duration<double,std::milli>(clock::now()-start).count();
    std::cout<<"LegacyXY 250k irregular-X, 150 viewport selections, "<<total
             <<" geometry columns, "<<ms<<" ms [C++ index-only] PASS\n";
}
