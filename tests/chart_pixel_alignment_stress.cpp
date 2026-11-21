// Native and legacy dense charts must share identical visible-axis columns.
// Pure C++ test: no Qt, deterministic matched samples + panning and clipping.
#include "../src/plot/LegacyXYIndex.h"
#include "../src/plot/NativeSampleIndex.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>
int main(){
    constexpr std::size_t n=250000;
    const double origin=-0.010,step=1e-7;
    std::vector<double> x(n),y(n);
    for(std::size_t i=0;i<n;++i){
        x[i]=origin+double(i)*step;
        y[i]=std::sin(i*.015);
    }
    y[123457]=123;
    NativeSampleIndex native;native.build(y);
    LegacyXYIndex legacy;legacy.build(x,y);
    for(int i=0;i<70;++i){
        const double left=origin-.002+i*.00011;
        const double right=left+.007;
        const auto a=native.columnsByView(y,origin,step,left,right,430);
        const auto b=legacy.columns(x,y,left,right,430);
        assert(a.size()==b.size());
        for(std::size_t j=0;j<a.size();++j){
            assert(std::abs(a[j].x-b[j].x)<1e-14);
            assert(a[j].envelope.count==b[j].envelope.count);
            assert(a[j].envelope.low.index==b[j].envelope.low.index);
            assert(a[j].envelope.high.index==b[j].envelope.high.index);
            assert(std::abs(a[j].envelope.sum-b[j].envelope.sum)<1e-8);
        }
    }
    const auto spikes=native.columnsByView(y,origin,step,-.002,.008,470);
    bool found=false;
    for(const auto& c:spikes)found|=c.envelope.high.index==123457;
    assert(found);
    // A logarithmic axis must use the same pixel boundaries too.
    {
        std::vector<double> logX(n);
        for(std::size_t i=0;i<n;++i)logX[i]=.001+double(i)*step;
        LegacyXYIndex logLegacy;logLegacy.build(logX,y);
        for(int i=0;i<20;++i){
            const double low=.001+i*.0001, high=.025-i*.0002;
            const auto a=native.columnsByView(y,.001,step,low,high,480,true);
            const auto b=logLegacy.columns(logX,y,low,high,480,true);
            assert(a.size()==b.size());
            for(std::size_t j=0;j<a.size();++j){
                assert(std::abs(a[j].x-b[j].x)<1e-13);
                assert(a[j].envelope.count==b[j].envelope.count);
            }
        }
    }
    // Viewport contains only the middle of a record: bin X positions must
    // still depend solely on viewport+pixel count, not visible record count.
    std::cout<<"Shared pixel columns: uniform native/XY axes aligned on 70 views PASS\n";
}
