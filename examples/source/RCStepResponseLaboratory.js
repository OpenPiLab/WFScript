// ┌─ WFScript LEARNING LAB: RC Step Response and Time Constant
// │ GOAL: Connect the RC differential equation, exponential charge/discharge,
// │       least-squares time-constant fitting, and a real AD3 Scope capture.
// │ MODE: The MODEL is fully usable without hardware. AD3 is explicitly armed
// │       only by a button after verified wiring; W1 shuts off after capture.
// │ TRY FIRST: Page 03 -> RUN SYNTHETIC EXPERIMENT. No instrument is touched.
// │ TRACE: stepCapture.clicked -> onCaptureClicked() -> capturePhysical() ->
// │        Step.analyzeCapture() -> Step.fitTransition() -> Step.solveAtTau().
// │ EDIT SAFELY: Change the synthetic noise value and watch residuals change.
// │ NOTE: The supplied 980 ohm / 571 nF are operator-entered LCR values.
// │ FULL LAB GUIDE: examples/guides/RCStepResponseLaboratory.md
// └───────────────────────────────────────────────────────────────────────────

/**
 * RC Step Response Laboratory — v0.2-dev.8.9.3.2.
 * A separate, self-contained .wfpanel; no changes to the validated AC Lab.
 * SOURCES: 'MODEL' => generated ideal RC + optional deterministic noise;
 *          'AD3'   => actual two-channel physical Scope sample arrays.
 * SAFETY: W1 is not touched on startup, simulation, parameter adjustment or
 * page switching. A real output requires an explicit wiring toggle and user
 * click. W1 uses only Square, 0 V offset, 0.5 V peak; stops automatically.
 * This is an educational experiment, not calibrated metrology or an interlock.
 */
// READING MAP
//   Step.theory()          -> tau = R*C; cutoff = 1/(2*pi*tau).
//   runSynthetic()         -> generated circuit and controlled artificial noise.
//   onCaptureClicked()     -> operator validation, then physical W1 setup.
//   capturePhysical()      -> actual paired CH1 and CH2 AD3 samples.
//   Step.analyzeCapture()  -> edge detection and exponential fitting.
//   showCapture()          -> plots of samples, fitted curve and residuals.
//   addResult()            -> Notebook rows labeled MODEL or AD3.
// Each result includes the original data source; no synthetic fallback is made.
var Step = modules.load('RCStepAnalysis.mjs');
var state = {
    busy: false,
    pending: 0,
    energized: false,
    verified: false,
    records: [],
    lastModel: null,
    lastPhysical: null,
    display: 'MODEL',
    lastResult: null,
    sessionCounter: 0
};
const COLORS = {
    input: '#6fc7fb',
    output: '#f4b66d',
    fit: '#60ddb0',
    residual: '#ef9bb6',
    theory: '#b59dff'
};
const SAMPLE_RATE = 250000,
    SAMPLE_COUNT = 8192,
    MAX_RECORDS = 500;
function isPhysical() {
    return String(Device.SN) !== 'DEMO' && String(Device.SN).length > 0;
}
function number(id, lo, hi) {
    const value = Number(ui.get(id));
    if (!Number.isFinite(value) || value < lo || value > hi) throw Error(id + ': expected ' + lo + ' .. ' + hi);
    return value;
}
/**
 * Read the parameters from visible controls in their native displayed units.
 * Theory converts capacitance from nanofarads to farads inside Step.theory().
 * Keeping the parameter reading here makes both MODELS and hardware use the
 * same operator-entered component values.
 */
