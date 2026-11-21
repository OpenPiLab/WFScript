#include "../src/plot/NativeSampleIndex.h"
#ifdef NDEBUG
#undef NDEBUG // smoke assertions must run in Windows Release too
#endif
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>
int main(){
    constexpr std::size_t n=1000000;
    std::vector<double> signal(n);
    for(std::size_t i=0;i<n;++i)signal[i]=std::sin(double(i)*.001)*0.8;
    signal[637351]=3.25;signal[900701]=-4.5;
    NativeSampleIndex index;index.build(signal);
    assert(index.valid() && index.blockCount()==15625);
    assert(index.minimum()==-4.5 && index.maximum()==3.25);
    const auto all=index.visible(signal,0,n,800);
    assert(all.size()<=1600);
    bool high=false,low=false;
    for(std::size_t i=0;i<all.size();++i){
        assert(i==0 || all[i].index>all[i-1].index);
        high|=all[i].index==637351 && all[i].value==3.25;
        low|=all[i].index==900701 && all[i].value==-4.5;
    }
    assert(high&&low);
    auto zoom=index.visible(signal,637345,637366,800);
    assert(zoom.size()==21);
    for(std::size_t i=0;i<zoom.size();++i){assert(zoom[i].index==637345+i);assert(zoom[i].value==signal[637345+i]);}
    const auto columns=index.columns(signal,0,n,800);
    assert(columns.size()==800);
    bool spikeHigh=false,spikeLow=false;
    for(const auto& e:columns){
        assert(e.valid && e.first.index<=e.last.index);
        assert(e.low.index>=e.first.index && e.low.index<=e.last.index);
        assert(e.high.index>=e.first.index && e.high.index<=e.last.index);
        if(e.hasPositive)assert(e.positiveLow.value>0);
        spikeHigh|=(e.high.index==637351 && e.high.value==3.25);
        spikeLow|=(e.low.index==900701 && e.low.value==-4.5);
    }
    assert(spikeHigh && spikeLow);
    // Weighted mean remains the true mean of all finite samples even when
    // column bounds merge many pre-indexed blocks and uneven end fragments.
    for(std::size_t col=0;col<columns.size();++col){
        const std::size_t a=(n*col)/columns.size();
        const std::size_t b=(n*(col+1))/columns.size();
        double sum=0;
        for(std::size_t i=a;i<b;++i)sum+=signal[i];
        assert(columns[col].count==b-a);
        assert(std::abs(columns[col].sum/double(columns[col].count)-sum/double(b-a))<1e-11);
    }
    const auto empty=index.columns(signal,n,n+1,800);
    assert(empty.empty());
    // Log Y still selects the smallest positive sample in a column
    // instead of dropping it because a negative minimum exists.
    const std::vector<double> mixed={-3.0,2.0,0.05,-4.0,1.5};
    NativeSampleIndex mixedIndex;mixedIndex.build(mixed);
    const auto mixedColumns=mixedIndex.columns(mixed,0,mixed.size(),1);
    assert(mixedColumns.size()==1 && mixedColumns[0].hasPositive);
    assert(mixedColumns[0].positiveLow.index==2 && mixedColumns[0].positiveLow.value==0.05);
    assert(mixedColumns[0].count==5);
    assert(std::abs(mixedColumns[0].sum-(-3+2+.05-4+1.5))<1e-12);
    // Invalid input does not poison the mean or extrema.
    const std::vector<double> mixedInvalid={1.0,std::numeric_limits<double>::quiet_NaN(),3.0};
    NativeSampleIndex nanIndex;nanIndex.build(mixedInvalid);
    const auto nanCol=nanIndex.columns(mixedInvalid,0,mixedInvalid.size(),1);
    assert(nanCol.size()==1 && nanCol[0].count==2 && nanCol[0].sum==4.0);
    auto falling=index.visible(signal,3120,9400,400);
    for(std::size_t i=1;i<falling.size();++i)assert(falling[i-1].index<falling[i].index);
    std::cout << "NativeSampleIndex 1M extrema, chronology, zoom, spike retention PASS\n";
}
