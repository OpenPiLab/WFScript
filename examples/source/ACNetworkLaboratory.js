/**
 * AC NETWORK LABORATORY — guided theory -> simulation -> breadboard -> AD3 -> report.
 * Requires WFScript v0.2-dev.8.9.1 native table limits; no native changes here.
 *
 * SAFETY AND PROVENANCE:
 * - Opening/starting the Panel performs NO hardware output operations.
 * - MODEL data is tagged and never labeled as physical measurement.
 * - W1 requires a physical device, the operator's wiring-verification toggle,
 *   and an explicit ARM action. Max 1 V peak, zero offset, sine only.
 * - Any acquisition/configuration error calls Wavegen1.stop() and Scope1.stop().
 * - Lifecycle onShutdown() and native runtime cleanup also stop W1.
 * - Only measured CH1 and CH2 samples produce AD3 rows.
 * - No oscilloscope input is ever used as a voltage source.
 *
 * This is an educational measurement, not calibrated metrology, a device
 * protection system, or a safety-rated interlock.
 */
// READING MAP
//   CircuitLab.mjs      -> theoretical low-pass / high-pass transfer functions.
//   CaptureAnalysis.mjs -> least-squares sine/cosine fit of REAL sampled voltages.
//   settings()/renderTheory() -> simulation and theoretical predictions only.
//   acquireOne()        -> actual two-channel AD3 acquisition and fitting.
//   renderFitExplorer() -> plot actual samples, reconstructed sinusoids, residuals.
//   addRow()/onExportClicked() -> preserve provenance in exported observations.
// Suggested learning path: understand the fitted equation BEFORE reading AD3 I/O.
var Circuit = modules.load('CircuitLab.mjs');
var Capture = modules.load('CaptureAnalysis.mjs');
var Archive = modules.load('SignalPackage.mjs');
var PI = Math.PI;
var colors = {
    a: '#55d6f5',
    b: '#f9bc70',
    model: '#c19bff',
    measured: '#65dfae',
    warn: '#ffad80'
};
var state = {
    armed: false,
    busy: false,
    pending: 0,
    sweep: false,
    frequencies: [],
    sweepIndex: 0,
    rows: [],
    lastCapture: null,
    lastMeasurement: null,
    lastPhysicalMode: '',
    lastPhysicalR: 0,
    lastPhysicalC: 0,
    seq: 0,
    fitView: 'SYNTHETIC'
};
var MAX_ROWS = 500;
function physical() {
    return String(Device.SN) !== 'DEMO' && String(Device.SN).length > 0;
}
function num(id, min, max) {
    const v = Number(ui.get(id));
    if (!Number.isFinite(v) || v < min || v > max) throw Error(id + ' must be ' + min + ' .. ' + max);
    return v;
}
function settings() {
    const type = String(ui.get('topology'));
    if (type !== 'Low-pass' && type !== 'High-pass') throw Error('Select Low-pass or High-pass');
    const R = num('rOhm', 100, 100000),
        Cnf = num('cNf', 1, 1000),
        f = num('simHz', 100, 20000),
        amplitude = num('driveVpk', 0.25, 1.00);
    return {
        type,
        R,
        Cnf,
        f,
        amplitude,
        params: Circuit.parameters(R, Cnf)
    };
}
function setStatus(message) {
    ui.set('labStatus', message);
    ui.set('measurementStatus', message);
    ui.set('notebookStatus', message);
    print('[AC Lab] ' + message);
}
function safeAction(name, fn) {
    try {
        fn();
    } catch (e) {
        if (state.armed || state.sweep) emergencyStop('Operation failed');
        setStatus('ERROR in ' + name + ': ' + String(e));
    }
}
function syncOverview() {
    const s = settings(),
        p = s.params,
        atCut = Circuit.transfer(s.type, s.R, s.Cnf, p.fc);
    const v = Circuit.transfer(s.type, s.R, s.Cnf, s.f);
    ui.set('statCutoff', 'fC  ' + p.fc.toFixed(1) + ' Hz');
    ui.set('statTau', 'TAU  ' + (p.tc * 1e6).toFixed(1) + ' us');
    ui.set('statGain', '|H|  ' + v.gain.toFixed(4) + '   /   ' + v.db.toFixed(2) + ' dB');
    ui.set('statPhase', 'PHASE  ' + v.phaseDeg.toFixed(1) + ' deg');
    ui.set('activeBuild', 'SELECTED CIRCUIT: ' + s.type.toUpperCase() + '   |   Follow the matching BUILD tab before arming W1');
    ui.set('hardwareSetpoints', 'MODEL VALUES: R=' + s.R + ' ohm, C=' + s.Cnf + ' nF.  Fit actual parts to these values; changing software does not change breadboard components.');
}
function renderTheory() {
    const s = settings(),
        bode = Circuit.makeBode(s.type, s.R, s.Cnf, 80, 20000, 220),
        time = Circuit.makeTime(s.type, s.R, s.Cnf, s.f, s.amplitude, 501);
    plots.clear('modelGain');
    plots.configure('modelGain', {
        title: 'MODEL   /   Transfer magnitude   |   theoretical, not measured',
        chartType: 'XY',
        xLabel: 'Frequency (Hz) [log]',
        yLabel: 'Gain (dB)',
        xLog: true,
        showGrid: true,
        showLegend: true,
        xAuto: true,
        yAuto: true
    });
    plots.setSeries('modelGain', 'Theoretical gain', bode.hz, bode.db, {
        color: colors.model,
        width: 2.5
    });
    plots.clear('modelPhase');
    plots.configure('modelPhase', {
        title: 'MODEL   /   Phase shift',
        chartType: 'XY',
        xLabel: 'Frequency (Hz) [log]',
        yLabel: 'Phase (deg)',
        xLog: true,
        showGrid: true,
        showLegend: true,
        xAuto: true,
        yAuto: true
    });
    plots.setSeries('modelPhase', 'Theoretical phase', bode.hz, bode.phase, {
        color: colors.model,
        width: 2.4
    });
    plots.clear('modelTime');
    plots.configure('modelTime', {
        title: 'MODEL   /   Input and output (4 periods)',
        chartType: 'XY',
        xLabel: 'Time (ms)',
        yLabel: 'Voltage (V)',
        showGrid: true,
        showLegend: true,
        xAuto: true,
        yAuto: true
    });
    plots.setSeries('modelTime', 'Vin — model', time.ms, time.vin, {
        color: colors.a,
        width: 2
    });
    plots.setSeries('modelTime', 'Vout — model', time.ms, time.vout, {
        color: colors.b,
        width: 2.3
    });
    syncOverview();
    ui.set('modelBanner', 'SIMULATION ONLY   •   ' + s.type + '   •   ' + s.f.toFixed(0) + ' Hz   •   theoretical curves (no AD3 measurements)');
    renderNotebookCharts();
}
function invalidateWiring(reason) {
    if (state.armed || state.busy || state.sweep) emergencyStop(reason);
    ui.set('wiringVerified', false);
    ui.set('hardwareState', 'W1 OFF  •  Wiring verification cleared');
}
function onModelParametersChanged() {
    safeAction('model update', function () {
        invalidateWiring('component values/topology changed');
        renderTheory();
    });
}
function onModelFrequencyChanged() {
    safeAction('simulation update', renderTheory);
}
function onModelSweepClicked() {
    safeAction('model sweep', function () {
        const s = settings();
        const hz = Circuit.sweepFrequencies(s.params.fc, 100, 20000);
        let count = 0;
        for (const f of hz) {
            const h = Circuit.transfer(s.type, s.R, s.Cnf, f);
            addRow({
                source: 'MODEL',
                type: s.type,
                R: s.R,
                Cnf: s.Cnf,
                f,
                gain: h.gain,
                db: h.db,
                phaseDeg: h.phaseDeg,
                inputVpk: s.amplitude,
                outputVpk: s.amplitude * h.gain,
                predGain: h.gain,
                predPhase: h.phaseDeg
            });
            count++;
        }
        setStatus('MODEL SWEEP • ' + count + ' mathematical predictions added; not AD3 data.');
    });
}
function requirePhysicalAndVerified() {
    if (!physical()) throw Error('Select Device > Physical Device before running. Simulation never enables W1.');
    const x = ui.get('wiringVerified');
    if (x !== true && Number(x) !== 1) throw Error('First inspect the breadboard and enable WIRING VERIFIED.');
    const s = settings();
    // A measured nominal 1 kOhm resistor may be 980 ohm (or slightly lower).
    if (s.R < 900) throw Error('For physical tests use R >= 900 ohm (nominal 1 kOhm class)');
    if (s.Cnf < 1 || s.Cnf > 1000) throw Error('Invalid physical capacitor');
    return s;
}
function updateHardwareIndicators() {
    ui.set('hardwareState', state.armed ? 'W1 ACTIVE  •  SINE  •  <= 1 Vpk  •  0 V OFFSET' : 'W1 OFF  •  no generated output');
    ui.set('deviceIndicator', physical() ? 'PHYSICAL DEVICE SELECTED — requires verified wiring' : 'SIMULATION — no real W1 or Scope acquisition');
    ui.set('recordCount', 'RESULTS   ' + state.rows.filter(r => r.source === 'AD3').length + ' AD3  /  ' + state.rows.filter(r => r.source === 'MODEL').length + ' model');
}
function configureWavegen(f, s) {
    if (!Number.isFinite(f) || f < 100 || f > 20000) throw Error('W1 frequency must be 100..20000 Hz');
    // Hardware may have started even if the subsequent status read throws.
    // Always attempt a stop on an incomplete ARM/reconfigure, including before
    // state.armed is set. Never leave W1 active after a failed prerequisite.
    try {
        Wavegen1.Channel1.enable = true;
        Wavegen1.Channel2.enable = false;
        Wavegen1.Channel1.Mode.text = 'Simple';
        Wavegen1.Channel1.Simple.Type.text = 'Sine';
        Wavegen1.Channel1.Simple.Frequency.value = f;
        Wavegen1.Channel1.Simple.Amplitude.value = s.amplitude;
        Wavegen1.Channel1.Simple.Offset.value = 0;
        if (Wavegen1.Channel1.Simple.Type.text !== 'Sine') throw Error('Wavegen rejected Sine waveform');
        if (!Wavegen1.run()) throw Error('W1 start failed: ' + Wavegen1.lastError);
        const actual = Wavegen1.status(1);
        if (actual && actual.carrierFunction && actual.carrierFunction !== 'Sine')
            throw Error('W1 hardware reported ' + actual.carrierFunction + ' rather than Sine');
    } catch (e) {
        try {
            Wavegen1.stop();
        } catch (ignore) { }
        throw e;
    }
}
/**
 * HARDWARE OWNERSHIP — request W1 and Scope shutdown on errors or Panel exit.
 * A failed stop is reported rather than claimed to be a safe physical state.
 */