function config() {
    const R = number('rcR', 100, 100000),
        Cnf = number('rcC', 1, 1000),
        fHz = number('rcFreq', 40, 200),
        Vpk = number('rcVpk', .2, .5),
        noiseRmsMv = number('rcNoise', 0, 30),
        p = Step.theory(R, Cnf, fHz);
    return {
        R,
        Cnf,
        fHz,
        Vpk,
        noiseRmsMv,
        p
    };
}
function status(s) {
    ui.set('rcStatus', s);
    ui.set('captureStatus', s);
    ui.set('notebookStatus', s);
    print('[RC Step] ' + s);
}
function safe(name, fn) {
    try {
        fn();
    } catch (err) {
        if (state.energized || state.pending) stopHardware('Acquisition error');
        status('ERROR ' + name + ': ' + String(err));
    }
}
function changeConfig() {
    safe('settings', function () {
        if (state.energized || state.pending) stopHardware('Settings changed');
        ui.set('stepWiringVerified', false);
        updateTheory();
        runSynthetic(false);
    });
}
function updateTheory() {
    const s = config(),
        p = s.p;
    ui.set('theoryTau', 'RC = ' + (p.tau * 1e6).toFixed(2) + ' microseconds');
    ui.set('theoryFc', '1/(2πRC) = ' + p.fc.toFixed(2) + ' Hz');
    ui.set('theoryHalf', 'Half period / τ = ' + p.halfPeriodsPerTau.toFixed(2) + '   •   After 5τ: ' + (100 * (1 - Math.exp(-5))).toFixed(2) + '% settled');
    ui.set('theoryComparison', 'Same physical RC:  τ = ' + (p.tau * 1e6).toFixed(2) + ' µs  ↔  fc = ' + p.fc.toFixed(2) + ' Hz');
    ui.set('modelSource', 'THEORY ONLY • R ' + s.R + ' Ω / C ' + s.Cnf + ' nF / square ' + s.fHz + ' Hz • NOT a measurement');
    ui.set('acPrediction', 'AC frequency-response prediction from entered R,C: fc = ' + p.fc.toFixed(2) + ' Hz  |  compare to the separately measured AC Laboratory cutoff.');
}
function stopHardware(reason) {
    if (state.pending) {
        try {
            clearTimeout(state.pending);
        } catch (ignore) { }
        state.pending = 0;
    }
    let errors = [];
    if (state.energized || state.busy) {
        try {
            if (Scope1.stop() === false) errors.push('Scope.stop() returned false');
        } catch (e) {
            errors.push('Scope: ' + String(e));
        }
        try {
            if (Wavegen1.stop() === false) errors.push('Wavegen1.stop() returned false');
        } catch (e) {
            errors.push('W1: ' + String(e));
        }
    }
    state.busy = false;
    state.energized = false;
    ui.set('hardwareOutput', errors.length ? 'STOP FAILED — verify device output manually: ' + errors.join('; ') : 'W1 OFF • no active laboratory waveform');
    if (reason) status((errors.length ? 'STOP REQUEST ERROR' : 'W1 OFF') + ' • ' + reason);
    return errors.length === 0;
}
function onStopClicked() {
    stopHardware('Operator STOP');
}
function onWiringChanged() {
    if (ui.get('stepWiringVerified') !== true && Number(ui.get('stepWiringVerified')) !== 1) {
        if (state.energized || state.pending) stopHardware('Wiring verification revoked');
    }
}
/**
 * HARDWARE PREFLIGHT: an output can be started only by an explicit operator
 * action after the wiring check. The software restrictions are conservative
 * laboratory limits, not a substitute for electrical safety.
 */
