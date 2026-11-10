#include "ScopeAnalysis.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ScopeAnalysis {
Spectrum fft(const double* data,size_t available,double rate,size_t n,const std::string& window) {
    if (!data || n<32 || n>65536 || (n&(n-1)) || available<n ||
        !std::isfinite(rate) || rate<=0)
        throw std::invalid_argument("FFT requires 32..65536 power-of-two samples and a positive sample rate");
    if(window!="Hann" && window!="Rectangular" && window!="Blackman")
        throw std::invalid_argument("FFT window must be Hann, Rectangular or Blackman");
    constexpr double pi=3.1415926535897932384626433832795;
    const size_t half=n/2;
    std::vector<std::complex<double>> x(n);
    double sumW=0;
    for(size_t i=0;i<n;++i){
        if(!std::isfinite(data[i])) throw std::invalid_argument("FFT cannot process non-finite samples");
        const double a=2*pi*double(i)/double(n-1);
        const double w=window=="Hann" ? 0.5-0.5*std::cos(a) :
            window=="Blackman" ? 0.42-0.5*std::cos(a)+0.08*std::cos(2*a) : 1.0;
        sumW+=w; x[i]=data[i]*w;
    }
    for(size_t i=1,j=0;i<n;++i){
        size_t bit=n>>1;
        for(;j&bit;bit>>=1)j^=bit;
        j^=bit;
        if(i<j)std::swap(x[i],x[j]);
    }
    for(size_t len=2;len<=n;len<<=1) {
        const double theta=-2*pi/double(len);
        const std::complex<double> step(std::cos(theta),std::sin(theta));
        for(size_t i=0;i<n;i+=len){
            std::complex<double> w(1,0);
            for(size_t j=0;j<len/2;++j){
                auto u=x[i+j],v=x[i+j+len/2]*w;
                x[i+j]=u+v; x[i+j+len/2]=u-v;w*=step;
            }
        }
    }
    Spectrum s; s.fftSize=n;s.binHz=rate/double(n);
    s.frequencyHz.reserve(half+1);s.amplitudePeak.reserve(half+1);
    s.amplitudeRms.reserve(half+1);s.powerV2.reserve(half+1);s.decibels.reserve(half+1);
    size_t best=0;double maximum=-1;
    for(size_t k=0;k<=half;++k){
        // Coherent-gain correction gives correct amplitude for bin-centered tones.
        // Non-bin-centered tones will exhibit scalloping loss.
        double amplitude=std::abs(x[k])/sumW*(k==0 || k==half ? 1.0:2.0);
        double rms=amplitude*(k==0 || k==half ? 1.0:1/std::sqrt(2.0));
        s.frequencyHz.push_back(k*s.binHz);
        s.amplitudePeak.push_back(amplitude);
        s.amplitudeRms.push_back(rms);
        s.powerV2.push_back(rms*rms);
        s.decibels.push_back(20*std::log10(std::max(amplitude,1.e-15)));
        if(k>0 && k<half && amplitude>maximum){maximum=amplitude;best=k;}
    }
    s.peakFrequencyHz=best*s.binHz;s.peakAmplitude=s.amplitudePeak[best];
    if(best>1 && best+1<half){
        const double a=std::log(std::max(s.amplitudePeak[best-1],1e-15));
        const double b=std::log(std::max(s.amplitudePeak[best],1e-15));
        const double c=std::log(std::max(s.amplitudePeak[best+1],1e-15));
        const double denominator=(a-2*b+c);
        if(std::abs(denominator)>1e-12){
            const double delta=std::clamp(0.5*(a-c)/denominator,-0.5,0.5);
            s.peakFrequencyHz=(double(best)+delta)*s.binHz;
        }
    }
    return s;
}
}
