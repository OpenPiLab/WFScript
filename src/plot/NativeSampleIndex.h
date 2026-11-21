#pragma once
// Render-only index for uniformly sampled native datasets. Keeps the original
// sample indices of every extrema, so falling slopes and narrow spikes cannot
// be reordered or lost by decimation. No Qt dependency; deterministic tests
// can exercise the exact renderer reduction without a GUI.
#include "PlotPixelColumns.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

struct NativeSamplePoint { std::size_t index=0; double value=0; };
struct NativeSampleExtrema {
    NativeSamplePoint low{}, high{}, first{}, last{}, positiveLow{};
    double sum=0.0;
    std::size_t count=0;
    bool valid=false, hasPositive=false;
};
struct NativePixelColumn { double x=0; NativeSampleExtrema envelope; };
class NativeSampleIndex {
public:
    static constexpr std::size_t blockSize=64;
    template<class Values> void build(const Values& values) {
        m_blocks.clear();m_min=0;m_max=0;m_minPositive=std::numeric_limits<double>::infinity();m_valid=false;
        const std::size_t n=std::size_t(values.size());
        m_sampleCount=n;
        m_blocks.reserve((n+blockSize-1)/blockSize);
        for(std::size_t begin=0;begin<n;begin+=blockSize){
            const std::size_t end=std::min(n,begin+blockSize);
            NativeSampleExtrema b;
            for(std::size_t i=begin;i<end;++i){
                add(b,i,values[i]);
                if(std::isfinite(values[i]) && values[i]>0)m_minPositive=std::min(m_minPositive,double(values[i]));
            }
            m_blocks.push_back(b);
            if(b.valid){
                if(!m_valid){m_min=b.low.value;m_max=b.high.value;m_valid=true;}
                else {m_min=std::min(m_min,b.low.value);m_max=std::max(m_max,b.high.value);}
            }
        }
    }
    // Incremental appends avoid rebuilding every 64-sample block when a script
    // streams values with plots.appendPoint()/appendSeries(). A front-trim at
    // the legacy 250k cap still requires an explicit full rebuild.
    template<class Values> void append(const Values& values,std::size_t previousCount){
        const std::size_t n=std::size_t(values.size());
        if(previousCount!=m_sampleCount || n<previousCount){build(values);return;}
        for(std::size_t i=previousCount;i<n;++i){
            if(i%blockSize==0)m_blocks.emplace_back();
            NativeSampleExtrema& block=m_blocks.back();
            add(block,i,values[i]);
            if(std::isfinite(values[i]) && values[i]>0)
                m_minPositive=std::min(m_minPositive,double(values[i]));
            if(std::isfinite(values[i])){
                if(!m_valid){m_min=m_max=values[i];m_valid=true;}
                else {m_min=std::min(m_min,double(values[i]));m_max=std::max(m_max,double(values[i]));}
            }
        }
        m_sampleCount=n;
    }
    bool valid() const {return m_valid;}
    double minimum() const {return m_min;}
    double maximum() const {return m_max;}
    double minimumPositive() const {return m_minPositive;}
    std::size_t blockCount() const {return m_blocks.size();}
    template<class Values> NativeSampleExtrema bounds(const Values& v,std::size_t first,std::size_t end) const {
        NativeSampleExtrema result;
        const std::size_t n=std::size_t(v.size());
        if(first>=n||first>=end)return result;
        end=std::min(end,n);
        while(first<end && first%blockSize!=0){add(result,first,v[first]);++first;}
        while(first+blockSize<=end){merge(result,m_blocks[first/blockSize]);first+=blockSize;}
        while(first<end){add(result,first,v[first]);++first;}
        return result;
    }
    // Dense display: at most one true extrema envelope per display column.
    // Its first/last samples let the renderer join adjacent columns without
    // creating a multi-thousand-element antialiased QPainterPath.
    template<class Values> std::vector<NativeSampleExtrema> columns(const Values& v,
                              std::size_t first,std::size_t end,std::size_t pixelColumns) const {
        std::vector<NativeSampleExtrema> result;
        const std::size_t n=std::size_t(v.size());
        if(first>=n || first>=end || pixelColumns==0)return result;
        end=std::min(end,n);
        const std::size_t count=end-first;
        const std::size_t columns=std::min(count,pixelColumns);
        result.reserve(columns);
        for(std::size_t col=0;col<columns;++col){
            const std::size_t a=first+(count*col)/columns;
            const std::size_t b=first+(count*(col+1))/columns;
            result.push_back(bounds(v,a,b));
        }
        return result;
    }
    // Unlike columns(), this bins a uniform record in the *visible axis* grid.
    // It uses the same boundaries and centers as LegacyXYIndex::columns().
    // Partial records don't shift/resize the bin grid when the user pans.
    template<class Values> std::vector<NativePixelColumn> columnsByView(
              const Values& v,double origin,double step,double xmin,double xmax,
              std::size_t pixelColumns,bool logX=false) const {
        std::vector<NativePixelColumn> result;
        if(v.size()==0 || !(step>0) || !std::isfinite(origin) ||
           !std::isfinite(step) || !(xmin<xmax) || !pixelColumns ||
           (logX && xmin<=0))return result;
        const std::size_t n=std::size_t(v.size());
        const auto lower=[&](double value,std::size_t from){
            std::size_t lo=from,hi=n;
            while(lo<hi){const std::size_t m=lo+(hi-lo)/2;
                if(origin+double(m)*step<value)lo=m+1;else hi=m;
            }
            return lo;
        };
        // Includes a point exactly on xmax, matching legacy upper_bound.
        const auto upper=[&](double value,std::size_t from){
            std::size_t lo=from,hi=n;
            while(lo<hi){const std::size_t m=lo+(hi-lo)/2;
                if(origin+double(m)*step<=value)lo=m+1;else hi=m;
            }
            return lo;
        };
        const std::size_t last=upper(xmax,0);
        std::size_t start=lower(xmin,0);
        if(start>=last)return result;
        result.reserve(pixelColumns);
        for(std::size_t c=0;c<pixelColumns;++c){
            const double edge=PlotPixelColumns::boundary(xmin,xmax,c+1,pixelColumns,logX);
            const std::size_t end=c+1==pixelColumns?last:
                std::min(last,lower(edge,start));
            if(end>start){
                NativePixelColumn col;
                col.x=PlotPixelColumns::center(xmin,xmax,c,pixelColumns,logX);
                col.envelope=bounds(v,start,end);
                if(col.envelope.valid)result.push_back(col);
            }
            start=end;
        }
        return result;
    }
    template<class Values> std::vector<NativeSamplePoint> visible(const Values& v,std::size_t first,
                                  std::size_t end,std::size_t pixelColumns) const {
        std::vector<NativeSamplePoint> points;
        const std::size_t n=std::size_t(v.size());
        if(first>=n || first>=end || pixelColumns==0)return points;
        end=std::min(end,n);const std::size_t count=end-first;
        // On zoom-in draw the actual samples, not a low-resolution preview.
        if(count<=pixelColumns*2) {
            points.reserve(count);
            for(std::size_t i=first;i<end;++i)
                if(std::isfinite(v[i]))points.push_back({i,v[i]});
            return points;
        }
        const std::size_t columns=std::min(count,pixelColumns);
        points.reserve(columns*2);
        for(std::size_t col=0;col<columns;++col){
            const std::size_t a=first+(count*col)/columns;
            const std::size_t b=first+(count*(col+1))/columns;
            const NativeSampleExtrema e=bounds(v,a,b);
            if(!e.valid)continue;
            if(e.low.index<e.high.index){points.push_back(e.low);points.push_back(e.high);}
            else if(e.high.index<e.low.index){points.push_back(e.high);points.push_back(e.low);}
            else points.push_back(e.low);
        }
        return points;
    }
private:
    std::vector<NativeSampleExtrema> m_blocks;
    std::size_t m_sampleCount=0;
    double m_min=0,m_max=0;
    double m_minPositive=std::numeric_limits<double>::infinity();
    bool m_valid=false;
    static void add(NativeSampleExtrema& dst,std::size_t i,double v){
        if(!std::isfinite(v))return;
        if(!dst.valid){
            dst.low={i,v};dst.high={i,v};dst.first={i,v};dst.last={i,v};dst.valid=true;
        } else {
            if(v<dst.low.value)dst.low={i,v};
            if(v>dst.high.value)dst.high={i,v};
            dst.last={i,v};
        }
        dst.sum+=v;
        ++dst.count;
        if(v>0 && (!dst.hasPositive || v<dst.positiveLow.value)){
            dst.positiveLow={i,v};dst.hasPositive=true;
        }
    }
    static void merge(NativeSampleExtrema& dst,const NativeSampleExtrema& src){
        if(!src.valid)return;
        if(!dst.valid){dst=src;return;}
        if(src.low.value<dst.low.value)dst.low=src.low;
        if(src.high.value>dst.high.value)dst.high=src.high;
        dst.last=src.last;
        dst.sum+=src.sum;
        dst.count+=src.count;
        if(src.hasPositive && (!dst.hasPositive || src.positiveLow.value<dst.positiveLow.value)){
            dst.positiveLow=src.positiveLow;dst.hasPositive=true;
        }
    }
};