function validatedPhysical() {
    const s = config();
    if (!isPhysical()) throw Error('Select an actual Physical Device; simulation never energizes W1');
    if (ui.get('stepWiringVerified') !== true && Number(ui.get('stepWiringVerified')) !== 1) throw Error('Verify flywires and enable WIRING VERIFIED first');
    if (s.R < 900) throw Error('For hardware R must be >=900 Ω (nominal 1 kΩ class)');
    if (s.p.tau < 0.000070 || s.p.tau > 0.00120) throw Error('For this 250 kS/s exercise, RC must be 70..1200 µs');
    if (s.fHz < 60 || s.fHz > 150) throw Error('Hardware square-wave frequency must be 60..150 Hz');
    if (s.p.halfPeriodsPerTau < 6) throw Error('Choose a slower square wave: each half-cycle must be at least 6τ');
    return s;
}
function onCaptureClicked() {
    safe('physical capture', function () {
        if (state.busy) throw Error('Capture already running. STOP before retrying');
        const s = validatedPhysical();
        state.busy = true;
        try {
            Wavegen1.Channel1.enable = true;
            Wavegen1.Channel2.enable = false;
            Wavegen1.Channel1.Mode.text = 'Simple';
            Wavegen1.Channel1.Simple.Type.text = 'Square';
            Wavegen1.Channel1.Simple.Frequency.value = s.fHz;
            Wavegen1.Channel1.Simple.Amplitude.value = s.Vpk;
            Wavegen1.Channel1.Simple.Offset.value = 0;
            if (Wavegen1.Channel1.Simple.Type.text !== 'Square') throw Error('W1 rejected Square');
            // Mark ownership BEFORE starting: even a failed start/readback requests OFF.
            state.energized = true;
            if (!Wavegen1.run()) throw Error('Wavegen1.run() failed: ' + Wavegen1.lastError);
            const rb = Wavegen1.status(1);
            if (rb && rb.carrierFunction && rb.carrierFunction !== 'Square') throw Error('W1 SDK readback is ' + rb.carrierFunction + ', expected Square');
            ui.set('hardwareOutput', 'W1 ACTIVE • ' + s.fHz + ' Hz Square • ' + s.Vpk.toFixed(2) + ' Vpk • 0 V DC • auto-stop after capture');
            status('PHYSICAL AD3 • Waiting 300 ms for the RC steady-periodic response; then acquiring CH1 and CH2');
            state.pending = setTimeout(function () {
                state.pending = 0;
                try {
                    capturePhysical(s);
                } catch (err) {
                    status('ERROR physical acquisition: ' + String(err));
                }
                finally {
                    stopHardware('Physical capture finished — W1 automatically disabled');
                }
            }, 300);
        } catch (err) {
            stopHardware('Physical setup failed');
            throw err;
        }
    });
}
/**
 * PHYSICAL CAPTURE — CH1 is the driven square wave; CH2 is the capacitor node.
 * The sample rate comes from the SDK readback, not merely from our request.
 * Step.analyzeCapture() analyzes only the acquired paired voltage arrays.
 */
function capturePhysical(s) {
    if (!state.energized) throw Error('W1 no longer armed');
    Scope1.Time.Samples.value = SAMPLE_COUNT;
    Scope1.Time.Rate.value = SAMPLE_RATE;
    Scope1.Channel1.Range.value = 5;
    Scope1.Channel2.Range.value = 5;
    Scope1.Trigger.Trigger.text = 'None';
    // no reliance on unverified trigger configuration
    if (!Scope1.single()) throw Error('Scope.single() failed: ' + Scope1.lastError);
    if (!Scope1.wait(3)) throw Error('Scope.wait() failed: ' + Scope1.lastError);
    const ch1 = Array.from(Scope1.Channel1.getData(), Number),
        ch2 = Array.from(Scope1.Channel2.getData(), Number);
    const info = Scope1.captureInfo();
    const actualRate = Number(info.sampleRateHz) || Number(Scope1.Time.Rate.real);
    if (!(actualRate > 0)) throw Error('Scope did not report its actual sample rate');
    const capture = {
        source: 'AD3',
        device: String(Device.SN),
        ch1,
        ch2,
        rateHz: actualRate,
        fHz: s.fHz,
        R: s.R,
        Cnf: s.Cnf,
        amplitudeVpk: s.Vpk,
        measuredAtISO: new Date().toISOString()
    };
    const result = Step.analyzeCapture(capture);
    // NO MODEL fallback on failure
    state.lastPhysical = capture;
    addResult(result);
    state.display = 'AD3';
    showCapture(capture, result);
    status('AD3 MEASURED • ' + (result.fitTauS * 1e6).toFixed(2) + ' µs • ' + result.transitions.length + ' complete edges • ' + (result.residualRmsV * 1000).toFixed(3) + ' mV residual');
}
/**
 * SIMULATION PATH — pure mathematics. Never enable an AWG or request a Scope
 * capture here. The deterministic noise source makes repeated demonstrations
 * reproducible, but repeated MODEL rows are not independent hardware trials.
 */
