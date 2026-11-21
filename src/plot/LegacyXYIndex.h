#pragma once
// RC1.5.2: indexed renderer support for the original plots.setSeries() /
// plots.setSamples() API. X may be irregularly spaced, and even duplicated.
// This is a GUI-thread-owned cache, not a new JavaScript sample representation.
// It is Qt-independent so irregular-X selection can be tested without Qt.
#include "NativeSampleIndex.h"
#include "PlotPixelColumns.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

struct LegacyXYExtent {
    bool valid=false;
    double xmin=0,xmax=0,ymin=0,ymax=0;
    void add(double x,double y){
        if(!valid){xmin=xmax=x;ymin=ymax=y;valid=true;return;}
        xmin=std::min(xmin,x);xmax=std::max(xmax,x);
        ymin=std::min(ymin,y);ymax=std::max(ymax,y);
    }
};
struct LegacyXYColumn {
    double x=0; // Original X position from this column (not a synthetic uniform X)
    NativeSampleExtrema envelope;
};
class LegacyXYIndex {
public:
    template<class X,class Y> void build(const X& x,const Y& y){
        clear();
        m_yIndex.build(y);
        appendMetadata(x,y,0);
    }
    template<class X,class Y> void append(const X& x,const Y& y,std::size_t previous){
        const std::size_t n=std::min(std::size_t(x.size()),std::size_t(y.size()));
        if(previous!=m_count || n<previous || x.size()!=y.size()){
            build(x,y);return;
        }
        m_yIndex.append(y,previous);
        appendMetadata(x,y,previous);
    }
    bool ascending() const{return m_ascending;}
    bool allFinite() const{return m_allFinite;}
    bool allXPositive() const{return m_allXPositive;}
    bool allYPositive() const{return m_allYPositive;}
    std::size_t count() const{return m_count;}
    const LegacyXYExtent& range(bool positiveX,bool positiveY) const {
        return m_extents[positiveX?1:0][positiveY?1:0];
    }
    bool rightRange(double& ymin,double& ymax) const {
        if(!m_rightValid)return false;
        ymin=m_rightMin;ymax=m_rightMax;return true;
    }
    template<class X> std::pair<std::size_t,std::size_t> visibleSpan(
        const X& x,double xmin,double xmax) const {
        if(!m_ascending || !std::isfinite(xmin) || !std::isfinite(xmax) || xmin>xmax)
            return {0,0};
        auto first=std::lower_bound(x.cbegin(),x.cbegin()+std::ptrdiff_t(m_count),xmin);
        auto last=std::upper_bound(first,x.cbegin()+std::ptrdiff_t(m_count),xmax);
        return {std::size_t(first-x.cbegin()),std::size_t(last-x.cbegin())};
    }
    template<class X,class Y> std::vector<LegacyXYColumn> columns(
        const X& x,const Y& y,double xmin,double xmax,std::size_t columnCount,bool logX=false) const {
        std::vector<LegacyXYColumn> out;
        if(!m_ascending || !m_allFinite || columnCount==0 || !(xmin<xmax) ||
           (logX && (xmin<=0 || xmax<=0)))return out;
        const auto visible=visibleSpan(x,xmin,xmax);
        if(visible.first>=visible.second)return out;
        const std::size_t columns=columnCount;
        out.reserve(columns);
                auto it=x.cbegin()+std::ptrdiff_t(visible.first);
        const auto last=x.cbegin()+std::ptrdiff_t(visible.second);
        for(std::size_t c=0;c<columns;++c){
            // Column boundaries are in real X coordinates. This matters for
            // variable-rate samples and nonuniform Network Analysis sweeps.
            const double bound=PlotPixelColumns::boundary(xmin,xmax,c+1,columns,logX);
            auto next=c+1==columns?last:std::lower_bound(it,last,bound);
            if(next==it)continue; // gap in sparse or nonuniform X values
            const std::size_t a=std::size_t(it-x.cbegin());
            const std::size_t b=std::size_t(next-x.cbegin());
            LegacyXYColumn col;
            col.x=PlotPixelColumns::center(xmin,xmax,c,columns,logX);
            col.envelope=m_yIndex.bounds(y,a,b);
            if(col.envelope.valid)out.push_back(col);
            it=next;
        }
        return out;
    }
private:
    NativeSampleIndex m_yIndex;
    LegacyXYExtent m_extents[2][2];
    std::size_t m_count=0;
    bool m_ascending=true,m_allFinite=true,m_allXPositive=true,m_allYPositive=true;
    bool m_rightValid=false;
    double m_rightMin=0,m_rightMax=0;
    double m_previousX=0;
    void clear(){
        m_extents[0][0]=m_extents[0][1]=m_extents[1][0]=m_extents[1][1]=LegacyXYExtent{};
        m_count=0;m_ascending=m_allFinite=m_allXPositive=m_allYPositive=true;
        m_rightValid=false;m_rightMin=m_rightMax=m_previousX=0;
    }
    template<class X,class Y> void appendMetadata(const X& x,const Y& y,std::size_t start){
        const std::size_t n=std::min(std::size_t(x.size()),std::size_t(y.size()));
        for(std::size_t i=start;i<n;++i){
            const double xx=x[i],yy=y[i];
            if(i>0 && !(m_previousX<=xx))m_ascending=false;
            m_previousX=xx;
            if(!std::isfinite(xx)||!std::isfinite(yy))m_allFinite=false;
            if(!(xx>0))m_allXPositive=false;
            if(!(yy>0))m_allYPositive=false;
            if(std::isfinite(yy)){
                if(!m_rightValid){m_rightMin=m_rightMax=yy;m_rightValid=true;}
                else {m_rightMin=std::min(m_rightMin,yy);m_rightMax=std::max(m_rightMax,yy);}
            }
            if(!std::isfinite(xx)||!std::isfinite(yy))continue;
            m_extents[0][0].add(xx,yy);
            if(xx>0)m_extents[1][0].add(xx,yy);
            if(yy>0)m_extents[0][1].add(xx,yy);
            if(xx>0&&yy>0)m_extents[1][1].add(xx,yy);
        }
        m_count=n;
    }
};
