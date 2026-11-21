/**
 * RCStepAnalysis.mjs — transparent, pure-JavaScript RC step response laboratory.
 * A REAL acquisition can be analyzed by this module; a MODEL is generated only
 * by syntheticCapture(). The returned source is always supplied by the caller.
 *
 * For every COMPLETE half-cycle found from CH1 edges, fit the measured CH2
 * waveform to v(t)=Vinf+A exp(-t/tau). Tau is nonlinear; Vinf and A are linear
 * at a fixed tau. We search one dimension (log(tau)), and analytically solve
 * the two linear least-squares coefficients at each trial tau.
 * No external optimizer or hidden native DSP function is used.
 */
const TWO_PI = 2 * Math.PI;
/**
 * THEORY: a series R / shunt C obeys RC*(dv/dt) + v = Vin.
 * Tau=R*C is in seconds. Cnf is nanofarads, hence the 1e-9 conversion.
 * The cutoff frequency in hertz is 1/(2*pi*tau).
 */
export function theory(R, Cnf, fHz) {
    if (!(Number.isFinite(R) && R >= 100 && R <= 100000 && Number.isFinite(Cnf) && Cnf >= 1 && Cnf <= 1000))
        throw Error('R=100..100000 ohm; C=1..1000 nF required');
    const tau = R * Cnf * 1e-9,
        fc = 1 / (TWO_PI * tau);
    if (!(Number.isFinite(fHz) && fHz >= 1 && fHz <= 2000)) throw Error('Step frequency must be 1..2000 Hz');
    const half = 0.5 / fHz;
    return {
        R,
        Cnf,
        tau,
        fc,
        fHz,
        halfPeriodS: half,
        halfPeriodsPerTau: half / tau,
        settle5TauS: 5 * tau,
        decayAfterHalf: Math.exp(-half / tau)
    };
}
/** Exact periodic steady-state response for a symmetric +/-V square wave. */
/**
 * MATHEMATICAL MODEL: produce a steady-periodic capacitor response to a
 * +/-V square wave. MODEL output is NOT measurement data.
 * Optional deterministic pseudo-random voltage noise makes exercises repeat.
 */
export function syntheticCapture(options = {}) {
    const R = Number((options.R === undefined ? 980 : options.R)),
        Cnf = Number((options.Cnf === undefined ? 571 : options.Cnf)),
        fHz = Number((options.fHz === undefined ? 100 : options.fHz));
    const Vpk = Number((options.amplitudeVpk === undefined ? 0.5 : options.amplitudeVpk)),
        noiseRmsMv = Number((options.noiseRmsMv === undefined ? 0 : options.noiseRmsMv));
    const count = Number((options.count === undefined ? 8192 : options.count)),
        rateHz = Number((options.rateHz === undefined ? 250000 : options.rateHz));
    if (!Number.isInteger(count) || count < 1024 || count > 16384 || !(rateHz > 10 * fHz) ||
        !(Vpk >= 0.2 && Vpk <= 1) || !(noiseRmsMv >= 0 && noiseRmsMv <= 50)) throw Error('Invalid synthetic capture settings');
    const p = theory(R, Cnf, fHz),
        half = p.halfPeriodS,
        q = Math.exp(-half / p.tau);
    const edgeFirst = 0.25 / fHz;
    const upStart = -Vpk * (1 - q) / (1 + q),
        downStart = -upStart;
    const ch1 = [],
        ch2 = [];
    let rng = 34987;
    function uniform() {
        rng = (Math.imul(1664525, rng) + 1013904223) >>> 0;
        return (rng + 0.5) / 4294967296;
    }
    function noise() {
        return noiseRmsMv * 1e-3 * Math.sqrt(-2 * Math.log(uniform())) * Math.cos(TWO_PI * uniform());
    }
    for (let i = 0; i < count; i++) {
        const t = i / rateHz,
            part = Math.floor((t - edgeFirst) / half);
        // Before the first rising transition, the preceding low half-cycle is already in steady state.
        let vin,
            vout;
        if (t < edgeFirst) {
            vin = -Vpk;
            vout = -Vpk + (downStart + Vpk) * Math.exp(-(t + half - edgeFirst) / p.tau);
        } else {
            const age = t - (edgeFirst + part * half);
            const rising = part % 2 === 0;
            vin = rising ? Vpk : -Vpk;
            vout = vin + ((rising ? upStart : downStart) - vin) * Math.exp(-age / p.tau);
        }
        ch1.push(vin + noise());
        ch2.push(vout + noise());
    }
    return {
        source: 'MODEL',
        ch1,
        ch2,
        rateHz,
        fHz,
        R,
        Cnf,
        amplitudeVpk: Vpk,
        noiseRmsMv,
        description: 'SYNTHETIC: exact steady-state first-order RC response plus deterministic generated noise'
    };
}
function finiteArray(a, name) {
    if (!a || typeof a.length !== 'number' || a.length < 1024 || a.length > 65536) throw Error(name + ' must have 1024..65536 samples');
    const o = new Array(a.length);
    for (let i = 0; i < a.length; i++) {
        const v = Number(a[i]);
        if (!Number.isFinite(v)) throw Error(name + ' non-finite sample ' + i);
        o[i] = v;
    }
    return o;
}
function quantile(sorted, q) {
    return sorted[Math.min(sorted.length - 1, Math.floor(q * (sorted.length - 1)))];
}
/**
 * FIND THE TIME ORIGIN for each transient by detecting CH1 threshold crossings.
 * Interpolation between consecutive samples estimates the edge time. We use
 * CH1, not CH2, to locate the excitation so that response delay is measurable.
 */