function runSynthetic(record) {
    const s = config();
    if (s.p.tau < 0.000040 || s.p.halfPeriodsPerTau < 3)
        throw Error('For the fixed 250 kS/s demonstration use τ >= 40 µs and each half-period >= 3τ; change R, C or frequency.');
    const capture = Step.syntheticCapture({
        R: s.R,
        Cnf: s.Cnf,
        fHz: s.fHz,
        amplitudeVpk: s.Vpk,
        noiseRmsMv: s.noiseRmsMv,
        rateHz: SAMPLE_RATE,
        count: SAMPLE_COUNT
    });
    const result = Step.analyzeCapture(capture);
    state.lastModel = capture;
    state.display = 'MODEL';
    showCapture(capture, result);
    if (record) addResult(result);
    status('SYNTHETIC MODEL ONLY • fitted ' + (result.fitTauS * 1e6).toFixed(2) + ' µs; no device operations');
}
function onSyntheticClicked() {
    safe('synthetic experiment', function () {
        runSynthetic(true);
    });
}
function onSyntheticSettingsChanged() {
    if (state.display === 'MODEL') safe('synthetic update', function () {
        updateTheory();
        runSynthetic(false);
    });
}
function onShowRealClicked() {
    safe('show physical', function () {
        if (!state.lastPhysical) throw Error('No genuine AD3 capture yet; use Page 04');
        const r = Step.analyzeCapture(state.lastPhysical);
        state.display = 'AD3';
        showCapture(state.lastPhysical, r);
    });
}
function onShowModelClicked() {
    safe('show model', function () {
        runSynthetic(false);
    });
}
function formatTau(v) {
    return v === null ? 'n/a' : (v * 1e6).toFixed(2) + ' µs';
}
/**
 * PRESENTATION — reconstructed voltage uses v(t)=Vinf+A*exp(-t/tau).
 * The plotted residual is actual/sample-model minus the reconstructed curve.
 * Small residuals demonstrate model agreement, not calibrated accuracy.
 */
