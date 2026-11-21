#pragma once
// Digital timing transition index. Built when new samples arrive, not on each
// paint. Sorted finite series retain exact step edges near zoom, and a
// bounded high/low occupancy envelope at dense scale (narrow pulses survive).
// Does not require Qt and does not change the source data or protocol decoder.
#include "PlotPixelColumns.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

struct DigitalTransition {double x=0; bool high=false;};
struct DigitalPixelColumn {
    double x0=0,x1=0;
    bool low=false,high=false,lastHigh=false;
};
class DigitalEdgeIndex {
public:
    template<class X,class Y> void build(const X& x,const Y& y) {
        m_events.clear();m_ok=false;m_count=0;m_first=0;m_last=0;
        const std::size_t n=std::min(std::size_t(x.size()),std::size_t(y.size()));
        if(!n || x.size()!=y.size())return;
        m_events.reserve(std::min(n,std::size_t(65536)));
        double prev=0;bool prior=false;
        for(std::size_t i=0;i<n;++i){
            const double xx=x[i], yy=y[i];
            if(!std::isfinite(xx)||!std::isfinite(yy)||(i && xx<prev)){
                m_events.clear();return; // Keep the old faithful path for gaps/unsorted series.
            }
            const bool state=yy>=0.5;
            if(!i || state!=prior)m_events.push_back({xx,state});
            if(!i)m_first=xx;
            prev=xx;prior=state;
        }
        m_last=prev;m_count=n;m_ok=true;
    }
    template<class X,class Y> void append(const X& x,const Y& y,std::size_t oldCount){
        const std::size_t n=std::min(std::size_t(x.size()),std::size_t(y.size()));
        if(!m_ok || oldCount!=m_count || n<oldCount || x.size()!=y.size()){
            build(x,y);return;
        }
        double prev=m_last;
        bool state=m_events.back().high;
        for(std::size_t i=oldCount;i<n;++i){
            const double xx=x[i], yy=y[i];
            if(!std::isfinite(xx)||!std::isfinite(yy)||xx<prev){
                m_ok=false;m_events.clear();m_count=n;return;
            }
            const bool current=yy>=0.5;
            if(current!=state)m_events.push_back({xx,current});
            state=current;prev=xx;
        }
        m_last=prev;m_count=n;
    }
    bool valid()const{return m_ok;}
    double firstX()const{return m_first;}
    double lastX()const{return m_last;}
    std::size_t eventCount()const{return m_events.size();}
    const std::vector<DigitalTransition>& transitions()const{return m_events;}
    bool stateAt(double x)const {
        if(m_events.empty())return false;
        const auto it=std::upper_bound(m_events.begin(),m_events.end(),x,
            [](double xx,const DigitalTransition& e){return xx<e.x;});
        return it==m_events.begin()?m_events.front().high:(it-1)->high;
    }
    std::size_t transitionsIn(double xmin,double xmax)const {
        if(!m_ok || xmin>xmax)return 0;
        const auto lo=std::lower_bound(m_events.begin(),m_events.end(),xmin,
            [](const DigitalTransition& e,double xx){return e.x<xx;});
        const auto hi=std::upper_bound(lo,m_events.end(),xmax,
            [](double xx,const DigitalTransition& e){return xx<e.x;});
        return std::size_t(hi-lo);
    }
    std::vector<DigitalPixelColumn> columns(double xmin,double xmax,std::size_t count)const {
        std::vector<DigitalPixelColumn> result;
        if(!m_ok || !count || !(xmin<xmax))return result;
        result.reserve(count);
        for(std::size_t col=0;col<count;++col){
            const double left=PlotPixelColumns::boundary(xmin,xmax,col,count);
            const double right=PlotPixelColumns::boundary(xmin,xmax,col+1,count);
            if(right<m_first || left>m_last)continue;
            DigitalPixelColumn bin;bin.x0=std::max(left,m_first);bin.x1=std::min(right,m_last);
            if(bin.x0>bin.x1)continue;
            // At most two binary searches per column, regardless of whether
            // this single pixel contains one edge or 100,000 transitions.
            const auto lo=std::lower_bound(m_events.begin(),m_events.end(),left,
                [](const DigitalTransition& e,double xx){return e.x<xx;});
            const auto hi=col+1==count?
                std::upper_bound(lo,m_events.end(),right,
                    [](double xx,const DigitalTransition& e){return xx<e.x;}):
                std::lower_bound(lo,m_events.end(),right,
                    [](const DigitalTransition& e,double xx){return e.x<xx;});
            bool state=lo==m_events.begin()?m_events.front().high:(lo-1)->high;
            if(state)bin.high=true;else bin.low=true;
            if(lo!=hi){
                state=(hi-1)->high;
                // All stored transitions after the first alternate binary
                // state. One transition shows both levels; no averaging.
                if(state)bin.high=true;else bin.low=true;
                if(hi-lo>1){bin.high=true;bin.low=true;}
            }
            bin.lastHigh=state;
            result.push_back(bin);
        }
        return result;
    }
private:
    std::vector<DigitalTransition> m_events;
    std::size_t m_count=0;
    double m_first=0,m_last=0;
    bool m_ok=false;
};