function edgesFromInput(vin, rateHz, fHz) {
    const sorted = vin.slice().sort((a, b) => a - b);
    const lo = quantile(sorted, .10),
        hi = quantile(sorted, .90),
        swing = hi - lo;
    if (!(swing >= 0.15 && swing <= 3)) throw Error('CH1 input lacks a safe square-wave swing (>=0.15 V)');
    const mid = (hi + lo) / 2;
    const minSamples = Math.max(16, Math.floor(rateHz / fHz * .28));
    const edges = [];
    let last = -minSamples;
    for (let i = 1; i < vin.length; i++) {
        if (i - last < minSamples) continue;
        const a = vin[i - 1] - mid,
            b = vin[i] - mid;
        if ((a < 0 && b >= 0) || (a > 0 && b <= 0)) {
            const slope = vin[i] - vin[i - 1];
            if (Math.abs(slope) < 0.02) continue;
            // ignore chatter close to threshold
            const frac = Math.max(0, Math.min(1, (mid - vin[i - 1]) / slope));
            edges.push({
                index: i,
                timeS: (i - 1 + frac) / rateHz,
                direction: slope > 0 ? 'RISE' : 'FALL'
            });
            last = i;
        }
    }
    if (edges.length < 3) throw Error('Need >=3 CH1 transitions for complete rising and falling windows; adjust frequency/rate');
    return {
        edges,
        lo,
        hi,
        mid,
        swing
    };
}
/** A trial tau; solve [1, exp(-t/tau)] using centered 2x2 normal equations. */
/**
 * LEAST SQUARES AT A FIXED TIME CONSTANT.
 *
 *   y_i = Vinf + A*e_i, where e_i = exp(-t_i/tau).
 *   Unknowns Vinf and A enter linearly, so minimize sum(y_i - yhat_i)^2.
 *   The centered normal equations give:
 *
 *     A    = (sum(e*y)-sum(e)*sum(y)/N) /
 *            (sum(e*e)-sum(e)^2/N)
 *     Vinf = (sum(y)-A*sum(e))/N
 *
 * This function evaluates *one trial* tau and returns the residual SSE.
 * The outer optimizer changes tau; this inner calculation stays transparent.
 */
export function solveAtTau(samples, first, last, rateHz, tau) {
    if (!(tau > 0 && last - first >= 20)) throw Error('Invalid exponential fit window');
    let n = 0,
        sumE = 0,
        sumEE = 0,
        sumY = 0,
        sumEY = 0;
    for (let i = first; i < last; i++) {
        const e = Math.exp(-(i - first) / (rateHz * tau)),
            y = samples[i];
        n++;
        sumE += e;
        sumEE += e * e;
        sumY += y;
        sumEY += e * y;
    }
    const ssE = sumEE - sumE * sumE / n;
    if (!(ssE > 1e-9)) return null;
    const amplitude = (sumEY - sumE * sumY / n) / ssE;
    const finalV = (sumY - amplitude * sumE) / n;
    let sse = 0,
        sst = 0,
        peak = 0;
    const mean = sumY / n;
    for (let i = first; i < last; i++) {
        const pred = finalV + amplitude * Math.exp(-(i - first) / (rateHz * tau));
        const diff = samples[i] - pred;
        sse += diff * diff;
        sst += (samples[i] - mean) ** 2;
        peak = Math.max(peak, Math.abs(diff));
    }
    return {
        tauS: tau,
        finalV,
        amplitude,
        initialV: finalV + amplitude,
        sse,
        n,
        rmseV: Math.sqrt(sse / n),
        rSquared: sst > 0 ? 1 - sse / sst : null,
        peakResidualV: peak
    };
}
/** Minimize SSE over log(tau), with linear Vinf, A estimated at each trial. */
/**
 * NONLINEAR PARAMETER SEARCH, BUT ONLY IN ONE DIMENSION.
 *
 * Start with coarse log-spaced tau candidates, then narrow the best interval
 * using a golden-section search. Every candidate uses solveAtTau() to find
 * its own best Vinf and A. This avoids needing a black-box optimizer.
 *
 * The 63.2% threshold is inferred from the fitted Vinf and A; this makes its
 * crossing-time estimate a useful check, but not a fully independent method.
 */