function showCapture(capture, result) {
    state.lastResult = result;
    const real = result.source === 'AD3',
        source = real ? 'PHYSICAL AD3' : 'SYNTHETIC MODEL ONLY';
    ui.set('analysisOrigin', source + ' • ' + result.count + ' paired samples • ' + result.rateHz.toFixed(1) + ' samples/s • ' + result.fHz + ' Hz');
    ui.set('measuredTau', source + '  τ = ' + formatTau(result.fitTauS));
    ui.set('measuredRise', 'RISING τ = ' + formatTau(result.riseTauS));
    ui.set('measuredFall', 'FALLING τ = ' + formatTau(result.fallTauS));
    ui.set('measured63', '63.2% CROSSING = ' + formatTau(result.tau63S));
    ui.set('measuredFc', 'FROM FIT:  fc = ' + result.fitFcHz.toFixed(2) + ' Hz');
    ui.set('measuredResidual', 'FIT RESIDUAL ' + (result.residualRmsV * 1000).toFixed(3) + ' mV RMS  •   ' + result.relativeResidualPct.toFixed(3) + '% of CH1 swing');
    ui.set('measuredError', 'vs ENTERED R,C: ' + result.errorPct.toFixed(2) + '% τ error  •   ' + result.transitions.length + ' complete transitions');
    ui.set('measurementCaution', (real ? 'AD3 Scope samples, not a simulated waveform. ' : 'Generated mathematical samples, not AD3 data. ') +
        'R and C are operator-entered values. Residual is model mismatch, NOT calibrated instrument accuracy.');
    const { ch1, ch2, rateHz } = capture;
    const x = [],
        a = [],
        b = [],
        step = Math.max(1, Math.floor(ch1.length / 650));
    for (let i = 0; i < ch1.length; i += step) {
        x.push(i * 1000 / rateHz);
        a.push(ch1[i]);
        b.push(ch2[i]);
    }
    for (const id of ['stepWavePlot', 'stepPhysicalPlot']) {
        plots.clear(id);
        plots.configure(id, {
            title: source + ' • CH1 square wave and CH2 capacitor voltage',
            chartType: 'XY',
            xLabel: 'Time (ms)',
            yLabel: 'Voltage (V)',
            showLegend: true,
            showGrid: true,
            xAuto: true,
            yAuto: true
        });
        plots.setSeries(id, 'CH1 input ' + source, x, a, {
            color: COLORS.input,
            width: 1.5
        });
        plots.setSeries(id, 'CH2 output ' + source, x, b, {
            color: COLORS.output,
            width: 2.1
        });
    }
    const tr = result.transitions[0],
        start = tr.first,
        end = tr.last,
        indices = [],
        sample = [],
        fitted = [],
        residual = [];
    const inc = Math.max(1, Math.floor((end - start) / 520));
    for (let i = start; i < end; i += inc) {
        const fit = Step.predict(tr, i, rateHz);
        indices.push((i / rateHz - tr.edgeTimeS) * 1000);
        sample.push(ch2[i]);
        fitted.push(fit);
        residual.push((ch2[i] - fit) * 1000);
    }
    plots.clear('stepFitPlot');
    plots.configure('stepFitPlot', {
        title: source + ' • ' + tr.direction + ' measured vs exponential fit',
        chartType: 'XY',
        xLabel: 'Time since CH1 edge (ms)',
        yLabel: 'CH2 voltage (V)',
        showLegend: true,
        showGrid: true,
        xAuto: true,
        yAuto: true
    });
    plots.setSeries('stepFitPlot', 'CH2 samples', indices, sample, {
        color: COLORS.output,
        width: 1.2
    });
    plots.setSeries('stepFitPlot', 'Exponential least-squares fit', indices, fitted, {
        color: COLORS.fit,
        width: 2.6
    });
    plots.clear('stepResidualPlot');
    plots.configure('stepResidualPlot', {
        title: source + ' • CH2 sample minus fit',
        chartType: 'XY',
        xLabel: 'Time since CH1 edge (ms)',
        yLabel: 'Residual (mV)',
        showLegend: true,
        showGrid: true,
        xAuto: true,
        yAuto: true
    });
    plots.setSeries('stepResidualPlot', 'Residuals', indices, residual, {
        color: COLORS.residual,
        width: 1.5
    });
    ui.set('transitionDetail', tr.direction + ' edge • V∞ ' + tr.finalV.toFixed(4) + ' V • A ' + tr.amplitude.toFixed(4) + ' V • τ ' +
        (tr.tauS * 1e6).toFixed(2) + ' µs • R² ' + (tr.rSquared === null ? 'n/a' : tr.rSquared.toFixed(8)));
    ui.set('compareLesson', 'THEORY: τ=' + formatTau(result.theoryTauS) + ' → fc=' + result.theoryFcHz.toFixed(2) + ' Hz    |    ' +
        source + ': τ=' + formatTau(result.fitTauS) + ' → fc=' + result.fitFcHz.toFixed(2) + ' Hz');
}
function rowValues(r) {
    return [
        r.source,
        String(r.R),
        String(r.Cnf),
        String(r.fHz),
        (r.theoryTauS * 1e6).toFixed(2),
        (r.riseTauS * 1e6).toFixed(2),
        (r.fallTauS * 1e6).toFixed(2),
        (r.fitTauS * 1e6).toFixed(2),
        r.tau63S === null ? 'n/a' : (r.tau63S * 1e6).toFixed(2),
        r.fitFcHz.toFixed(2),
        (r.residualRmsV * 1000).toFixed(3),
        r.errorPct.toFixed(2)
    ];
}
/**
 * NOTEBOOK — save small fit summaries, not thousands of duplicate raw samples.
 * Separate source labels are retained in each exported record. Real waveforms
 * have an independent CSV export so the original samples remain auditable.
 */