function emergencyStop(reason) {
    if (state.pending) {
        clearTimeout(state.pending);
        state.pending = 0;
    }
    state.sweep = false;
    state.busy = false;
    state.armed = false;
    // Best-effort cleanup even when the previous command threw.
    try {
        Wavegen1.stop();
    } catch (ignore) { }
    try {
        Scope1.stop();
    } catch (ignore) { }
    updateHardwareIndicators();
    if (reason) setStatus('W1 OFF / SWEEP STOPPED   •   ' + reason);
}
function onArmClicked() {
    safeAction('ARM W1', function () {
        if (state.busy || state.sweep) throw Error('Stop the active capture or sweep before arming');
        const s = requirePhysicalAndVerified();
        const hz = num('testHz', 100, 20000);
        configureWavegen(hz, s);
        state.armed = true;
        state.lastPhysicalMode = s.type;
        state.lastPhysicalR = s.R;
        state.lastPhysicalC = s.Cnf;
        updateHardwareIndicators();
        setStatus('PHYSICAL W1 ARMED • ' + hz + ' Hz sine / ' + s.amplitude.toFixed(2) + ' Vpk, DC offset 0. CH1=Vin, CH2=Vout.');
    });
}
function onWiringVerificationChanged() {
    // Revoking confirmation while energized must shut W1 down immediately.
    if (ui.get('wiringVerified') !== true && Number(ui.get('wiringVerified')) !== 1 && state.armed)
        emergencyStop('Wiring verification revoked');
}
function onOffClicked() {
    emergencyStop('Operator STOP button');
}
function schedule(fn, ms) {
    state.pending = setTimeout(function () {
        state.pending = 0;
        safeAction('scheduled acquisition', fn);
    }, ms);
}
function verifyStillMatched() {
    const s = requirePhysicalAndVerified();
    if (!state.armed) throw Error('W1 is not armed');
    if (state.lastPhysicalMode !== s.type || state.lastPhysicalR !== s.R || state.lastPhysicalC !== s.Cnf)
        throw Error('Circuit changed after ARM; STOP, recheck wiring and re-ARM');
    return s;
}
/**
 * PHYSICAL MEASUREMENT — inspect this after understanding CaptureAnalysis.mjs.
 *
 * Both channels must share one acquisition and the device's *actual* sample
 * rate. The fit estimates input/output amplitude and phase at commanded f.
 * No synthetic values are substituted if physical acquisition fails.
 */
