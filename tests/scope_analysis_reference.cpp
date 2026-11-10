// Built using standard C++17 alone; proves FFT and amplitude correction independently of Qt.
#include "ScopeAnalysis.h"
#include <cmath>
#include <iostream>
#include <vector>
int main() {
    constexpr double pi=3.14159265358979323846;
    constexpr size_t N=8192;
    const double fs=8192.0;
    std::vector<double> x(N);
    for(size_t i=0;i<N;++i) x[i]=0.15+1.25*std::sin(2*pi*128*i/fs);
    for(auto w:{"Hann","Rectangular","Blackman"}) {
        const auto s=ScopeAnalysis::fft(x.data(),x.size(),fs,N,w);
        if(std::abs(s.peakFrequencyHz-128.0)>0.1 ||
           std::abs(s.peakAmplitude-1.25)>0.015 ||
           std::abs(s.amplitudePeak[0]-0.15)>0.005 ||
           std::abs(s.amplitudeRms[128]-1.25/std::sqrt(2.))>0.015 ||
           s.frequencyHz.size()!=N/2+1) return 1;
        std::cout<<w<<" freq "<<s.peakFrequencyHz<<" amp "<<s.peakAmplitude<<" DC "<<s.amplitudePeak[0]<<"\n";
    }
    try {ScopeAnalysis::fft(x.data(),x.size(),fs,1000,"Hann");return 2;}
    catch(const std::invalid_argument&) {}
    return 0;
}