function addResult(result) {
    if (state.records.length >= MAX_RECORDS) throw Error('Notebook is full: export and clear');
    // Keep numerical summaries, never duplicate 8192-sample arrays in the table/report.
    // QJSEngine compatibility: copy only scalar/result metadata. Object-rest
    // and object-spread syntax are not accepted by the target script engine.
    const summary = {};
    for (const key of Object.keys(result)) {
        if (key !== 'transitions' && key !== 'edges' && key !== 'ch1' && key !== 'ch2') summary[key] = result[key];
    }
    summary.transitions = result.transitions.map(function (t) {
        const compact = {};
        for (const key of Object.keys(t)) {
            if (key !== 'first' && key !== 'last' && key !== 'crossing63S' && key !== 'edgeIndex') compact[key] = t[key];
        }
        return compact;
    });
    state.records.push(summary);
    ui.set('stepNotebook', state.records.map(rowValues));
    const countAD3 = state.records.filter(r => r.source === 'AD3').length;
    ui.set('notebookCount', 'SESSION: ' + state.records.length + ' rows • ' + countAD3 + ' genuine AD3 / ' + (state.records.length - countAD3) + ' synthetic');
}
function exportStem() {
    // Collision-resistant within this Panel instance; no overwrite of earlier experiment files.
    const stamp = new Date().toISOString().replace(/[-:]/g, '').replace(/\..*/, '').replace('T', '_');
    state.sessionCounter++;
    return 'rc_step_' + stamp + '_' + String(state.sessionCounter).padStart(3, '0');
}
function onExportClicked() {
    safe('export notebook', function () {
        if (!state.records.length) throw Error('No records. RUN SYNTHETIC or CAPTURE AD3 first');
        const stem = exportStem(),
            rows = [[
                'Source',
                'R_ohm',
                'C_nF',
                'Frequency_Hz',
                'Tau_theory_us',
                'Tau_rise_us',
                'Tau_fall_us',
                'Tau_fit_us',
                'Tau_63pct_us',
                'Cutoff_from_fit_Hz',
                'Residual_RMS_mV',
                'Tau_error_pct'
            ]].concat(state.records.map(rowValues));
        const path = FileIO.dataPath(stem + '.csv'),
            json = FileIO.dataPath(stem + '.json');
        if (!FileIO.writeCsv(path, rows)) throw Error(FileIO.lastError);
        if (!FileIO.writeJson(json, {
            schema: 1,
            app: 'WFScript',
            lab: 'RC Step Response',
            exportedAt: new Date().toISOString(),
            note: 'MODEL rows are generated; AD3 rows are real Scope measurements. Entered R/C are not independently verified.',
            records: state.records
        })) throw Error(FileIO.lastError);
        ui.set('exportPath', 'SAVED ' + path + ' and ' + json);
        status('Exported ' + state.records.length + ' observations without overwriting earlier exports');
    });
}
function onWaveExportClicked() {
    safe('physical waveform export', function () {
        const c = state.lastPhysical;
        if (!c) throw Error('No AD3 waveform; synthetic samples cannot be exported as real hardware');
        const rows = [['Time_s', 'CH1_Vin_V', 'CH2_Vcap_V']];
        for (let i = 0; i < c.ch1.length; i++)rows.push([(i / c.rateHz).toFixed(9), c.ch1[i].toFixed(8), c.ch2[i].toFixed(8)]);
        const path = FileIO.dataPath(exportStem() + '_AD3_waveform.csv');
        if (!FileIO.writeCsv(path, rows)) throw Error(FileIO.lastError);
        ui.set('exportPath', 'PHYSICAL WAVEFORM CSV: ' + path);
        status('Exported ' + c.ch1.length + ' paired REAL AD3 samples');
    });
}
function onClearClicked() {
    safe('clear notebook', function () {
        if (state.energized || state.busy) throw Error('STOP hardware before clearing');
        state.records = [];
        state.lastPhysical = null;
        ui.set('stepNotebook', []);
        ui.set('notebookCount', 'SESSION: 0 rows');
        ui.set('exportPath', 'No files removed; previously exported files remain saved');
        if (state.display === 'AD3') runSynthetic(false);
        status('Notebook cleared, previous exports preserved');
    });
}
function onAnswerClicked() {
    safe('student challenge', function () {
        const p = config().p;
        ui.set('challengeAnswer', 'For R=' + p.R + ' Ω and C=' + p.Cnf + ' nF, τ=' + formatTau(p.tau) +
            ', 5τ=' + (p.settle5TauS * 1000).toFixed(3) + ' ms and fc=' + p.fc.toFixed(2) + ' Hz. Half-period/τ=' +
            p.halfPeriodsPerTau.toFixed(2) + '. Compare observed rising/falling fits and explain any disagreement.');
    });
}
function onInitialize() {
    try {
        ui.set('hardwareOutput', 'W1 OFF • model does not energize AD3');
        ui.set('stepWiringVerified', false);
        ui.set('stepNotebook', []);
        ui.set('notebookCount', 'SESSION: 0 rows');
        ui.set('exportPath', 'Exports use distinct timestamped file names in shared PanelData');
        updateTheory();
        runSynthetic(false);
        status('READY • Start Page 03: RUN SYNTHETIC. Physical W1 remains OFF until Page 04 verified capture.');
    } catch (e) {
        status('INITIALIZATION ERROR ' + String(e));
    }
}
function onShutdown() {
    if (state.energized || state.busy || state.pending) stopHardware('Panel shutdown');
}
