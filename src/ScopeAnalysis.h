#pragma once

#include <complex>
#include <cstddef>
#include <string>
#include <vector>

// Standalone DSP implementation, independent of Qt and of the DWF SDK.
// All spectra are single-sided; amplitudes are in volts PEAK (except DC).
namespace ScopeAnalysis {
struct Spectrum {
    std::vector<double> frequencyHz, amplitudePeak, amplitudeRms, powerV2, decibels;
    double binHz=0, peakFrequencyHz=0, peakAmplitude=0;
    size_t fftSize=0;
};
// N must be a power of two between 32 and 65536 and within available samples.
Spectrum fft(const double* samples, size_t available, double sampleRate,
             size_t fftSize, const std::string& window);
}