export function fitTransition(samples, first, last, rateHz, direction) {
    if (last - first < 80) throw Error('Too few usable samples in step transition');
    const duration = (last - first) / rateHz;
    const minTau = 2 / rateHz,
        maxTau = duration * 0.75;
    if (maxTau <= minTau * 3) throw Error('Step window too short');
    const L = Math.log(minTau),
        H = Math.log(maxTau),
        count = 56;
    let best = null,
        bestI = 0;
    for (let i = 0; i <= count; i++) {
        const trial = solveAtTau(samples, first, last, rateHz, Math.exp(L + (H - L) * i / count));
        if (trial && (!best || trial.sse < best.sse)) {
            best = trial;
            bestI = i;
        }
    }
    if (!best) throw Error('No identifiable exponential fit');
    let left = L + (H - L) * Math.max(0, bestI - 1) / count;
    let right = L + (H - L) * Math.min(count, bestI + 1) / count;
    const golden = (Math.sqrt(5) - 1) / 2;
    let x1 = right - golden * (right - left),
        x2 = left + golden * (right - left);
    let q1 = solveAtTau(samples, first, last, rateHz, Math.exp(x1));
    let q2 = solveAtTau(samples, first, last, rateHz, Math.exp(x2));
    for (let i = 0; i < 42; i++) {
        if (!q2 || (q1 && q1.sse <= q2.sse)) {
            right = x2;
            x2 = x1;
            q2 = q1;
            x1 = right - golden * (right - left);
            q1 = solveAtTau(samples, first, last, rateHz, Math.exp(x1));
        }
        else {
            left = x1;
            x1 = x2;
            q1 = q2;
            x2 = left + golden * (right - left);
            q2 = solveAtTau(samples, first, last, rateHz, Math.exp(x2));
        }
    }
    let fitted = [best, q1, q2].filter(Boolean).sort((a, b) => a.sse - b.sse)[0];
    if (!(fitted && Math.abs(fitted.amplitude) >= 0.06)) throw Error('CH2 exponential amplitude too small; verify capacitor node');
    if ((direction === 'RISE' && fitted.amplitude >= 0) || (direction === 'FALL' && fitted.amplitude <= 0))
        throw Error('CH2 direction opposite CH1 transition; verify connections');
    // Crossing calculation happens in analyzeCapture(), where the CH1 edge
    // instant is available. The threshold below is for the fitted sample origin.
    const threshold = fitted.finalV + fitted.amplitude / Math.E;
    let crossingS = null;
    for (let i = first + 1; i < last; i++) {
        const prev = samples[i - 1] - threshold,
            cur = samples[i] - threshold;
        if ((direction === 'RISE' && prev < 0 && cur >= 0) || (direction === 'FALL' && prev > 0 && cur <= 0)) {
            const denominator = samples[i] - samples[i - 1];
            if (Math.abs(denominator) > 1e-12) {
                const frac = (threshold - samples[i - 1]) / denominator;
                crossingS = (i - 1 + frac) / rateHz;
            }
            break;
        }
    }
    // The fit is a new per-transition result. Append metadata directly rather
    // than using object-spread syntax, which fails under the target QJSEngine.
    fitted.first = first;
    fitted.last = last;
    fitted.direction = direction;
    fitted.thresholdV = threshold;
    fitted.crossing63S = crossingS;
    return fitted;
}
/** Reconstruct one CH2 voltage using the fitted exponential coefficients. */
export function predict(fit, i, rateHz) {
    return fit.finalV + fit.amplitude * Math.exp(-(i - fit.first) / (rateHz * fit.tauS));
}
/**
 * COMPLETE EXPERIMENT PIPELINE.
 * Validate genuine paired arrays -> detect input edges -> fit each full
 * rise/fall window -> average fitted tau -> infer fc = 1/(2*pi*tau).
 *
 * Model and hardware use identical analysis. Input `source` must identify
 * where the samples came from; the module never invents physical samples.
 */
