/**
 * WFScript AC Laboratory: first-order passive RC circuit theory.
 * Educational mathematical model, not an AD3 hardware measurement.
 * Fraction.js is an embedded, separately licensed third-party module.
 */
import Fraction from './vendor/fraction.mjs';
const TAU = 2 * Math.PI;
export function parameters(R, Cnf) {
    if (!Number.isFinite(R) || R < 100 || R > 100000 || !Number.isFinite(Cnf) || Cnf < 1 || Cnf > 1000)
        throw Error('Use R=100..100000 ohm and C=1..1000 nF');
    const C = Cnf * 1e-9;
    return {
        R,
        C,
        Cnf,
        tc: R * C,
        fc: 1 / (TAU * R * C)
    };
}
/**
 * Transfer function of a first-order RC low-pass or high-pass network.
 * Evaluate H(j*omega) at omega=2*pi*f; report magnitude and phase without
 * requiring physical samples. Compare separately with CaptureAnalysis.mjs.
 */
export function transfer(type, R, Cnf, f) {
    const p = parameters(R, Cnf);
    if (!Number.isFinite(f) || f <= 0 || f > 2000000) throw Error('Frequency outside theory range');
    if (type !== 'Low-pass' && type !== 'High-pass') throw Error('Unknown topology');
    const x = TAU * f * p.tc;
    // H_LP = 1/(1+jx); H_HP = jx/(1+jx)
    const re = type === 'Low-pass' ? 1 / (1 + x * x) : x * x / (1 + x * x);
    const im = type === 'Low-pass' ? -x / (1 + x * x) : x / (1 + x * x);
    const gain = Math.hypot(re, im);
    return {
        re,
        im,
        gain,
        db: 20 * Math.log10(Math.max(gain, 1e-15)),
        phaseDeg: Math.atan2(im, re) * 180 / Math.PI,
        fc: p.fc,
        tau: p.tc,
        impedanceOhm: 1 / (TAU * f * p.C)
    };
}
export function makeBode(type, R, Cnf, minF, maxF, count) {
    const hz = [],
        db = [],
        phase = [];
    for (let i = 0; i < count; i++) {
        const f = minF * Math.pow(maxF / minF, i / (count - 1));
        const h = transfer(type, R, Cnf, f);
        hz.push(f);
        db.push(h.db);
        phase.push(h.phaseDeg);
    }
    return {
        hz,
        db,
        phase
    };
}
export function makeTime(type, R, Cnf, f, amplitude, count) {
    const h = transfer(type, R, Cnf, f),
        x = [],
        vin = [],
        vout = [];
    const total = 4 / f;
    // Four periods
    for (let i = 0; i < count; i++) {
        const t = i * total / (count - 1),
            w = TAU * f * t;
        x.push(t * 1000);
        vin.push(amplitude * Math.sin(w));
        vout.push(amplitude * h.gain * Math.sin(w + h.phaseDeg * Math.PI / 180));
    }
    return {
        ms: x,
        vin,
        vout
    };
}
export function nominalSampleRatio(rateHz, fHz) {
    if (!Number.isInteger(rateHz) || !Number.isInteger(fHz) || rateHz <= 0 || fHz <= 0) return 'n/a';
    return new Fraction(rateHz, fHz).toFraction();
}
export function sweepFrequencies(fc, minF, maxF) {
    const raw = [0.15, 0.3, 0.5, 0.75, 1, 1.5, 2.5, 4, 7].map(x => Math.round(fc * x));
    const items = [...new Set(raw.filter(x => x >= minF && x <= maxF))].sort((a, b) => a - b);
    if (items.length < 4) throw Error('Need at least four sweep frequencies in safe range; adjust R/C');
    return items;
}
