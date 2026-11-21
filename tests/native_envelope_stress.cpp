// Hardware-free 4x1M data-index workload. Qt-independent so this can be
// checked on toolchains without Qt, and measures envelope selection cost.
#include "../src/plot/NativeSampleIndex.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>
int main() {
    constexpr std::size_t n=1000000;
    std::vector<NativeSampleIndex> indexes(4);
    std::vector<std::vector<double>> signal(4,std::vector<double>(n));
    for(std::size_t ch=0;ch<4;++ch){
        // Sweep / sawtooth-like trace, deliberately hostile to antialiasing.
        for(std::size_t i=0;i<n;++i)
            signal[ch][i]=double((i*73+1519+ch*317)&65535)/65535.0+ch*0.17;
        signal[ch][632117]=5+ch; // spike must survive entire-record display
        indexes[ch].build(signal[ch]);
    }
    const auto t0=std::chrono::steady_clock::now();
    std::size_t totalColumns=0;
    for(std::size_t pass=0;pass<120;++pass){
        // Full record, progressively narrow view, pan, and fast preview widths.
        const std::size_t first=(pass%3==0)?0:pass*251;
        const std::size_t count=(pass%3==0)?n:(pass%3==1?440000:std::size_t(120+pass*100));
        const std::size_t end=std::min(n,first+count);
        const std::size_t width=(pass%5==0)?410:820;
        for(std::size_t ch=0;ch<4;++ch){
            auto out=indexes[ch].columns(signal[ch],first,end,width);
            assert(out.size()<=width);
            totalColumns+=out.size();
            for(const auto& e:out)if(e.valid){
                assert(e.first.index>=first && e.last.index<end);
                assert(e.low.index>=e.first.index && e.low.index<=e.last.index);
                assert(e.high.index>=e.first.index && e.high.index<=e.last.index);
            }
        }
    }
    for(std::size_t ch=0;ch<4;++ch){
        auto out=indexes[ch].columns(signal[ch],0,n,820);
        bool found=false;
        for(const auto& e:out)found|=e.valid && e.high.index==632117;
        assert(found);
        auto detail=indexes[ch].visible(signal[ch],632110,632130,820);
        assert(detail.size()==20 && detail[7].value==5+ch);
    }
    const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count();
    std::cout << "NativeEnvelope 4x1M, 120 multi-view sweeps, " << totalColumns
              << " indexed display columns, " << ms << " ms [diagnostic; not hardware GUI timing] PASS\n";
}