function acquireOne(f) {
    const s = verifyStillMatched();
    Scope1.Time.Samples.value = 4096;
    Scope1.Time.Rate.value = Math.max(20000, Math.min(2500000, Math.round(f * 128)));
    Scope1.Channel1.Range.value = 5;
    Scope1.Channel2.Range.value = 5;
    Scope1.Trigger.Trigger.text = 'None';
    if (!Scope1.single()) throw Error('Scope single() failed: ' + Scope1.lastError);
    if (!Scope1.wait(3)) throw Error('Scope wait() failed: ' + Scope1.lastError);
    const raw1 = Scope1.Channel1.getData(),
        raw2 = Scope1.Channel2.getData();
    // Rate is taken from the completed acquisition, not an assumed request.
    const info = Scope1.captureInfo();
    const actualRate = Number(info.sampleRateHz) || Number(Scope1.Time.Rate.real);
    if (!(actualRate > 0)) throw Error('Scope did not report a valid actual sample rate');
    const m = Capture.analyzeTwoChannel(raw1, raw2, actualRate, f);
    const p = Circuit.transfer(s.type, s.R, s.Cnf, f);
    const row = {
        source: 'AD3',
        type: s.type,
        R: s.R,
        Cnf: s.Cnf,
        f,
        gain: m.gain,
        db: m.db,
        phaseDeg: m.phaseDeg,
        inputVpk: m.inputVpk,
        outputVpk: m.outputVpk,
        predGain: p.gain,
        predPhase: p.phaseDeg,
        rateHz: actualRate,
        device: String(Device.SN),
        fitInRmsV: m.inputFit.rmsResidualV,
        fitOutRmsV: m.outputFit.rmsResidualV,
        fitInPct: m.inputFit.residualPct,
        fitOutPct: m.outputFit.residualPct,
        fitInR2: m.inputFit.rSquared,
        fitOutR2: m.outputFit.rSquared
    };
    addRow(row);
    state.lastMeasurement = m;
    const preview = Capture.timePreview(raw1, raw2, actualRate, 600);
    plots.clear('physicalTime');
    plots.configure('physicalTime', {
        title: 'AD3 PHYSICAL CAPTURE   •   CH1 Vin / CH2 Vout',
        chartType: 'XY',
        xLabel: 'Time (ms)',
        yLabel: 'Measured volts',
        showLegend: true,
        showGrid: true,
        xAuto: true,
        yAuto: true
    });
    plots.setSeries('physicalTime', 'CH1 Vin MEASURED', preview.ms, preview.vin, {
        color: colors.a,
        width: 1.8
    });
    plots.setSeries('physicalTime', 'CH2 Vout MEASURED', preview.ms, preview.vout, {
        color: colors.b,
        width: 2.2
    });
    // Decimate ONLY for plotting; preserve actual captured voltages for export.
    state.lastCapture = {
        fHz: f,
        rateHz: actualRate,
        device: String(Device.SN),
        topology: s.type,
        R: s.R,
        Cnf: s.Cnf,
        ch1: Array.from(raw1),
        ch2: Array.from(raw2)
    };
    if (state.fitView === 'AD3') renderFitExplorer();
    ui.set('measuredGain', 'AD3 GAIN   ' + m.gain.toFixed(4) + '   /   ' + m.db.toFixed(2) + ' dB');
    ui.set('measuredPhase', 'AD3 PHASE   ' + m.phaseDeg.toFixed(2) + ' deg');
    ui.set('measuredAmplitude', 'CH1 ' + m.inputVpk.toFixed(3) + ' Vpk   •   CH2 ' + m.outputVpk.toFixed(3) + ' Vpk   •   ' + Circuit.nominalSampleRatio(Math.round(actualRate), Math.round(f)) + ' samples/cycle');
    ui.set('measuredError', 'vs MODEL: gain ' + ((m.gain / p.gain - 1) * 100).toFixed(2) + '%   •   phase ' + (m.phaseDeg - p.phaseDeg).toFixed(2) + ' deg');
    return row;
}
function onCaptureClicked() {
    safeAction('single capture', function () {
        if (state.busy || state.sweep) throw Error('Acquisition in progress');
        const s = verifyStillMatched(),
            f = num('testHz', 100, 20000);
        configureWavegen(f, s);
        state.busy = true;
        setStatus('AD3 CAPTURE PENDING • W1 setting settling for 180 ms');
        schedule(function () {
            const row = acquireOne(f);
            state.busy = false;
            setStatus('AD3 CAPTURED ' + row.f + ' Hz • gain ' + row.gain.toFixed(4) + ' • phase ' + row.phaseDeg.toFixed(2) + ' deg');
        }, 180);
    });
}
function advanceSweep() {
    if (!state.sweep || !state.armed) return;
    if (state.sweepIndex >= state.frequencies.length) {
        state.sweep = false;
        state.busy = false;
        emergencyStop('AD3 SWEEP COMPLETE — W1 automatically disabled');
        return;
    }
    const f = state.frequencies[state.sweepIndex],
        s = verifyStillMatched();
    configureWavegen(f, s);
    setStatus('AD3 SWEEP • ' + (state.sweepIndex + 1) + '/' + state.frequencies.length + ' • settling ' + f + ' Hz');
    schedule(function () {
        const row = acquireOne(f);
        state.sweepIndex++;
        setStatus('MEASURED ' + state.sweepIndex + '/' + state.frequencies.length + ' • ' + f + ' Hz • ' + row.db.toFixed(2) + ' dB / ' + row.phaseDeg.toFixed(1) + ' deg');
        schedule(advanceSweep, 60);
    }, 180);
}
function onHardwareSweepClicked() {
    safeAction('AD3 sweep', function () {
        if (state.busy || state.sweep) throw Error('Acquisition already in progress');
        const s = verifyStillMatched();
        state.frequencies = Circuit.sweepFrequencies(s.params.fc, 100, 20000);
        state.sweepIndex = 0;
        state.sweep = true;
        state.busy = true;
        advanceSweep();
    });
}
function rowText(r) {
    return [
        r.source,
        r.type,
        String(r.R),
        String(r.Cnf),
        String(r.f),
        r.predGain.toFixed(4),
        r.gain.toFixed(4),
        r.db.toFixed(2),
        r.predPhase.toFixed(1),
        r.phaseDeg.toFixed(1),
        r.source === 'AD3' ? ((r.gain / r.predGain - 1) * 100).toFixed(2) : '—'
    ];
}
function addRow(row) {
    if (state.rows.length >= MAX_ROWS) throw Error('Log full (' + MAX_ROWS + ' rows): export and clear before adding more');
    state.rows.push(row);
    state.seq++;
    updateNotebook();
}
function updateNotebook() {
    ui.set('resultsTable', state.rows.map(rowText));
    const measured = state.rows.filter(r => r.source === 'AD3'),
        model = state.rows.filter(r => r.source === 'MODEL');
    ui.set('recordCount', 'RESULTS   ' + measured.length + ' AD3  /  ' + model.length + ' model');
    ui.set('totalRows', 'RECORDS  ' + state.rows.length + ' / ' + MAX_ROWS);
    const m = Capture.summary(state.rows);
    ui.set('errorSummary', m.count ? 'AD3 mean |gain err| ' + m.meanGainErrorPct.toFixed(2) + '%   |phase err| ' + m.meanPhaseErrorDeg.toFixed(2) + ' deg' : 'No physical measurements yet — model rows are predictions only');
    renderNotebookCharts();
}
function renderNotebookCharts() {
    const s = settings(),
        bode = Circuit.makeBode(s.type, s.R, s.Cnf, 80, 20000, 180);
    const rows = state.rows.filter(r => r.type === s.type && r.R === s.R && r.Cnf === s.Cnf && r.source === 'AD3').sort((a, b) => a.f - b.f);
    plots.clear('comparisonGain');
    plots.configure('comparisonGain', {
        title: 'THEORY vs PHYSICAL AD3   •   Gain',
        chartType: 'XY',
        xLabel: 'Frequency (Hz) [log]',
        yLabel: 'Gain (dB)',
        xLog: true,
        showLegend: true,
        showGrid: true,
        xAuto: true,
        yAuto: true
    });
    plots.setSeries('comparisonGain', 'RC model', bode.hz, bode.db, {
        color: colors.model,
        width: 2
    });
    if (rows.length) plots.setSeries('comparisonGain', 'AD3 measurements', rows.map(r => r.f), rows.map(r => r.db), {
        color: colors.measured,
        width: 1.3,
        markerSize: 7
    });
    plots.clear('comparisonPhase');
    plots.configure('comparisonPhase', {
        title: 'THEORY vs PHYSICAL AD3   •   Phase',
        chartType: 'XY',
        xLabel: 'Frequency (Hz) [log]',
        yLabel: 'Phase (deg)',
        xLog: true,
        showLegend: true,
        showGrid: true,
        xAuto: true,
        yAuto: true
    });
    plots.setSeries('comparisonPhase', 'RC model', bode.hz, bode.phase, {
        color: colors.model,
        width: 2
    });
    if (rows.length) plots.setSeries('comparisonPhase', 'AD3 measurements', rows.map(r => r.f), rows.map(r => r.phaseDeg), {
        color: colors.measured,
        width: 1.3,
        markerSize: 7
    });
}
function onClearLogClicked() {
    safeAction('clear results', function () {
        if (state.busy || state.sweep) throw Error('STOP sweep before clearing data');
        state.rows = [];
        state.lastCapture = null;
        state.lastMeasurement = null;
        if (state.fitView === 'AD3') {
            state.fitView = 'SYNTHETIC';
            renderFitExplorer();
        }
        updateNotebook();
        setStatus('Results cleared. Previous exported files are unchanged.');
    });
}
function filePath(name) {
    const path = FileIO.dataPath(name);
    if (!path) throw Error('Panel Data directory not available');
    return path;
}
function csvRows() {
    const header = [
        'Source',
        'Topology',
        'R_ohm',
        'C_nF',
        'Frequency_Hz',
        'Pred_Gain',
        'Gain',
        'Gain_dB',
        'Pred_Phase_deg',
        'Phase_deg',
        'Gain_Error_pct',
        'Device',
        'CH1_Fit_RMS_V',
        'CH2_Fit_RMS_V',
        'CH1_Fit_Residual_pct',
        'CH2_Fit_Residual_pct'
    ];
    return [header].concat(state.rows.map(r => [
        r.source,
        r.type,
        r.R,
        r.Cnf,
        r.f,
        r.predGain.toFixed(8),
        r.gain.toFixed(8),
        r.db.toFixed(5),
        r.predPhase.toFixed(5),
        r.phaseDeg.toFixed(5),
        ((r.gain / r.predGain - 1) * 100).toFixed(4),
        r.device || 'MODEL',
        r.source === 'AD3' ? r.fitInRmsV.toFixed(7) : '',
        r.source === 'AD3' ? r.fitOutRmsV.toFixed(7) : '',
        r.source === 'AD3' ? r.fitInPct.toFixed(4) : '',
        r.source === 'AD3' ? r.fitOutPct.toFixed(4) : ''
    ]));
}
function onExportClicked() {
    safeAction('CSV export', function () {
        if (!state.rows.length) throw Error('Nothing to export; simulate a sweep or capture AD3');
        const path = filePath('ac_network_lab_results.csv');
        if (!FileIO.writeCsv(path, csvRows())) throw Error(FileIO.lastError);
        const jsonPath = filePath('ac_network_lab_report.json');
        const report = {
            tool: 'WFScript AC Network Laboratory',
            schemaVersion: 1,
            dateISO: new Date().toISOString(),
            note: 'MODEL is theory only. AD3 rows derive from physical two-channel Scope acquisitions.',
            configuration: settings(),
            measurements: state.rows,
            summary: Capture.summary(state.rows)
        };
        if (!FileIO.writeJson(jsonPath, report)) throw Error(FileIO.lastError);
        ui.set('exportLocation', 'CSV + JSON in ' + FileIO.dataDirectory());
        setStatus('EXPORTED ' + state.rows.length + ' rows • ' + path + ' • ' + jsonPath);
    });
}
function onExportWaveClicked() {
    safeAction('waveform CSV', function () {
        const c = state.lastCapture;
        if (!c) throw Error('No physical AD3 capture available; simulation does not create raw scope files');
        const rows = [['Time_s', 'CH1_Vin_V', 'CH2_Vout_V']];
        for (let i = 0; i < c.ch1.length; i++)rows.push([(i / c.rateHz).toFixed(9), c.ch1[i].toFixed(7), c.ch2[i].toFixed(7)]);
        const path = filePath('ac_network_last_ad3_waveform.csv');
        if (!FileIO.writeCsv(path, rows)) throw Error(FileIO.lastError);
        setStatus('EXPORTED REAL AD3 waveform: ' + c.ch1.length + ' paired samples • ' + path);
    });
}
function onPackClicked() {
    safeAction('compressed capture', function () {
        const c = state.lastCapture;
        if (!c) throw Error('No real AD3 capture. Press CAPTURE before PACK REAL CAPTURE');
        const packed = Archive.packCapture(c),
            restored = Archive.unpackCapture(packed.encoded);
        if (restored.ch1.length !== c.ch1.length || restored.ch2.length !== c.ch2.length || restored.fHz !== c.fHz ||
            !c.ch1.every((v, i) => Object.is(v, restored.ch1[i])) ||
            !c.ch2.every((v, i) => Object.is(v, restored.ch2[i])))
            throw Error('Compressed capture failed full-array lossless verification');
        const path = filePath('ac_network_last_ad3_capture_packed.json');
        if (!FileIO.writeJson(path, {
            schemaVersion: 1,
            codec: 'pako-deflate/base64-js',
            payload: packed.encoded
        }))
            throw Error(FileIO.lastError);
        ui.set('packedResult', 'REAL DATA PACKED  ' + (100 * (1 - packed.ratio)).toFixed(1) + '% smaller   •   lossless PASS');
        setStatus('REAL AD3 CAPTURE COMPRESSED & VERIFIED • ' + path);
    });
}
// PAGE 09 — EDUCATIONAL FIT EXPLORER. Simulation does not access any device.
function makeSyntheticFitSamples() {
    const s = settings(),
        f = num('fitDemoHz', 100, 20000),
        cycles = num('fitDemoCycles', 1, 10),
        noisePct = num('fitDemoNoise', 0, 20),
        harmonicPct = num('fitDemoHarmonic', 0, 20);
    const n = 1024,
        rate = n * f / cycles,
        model = Circuit.transfer(s.type, s.R, s.Cnf, f);
    const vin = [],
        vout = [];
    let randomState = 2463534242;
    // deterministic LCG; reproducible, not a physical noise source
    function noise() {
        randomState = (Math.imul(1664525, randomState) + 1013904223) >>> 0;
        return (randomState / 4294967296 - 0.5) * Math.sqrt(12);
    }
    for (let i = 0; i < n; i++) {
        const theta = 2 * PI * f * i / rate + 0.23;
        vin.push(0.8 * Math.sin(theta) + 0.11 + 0.8 * noisePct / 100 * noise()
            + 0.8 * harmonicPct / 100 * Math.sin(2 * theta + 0.4));
        vout.push(0.8 * model.gain * Math.sin(theta + model.phaseDeg * PI / 180) - 0.07
            + 0.8 * model.gain * noisePct / 100 * noise()
            + 0.8 * model.gain * harmonicPct / 100 * Math.sin(2 * theta + 0.7));
    }
    return {
        fHz: f,
        rateHz: rate,
        ch1: vin,
        ch2: vout,
        topology: s.type,
        model,
        source: 'SYNTHETIC'
    };
}
/**
 * EDUCATIONAL PLOT — compare sampled voltages with the fitted model.
 *
 * Residual = sample - fit, not an independently calibrated error measurement.
 * The synthetic explorer allows deliberate noise and harmonic changes; it is
 * never presented as an AD3 observation.
 */