export function analyzeCapture(capture) {
    const ch1 = finiteArray(capture.ch1, 'CH1'),
        ch2 = finiteArray(capture.ch2, 'CH2');
    if (ch1.length !== ch2.length) throw Error('Scope channels must have paired equal-length samples');
    const rateHz = Number(capture.rateHz),
        fHz = Number(capture.fHz);
    if (!(Number.isFinite(rateHz) && rateHz >= 10000 && rateHz <= 10000000 && Number.isFinite(fHz) && fHz >= 20 && fHz <= 200))
        throw Error('Invalid acquired sample rate or step frequency');
    if (rateHz / fHz < 60) throw Error('Need at least 60 samples per commanded cycle');
    const e = edgesFromInput(ch1, rateHz, fHz);
    const transitions = [];
    for (let k = 0; k < e.edges.length - 1; k++) {
        const edge = e.edges[k],
            next = e.edges[k + 1];
        if (edge.direction === next.direction) continue;
        // Start 3 samples after CH1 threshold crossing to avoid edge slew / ringing.
        const first = edge.index + 3,
            last = next.index - 3;
        if (last - first < 100) continue;
        // Reject half periods implausibly short/long versus commanded frequency.
        const half = (next.timeS - edge.timeS),
            target = 0.5 / fHz;
        if (half < target * .65 || half > target * 1.35) continue;
        const fit = fitTransition(ch2, first, last, rateHz, edge.direction);
        // 63% method must refer to the INPUT edge time, not the fit window's
        // first sample. If data are noisy it is only a cross-check, not a fit input.
        // Convert the fitted A at the first analysis sample back to the observed
        // CH1 edge instant. Only then is 63.2% relative to the physical step.
        const edgeAmplitude = fit.amplitude * Math.exp((first / rateHz - edge.timeS) / fit.tauS);
        const edgeThreshold = fit.finalV + edgeAmplitude / Math.E;
        let cross = null;
        for (let i = first + 1; i < last; i++) {
            const before = ch2[i - 1] - edgeThreshold,
                after = ch2[i] - edgeThreshold;
            if ((edge.direction === 'RISE' && before < 0 && after >= 0) ||
                (edge.direction === 'FALL' && before > 0 && after <= 0)) {
                const slope = ch2[i] - ch2[i - 1];
                if (Math.abs(slope) > 1e-12) cross = (i - 1 + (edgeThreshold - ch2[i - 1]) / slope) / rateHz;
                break;
            }
        }
        const tau63 = cross === null ? null : cross - edge.timeS;
        fit.edgeTimeS = edge.timeS;
        fit.edgeIndex = edge.index;
        fit.tau63S = tau63 !== null && tau63 > 0 && tau63 < half ? tau63 : null;
        fit.halfPeriodS = half;
        transitions.push(fit);
    }
    const rises = transitions.filter(x => x.direction === 'RISE'),
        falls = transitions.filter(x => x.direction === 'FALL');
    if (!rises.length || !falls.length) throw Error('A complete rising and falling transition are both required');
    function avg(xs, key) {
        return xs.reduce((t, x) => t + x[key], 0) / xs.length;
    }
    const riseTau = avg(rises, 'tauS'),
        fallTau = avg(falls, 'tauS'),
        tau = avg(transitions, 'tauS');
    const tau63values = transitions.map(x => x.tau63S).filter(x => x !== null);
    const pooledRMSE = Math.sqrt(transitions.reduce((sum, x) => sum + x.sse, 0) / transitions.reduce((sum, x) => sum + x.n, 0));
    const p = theory(Number(capture.R), Number(capture.Cnf), fHz);
    const phys = capture.source === 'AD3';
    return {
        source: phys ? 'AD3' : 'MODEL',
        device: phys ? String(capture.device || 'AD3') : 'SYNTHETIC',
        R: p.R,
        Cnf: p.Cnf,
        rateHz,
        fHz,
        count: ch1.length,
        inputSwingV: e.swing,
        theoryTauS: p.tau,
        theoryFcHz: p.fc,
        fitTauS: tau,
        riseTauS: riseTau,
        fallTauS: fallTau,
        fitFcHz: 1 / (TWO_PI * tau),
        tau63S: tau63values.length ? tau63values.reduce((a, b) => a + b, 0) / tau63values.length : null,
        residualRmsV: pooledRMSE,
        relativeResidualPct: 100 * pooledRMSE / e.swing,
        errorPct: 100 * (tau / p.tau - 1),
        transitions,
        edgeCount: e.edges.length,
        edges: e.edges,
        ch1,
        ch2,
        halfPeriodTau: p.halfPeriodsPerTau
    };
}
