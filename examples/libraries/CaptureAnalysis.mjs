/**
 * WFScript AC Laboratory — transparent two-channel least-squares sinusoid fitting.
 *
 * At a KNOWN commanded frequency f, fit v[i] = d + a*sin(w*i/fs) + b*cos(w*i/fs).
 * The three unknowns (d,a,b) minimize the sum of squared sample residuals.
 * Both channels use the SAME actual acquisition sample rate and time origin.
 *
 * Diagnostic RMS residuals describe deviation from the single-frequency model;
 * they are NOT calibrated accuracy, frequency uncertainty, or an SNR estimate.
 */
/**
 * THE LINEAR FIT EXPLAINED:
 *   y_i = DC + a*sin(2*pi*f*t_i) + b*cos(2*pi*f*t_i)
 * Accumulate the normal-equation sums without assuming an integer number of
 * signal cycles, eliminate DC, and solve the remaining 2-by-2 system.
 *
 * A = hypot(a,b) and phase = atan2(b,a), measured in radians.
 * The code fits voltage samples; no SDK-generated gain/phase data are used.
 */
export function fitSinusoid(samples, rate, f) {
    const n = samples.length;
    if (n < 128 || !(Number.isFinite(rate) && Number.isFinite(f)) || !(rate > 2 * f) || !(f > 0))
        throw Error('Invalid scope sample count or rate');
    let n0 = 0,
        s0 = 0,
        c0 = 0,
        ss = 0,
        cc = 0,
        sc = 0,
        y0 = 0,
        ys = 0,
        yc = 0;
    const step = 2 * Math.PI * f / rate;
    // X has columns [1, sin(w*t), cos(w*t)]. Accumulate X^T X and X^T y.
    for (let i = 0; i < n; i++) {
        const y = Number(samples[i]);
        if (!Number.isFinite(y)) throw Error('Non-finite Scope sample ' + i);
        const s = Math.sin(step * i),
            c = Math.cos(step * i);
        n0++;
        s0 += s;
        c0 += c;
        ss += s * s;
        cc += c * c;
        sc += s * c;
        y0 += y;
        ys += y * s;
        yc += y * c;
    }
    // Remove the fitted DC term analytically: 3x3 normal equations -> 2x2.
    const a = ss - s0 * s0 / n0,
        b = sc - s0 * c0 / n0,
        d = cc - c0 * c0 / n0;
    const rhsS = ys - s0 * y0 / n0,
        rhsC = yc - c0 * y0 / n0,
        det = a * d - b * b;
    if (!(det > 1e-9)) throw Error('Insufficient independent sine/cosine samples');
    // Solve the centered 2x2 system without an external numerical library.
    const sinCoef = (rhsS * d - rhsC * b) / det,
        cosCoef = (rhsC * a - rhsS * b) / det;
    const offset = (y0 - sinCoef * s0 - cosCoef * c0) / n0;
    const amplitude = Math.hypot(sinCoef, cosCoef);
    const angle = Math.atan2(cosCoef, sinCoef);
    // a=A*cos(phi), b=A*sin(phi)

    // New diagnostic pass: residual = actual sample - reconstructed sine.
    // Keeping this separate makes the existing coefficient solve easy to audit.
    let residualSSE = 0,
        totalSST = 0,
        peakResidual = 0;
    const mean = y0 / n0;
    for (let i = 0; i < n; i++) {
        const y = Number(samples[i]);
        const fitted = offset + sinCoef * Math.sin(step * i) + cosCoef * Math.cos(step * i);
        const residual = y - fitted;
        residualSSE += residual * residual;
        totalSST += (y - mean) * (y - mean);
        peakResidual = Math.max(peakResidual, Math.abs(residual));
    }
    const rmsResidualV = Math.sqrt(residualSSE / n);
    // RMS residual relative to fitted PEAK voltage, not a calibrated noise %.
    const residualPct = amplitude > 0 ? 100 * rmsResidualV / amplitude : null;
    const rSquared = totalSST > 0 ? 1 - residualSSE / totalSST : null;
    return {
        amplitude,
        angle,
        offset,
        sinCoef,
        cosCoef,
        rmsResidualV,
        residualPct,
        rSquared,
        peakResidualV: peakResidual,
        samples: n,
        cycles: n * f / rate
    };
}
/** Return v_hat[i] at exact sample index i, without changing captured samples. */
export function predictSample(fit, rate, f, i) {
    const theta = 2 * Math.PI * f * i / rate;
    return fit.offset + fit.sinCoef * Math.sin(theta) + fit.cosCoef * Math.cos(theta);
}
/**
 * Fit CH1 and CH2 separately with the same sampled clock. Divide amplitudes
 * for the linear gain and subtract phases for the voltage phase shift.
 * Sine-fit residuals quantify model mismatch, not independently verified
 * instrument accuracy.
 */
export function analyzeTwoChannel(input, output, rate, f, options) {
    // Only the explicitly marked synthetic explorer may bypass PHYSICAL signal gates.
    const isSynthetic = options && options.synthetic === true;
    const n = Math.min(input.length, output.length);
    if (n < 512 || n > 65536) throw Error('Scope needs 512..65536 paired samples');
    // Explicit copies avoid runtime-specific QVector<double> iterator assumptions.
    const a = Array.from({ length: n }, (_, i) => Number(input[i]));
    const b = Array.from({ length: n }, (_, i) => Number(output[i]));
    const A = fitSinusoid(a, rate, f),
        B = fitSinusoid(b, rate, f);
    if (!isSynthetic && (A.amplitude < 0.08 || A.amplitude > 1.6)) throw Error('CH1 input too small or too large: ' + A.amplitude.toFixed(3) + ' Vpk');
    if (!isSynthetic && B.amplitude < 0.002) throw Error('CH2 output below useful measurement floor');
    if (!(A.amplitude > 0)) throw Error('Zero-amplitude input cannot define gain');
    const gain = B.amplitude / A.amplitude;
    if (!isSynthetic && gain > 1.20) throw Error('Unexpected gain >1.20; check connections and ranges');
    let phase = (B.angle - A.angle) * 180 / Math.PI;
    while (phase > 180) phase -= 360;
    while (phase <= -180) phase += 360;
    return {
        inputVpk: A.amplitude,
        outputVpk: B.amplitude,
        inputOffset: A.offset,
        outputOffset: B.offset,
        gain,
        db: 20 * Math.log10(gain),
        phaseDeg: phase,
        count: n,
        rateHz: rate,
        inputFit: A,
        outputFit: B
    };
}
export function timePreview(input, output, rate, maxCount) {
    const n = Math.min(input.length, output.length),
        step = Math.max(1, Math.floor(n / maxCount));
    const ms = [],
        vin = [],
        vout = [];
    for (let i = 0; i < n; i += step) {
        ms.push(1000 * i / rate);
        vin.push(Number(input[i]));
        vout.push(Number(output[i]));
    }
    return {
        ms,
        vin,
        vout
    };
}
export function summary(rows) {
    const measured = rows.filter(r => r.source === 'AD3');
    if (!measured.length) return {
        count: 0,
        meanGainErrorPct: null,
        meanPhaseErrorDeg: null
    };
    return {
        count: measured.length,
        meanGainErrorPct: measured.reduce((s, r) => s + Math.abs(r.gain / r.predGain - 1) * 100, 0) / measured.length,
        meanPhaseErrorDeg: measured.reduce((s, r) => s + Math.abs(r.phaseDeg - r.predPhase), 0) / measured.length
    };
}