function renderFitExplorer() {
    const capture = state.fitView === 'AD3' ? state.lastCapture : makeSyntheticFitSamples();
    if (!capture) {
        ui.set('fitSource', 'NO AD3 CAPTURE — first measure on Page 04, or run the synthetic demonstration.');
        return;
    }
    const f = capture.fHz,
        rate = capture.rateHz;
    const result = Capture.analyzeTwoChannel(capture.ch1, capture.ch2, rate, f,
        state.fitView === 'SYNTHETIC' ? { synthetic: true } : undefined);
    const A = result.inputFit,
        B = result.outputFit;
    const indices = [],
        ms = [],
        vin = [],
        vout = [],
        aFit = [],
        bFit = [],
        aErr = [],
        bErr = [];
    const n = Math.min(capture.ch1.length, capture.ch2.length),
        step = Math.max(1, Math.floor(n / 600));
    for (let i = 0; i < n; i += step) {
        const x = Number(capture.ch1[i]),
            y = Number(capture.ch2[i]);
        const xHat = Capture.predictSample(A, rate, f, i),
            yHat = Capture.predictSample(B, rate, f, i);
        indices.push(i);
        ms.push(1000 * i / rate);
        vin.push(x);
        vout.push(y);
        aFit.push(xHat);
        bFit.push(yHat);
        aErr.push(x - xHat);
        bErr.push(y - yHat);
    }
    plots.clear('fitOverlay');
    plots.configure('fitOverlay', {
        title: 'SAMPLES + LEAST-SQUARES RECONSTRUCTION',
        chartType: 'XY',
        xLabel: 'Time (ms)',
        yLabel: 'Voltage (V)',
        showGrid: true,
        showLegend: true,
        xAuto: true,
        yAuto: true
    });
    plots.setSeries('fitOverlay', 'CH1 samples', ms, vin, {
        color: colors.a,
        width: 1.1
    });
    plots.setSeries('fitOverlay', 'CH1 fitted', ms, aFit, {
        color: '#9ccaff',
        width: 2.5
    });
    plots.setSeries('fitOverlay', 'CH2 samples', ms, vout, {
        color: colors.b,
        width: 1.1
    });
    plots.setSeries('fitOverlay', 'CH2 fitted', ms, bFit, {
        color: colors.measured,
        width: 2.5
    });
    plots.clear('fitResidual');
    plots.configure('fitResidual', {
        title: 'RESIDUAL = SAMPLE − FIT (ZOOMED Y AXIS)',
        chartType: 'XY',
        xLabel: 'Time (ms)',
        yLabel: 'Residual (V)',
        showGrid: true,
        showLegend: true,
        xAuto: true,
        yAuto: true
    });
    plots.setSeries('fitResidual', 'CH1 residual', ms, aErr, {
        color: colors.a,
        width: 1.5
    });
    plots.setSeries('fitResidual', 'CH2 residual', ms, bErr, {
        color: colors.b,
        width: 1.5
    });
    const real = state.fitView === 'AD3';
    const circuitInfo = real ? (capture.topology + ' • R ' + capture.R + ' ohm • C ' + capture.Cnf + ' nF') :
        (capture.topology + ' • generated from current model settings');
    ui.set('fitSource', (real ? 'PHYSICAL AD3 • ' : 'SYNTHETIC DEMONSTRATION • ') +
        circuitInfo + ' • ' + f.toFixed(1) + ' Hz • ' + n + ' paired samples • ' +
        (n * f / rate).toFixed(2) + ' cycles • ' + rate.toFixed(1) + ' samples/s');
    function quality(name, fit) {
        return name + '  RMS residual ' + (1000 * fit.rmsResidualV).toFixed(3) + ' mV   •   ' +
            fit.residualPct.toFixed(2) + '% of fitted peak   •   R² ' + fit.rSquared.toFixed(5);
    }
    ui.set('fitQualityIn', quality('CH1 Vin:', A));
    ui.set('fitQualityOut', quality('CH2 Vout:', B));
    ui.set('fitCoeffs', 'CH1: d=' + A.offset.toFixed(5) + ' V, a=' + A.sinCoef.toFixed(5) + ', b=' + A.cosCoef.toFixed(5) +
        '     CH2: d=' + B.offset.toFixed(5) + ' V, a=' + B.sinCoef.toFixed(5) + ', b=' + B.cosCoef.toFixed(5));
    ui.set('fitComparison', 'FITTED GAIN ' + result.gain.toFixed(5) + ' (' + result.db.toFixed(3) + ' dB)' +
        '   •   CH2 − CH1 PHASE ' + result.phaseDeg.toFixed(3) + '°');
    ui.set('fitInterpretation', (real ? 'These residuals come from actual AD3 voltage samples. ' : 'These residuals come from generated values ONLY. ') +
        'Residuals show how well a single sine at the COMMANDED frequency represents the samples; ' +
        'they do not establish calibrated measurement accuracy.' +
        ((A.residualPct > 5 || B.residualPct > 5) ? ' Large residual: inspect noise, harmonics, clipping or frequency mismatch.' : ''));
}
function onFitDemoClicked() {
    safeAction('synthetic fit', function () {
        state.fitView = 'SYNTHETIC';
        renderFitExplorer();
    });
}
function onFitPhysicalClicked() {
    if (!state.lastCapture) {
        ui.set('fitSource', 'NO PHYSICAL AD3 CAPTURE YET — take a real capture on Page 04 first.');
        return;
    }
    safeAction('real fit', function () {
        state.fitView = 'AD3';
        renderFitExplorer();
    });
}
function onFitDemoChanged() {
    if (state.fitView === 'SYNTHETIC') safeAction('synthetic fit update', renderFitExplorer);
}
function onQuickQuestionClicked() {
    safeAction('student question', function () {
        const s = settings(),
            x = s.f / s.params.fc,
            h = Circuit.transfer(s.type, s.R, s.Cnf, s.f);
        ui.set('challengeAnswer', 'At ' + s.f.toFixed(0) + ' Hz, f/fc=' + x.toFixed(2) + ', predicted gain=' + h.gain.toFixed(3) +
            ', phase=' + h.phaseDeg.toFixed(1) + '°. Explain why low-pass and high-pass differ.');
        setStatus('Challenge generated from CURRENT model settings. Compare with real AD3 values on Notebook.');
    });
}
function onInitialize() {
    updateHardwareIndicators();
    try {
        renderTheory();
        updateNotebook();
        renderFitExplorer();
        setStatus('READY • Start with MODEL. W1 is OFF until you verify wiring and explicitly ARM on the AD3 tab.');
    }
    catch (e) {
        setStatus('INITIALIZATION ERROR: ' + String(e));
    }
}
function onShutdown() {
    emergencyStop('Panel shutdown cleanup');
}
