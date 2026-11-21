/**
 * WFScript DSPCore — original educational algorithms (not FFT.js or Fili).
 * Demonstrates how self-contained ES modules can implement analysis without C++.
 * For laboratory education, not calibrated metrology or hard real-time DSP.
 */

/** Deterministic N-point synthetic voltage capture with seeded noise. */
export function generateSignal(spec) {
    const n = spec.samples,
        fs = spec.sampleRateHz,
        f = spec.toneHz;
    if (!Number.isInteger(n) || n < 8 || n > 8192 || n & (n - 1)) throw Error('FFT samples must be a power of two');
    if (!(fs > 2 * f && f > 0)) throw Error('Tone must be below Nyquist');
    const x = [],
        timeMs = [];
    let seed = (spec.seed >>> 0) || 1;
    function rand() {
        seed = (1664525 * seed + 1013904223) >>> 0;
        return seed / 4294967296;
    }
    for (let i = 0; i < n; i++) {
        const phase = 2 * Math.PI * f * i / fs;
        let signal = Math.sin(phase);
        if (spec.shape === 'Square') signal = Math.sin(phase) >= 0 ? 1 : -1;
        if (spec.shape === 'Triangle') signal = 2 / Math.PI * Math.asin(Math.sin(phase));
        const harmonic = (spec.harmonicsPct / 100) * Math.sin(3 * phase);
        const noise = (spec.noisePct / 100) * (2 * rand() - 1);
        x.push(signal + harmonic + noise);
        timeMs.push(1000 * i / fs);
    }
    return {
        values: x,
        timeMs
    };
}

/** Two-pole Butterworth lowpass using bilinear-transform biquad coefficients. */
export function biquadLowpass(samples, fs, cutoff) {
    if (!(fs > 0 && cutoff > 0 && cutoff < fs / 2)) throw Error('Invalid filter cutoff');
    const w = 2 * Math.PI * cutoff / fs;
    const c = Math.cos(w),
        s = Math.sin(w),
        alpha = s / Math.sqrt(2);
    const a0 = 1 + alpha;
    const coeff = {
        b0: (1 - c) / (2 * a0),
        b1: (1 - c) / a0,
        b2: (1 - c) / (2 * a0),
        a1: (-2 * c) / a0,
        a2: (1 - alpha) / a0
    };
    let x1 = 0,
        x2 = 0,
        y1 = 0,
        y2 = 0;
    const result = new Array(samples.length);
    for (let i = 0; i < samples.length; i++) {
        const x0 = samples[i];
        const y0 = coeff.b0 * x0 + coeff.b1 * x1 + coeff.b2 * x2 - coeff.a1 * y1 - coeff.a2 * y2;
        result[i] = y0;
        x2 = x1;
        x1 = x0;
        y2 = y1;
        y1 = y0;
    }
    return {
        samples: result,
        coefficients: coeff
    };
}

/** Radix-2 FFT of a real array with Hann window and coherent-gain compensation. */
export function fftAmplitude(signal, fs) {
    const n = signal.length;
    if (n < 8 || n > 8192 || (n & (n - 1))) throw Error('FFT length must be power of two: 8..8192');
    const re = new Array(n),
        im = new Array(n).fill(0);
    let windowSum = 0;
    for (let i = 0; i < n; i++) {
        const weight = 0.5 * (1 - Math.cos(2 * Math.PI * i / (n - 1)));
        re[i] = signal[i] * weight;
        windowSum += weight;
    }
    for (let i = 1, j = 0; i < n; i++) {
        let bit = n >> 1;
        for (; j & bit; bit >>= 1)j ^= bit;
        j ^= bit;
        if (i < j) {
            let t = re[i];
            re[i] = re[j];
            re[j] = t;
        }
    }
    for (let len = 2; len <= n; len <<= 1) {
        const angle = -2 * Math.PI / len;
        const wr0 = Math.cos(angle),
            wi0 = Math.sin(angle);
        for (let start = 0; start < n; start += len) {
            let wr = 1,
                wi = 0;
            for (let j = 0; j < len / 2; j++) {
                const a = start + j,
                    b = a + len / 2;
                const tr = wr * re[b] - wi * im[b],
                    ti = wr * im[b] + wi * re[b];
                re[b] = re[a] - tr;
                im[b] = im[a] - ti;
                re[a] += tr;
                im[a] += ti;
                const nr = wr * wr0 - wi * wi0;
                wi = wr * wi0 + wi * wr0;
                wr = nr;
            }
        }
    }
    const hz = [],
        amplitude = [],
        db = [];
    let peakIndex = 1;
    for (let k = 0; k <= n / 2; k++) {
        const mag = Math.hypot(re[k], im[k]) * (k === 0 || k === n / 2 ? 1 : 2) / windowSum;
        hz.push(k * fs / n);
        amplitude.push(mag);
        db.push(20 * Math.log10(Math.max(mag, 1e-7)));
        if (k > 0 && amplitude[k] > amplitude[peakIndex]) peakIndex = k;
    }
    return {
        hz,
        amplitude,
        db,
        peakHz: hz[peakIndex],
        peakAmplitude: amplitude[peakIndex],
        binHz: fs / n
    };
}

/** Steady-state filter magnitude (dB), evaluated on the complex unit circle. */
export function response(coeff, fs, maxHz, steps = 200) {
    const hz = [],
        db = [];
    for (let i = 0; i <= steps; i++) {
        const f = (i + 1) * maxHz / (steps + 1),
            w = 2 * Math.PI * f / fs;
        const c = Math.cos(w),
            s = -Math.sin(w),
            c2 = Math.cos(2 * w),
            s2 = -Math.sin(2 * w);
        const nr = coeff.b0 + coeff.b1 * c + coeff.b2 * c2,
            ni = coeff.b1 * s + coeff.b2 * s2;
        const dr = 1 + coeff.a1 * c + coeff.a2 * c2,
            di = coeff.a1 * s + coeff.a2 * s2;
        const mag = Math.hypot(nr, ni) / Math.max(1e-15, Math.hypot(dr, di));
        hz.push(f);
        db.push(20 * Math.log10(Math.max(mag, 1e-9)));
    }
    return {
        hz,
        db
    };
}

/** RMS of uniformly spaced samples. */
export function rms(samples) {
    if (!samples.length) throw Error('RMS requires samples');
    let sum = 0;
    for (const sample of samples) sum += sample * sample;
    return Math.sqrt(sum / samples.length);
}
