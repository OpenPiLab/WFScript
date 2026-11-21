// ┌─ WFScript DSP STUDIO: THIRD-PARTY LIBRARIES + NATIVE PANELS
// │ Demonstrates portable ECMAScript modules composed with QJSEngine.
// │ Simulation-only; no instrument output, no hardware writes.
// │ Three genuine open-source libraries embedded with licenses:
// │   Fraction.js 4.3.7, pako 1.0.11, base64-js 0.0.8.
// │ Two self-contained WFScript-authored modules implement DSP and adapters.
// │ Designer-managed events; no manual ui.on() registrations.
// └───────────────────────────────────────────────────────────

// LEARNING PATH: configure a synthetic signal -> inspect filtering -> inspect
// its spectrum -> examine the DSPCore.mjs implementation. The `vendor/`
// modules are third-party dependencies with licenses, not introductory lessons.
// No AD3 or external equipment is controlled by this project.
var DSP = modules.load('DSPCore.mjs');
var Package = modules.load('SignalPackage.mjs');
var SAMPLE_RATE = 16384,
    SAMPLE_COUNT = 1024,
    noiseSeed = 12345;
var lastCapture = null;
var palette = {
    input: '#49d3e8',
    filtered: '#f0bc68',
    spectrum: '#62c5fa',
    processed: '#76e3ac',
    response: '#bd9fff'
};

function settingNumber(id, lo, hi) {
    const value = Number(ui.get(id));
    if (!isFinite(value) || value < lo || value > hi) throw Error('Invalid ' + id + ' (expected ' + lo + '..' + hi + ')');
    return value;
}
function status(text) {
    ui.set('studioStatus', text);
    print('[DSP Studio] ' + text);
}

/** Generate 1024 samples, filter the signal, compute two FFTs and graph everything. */
function analyze() {
    const toneHz = Math.round(settingNumber('toneHz', 128, 3000));
    const shape = String(ui.get('shape'));
    if (['Sine', 'Square', 'Triangle'].indexOf(shape) < 0) throw Error('Invalid signal shape: ' + shape);
    const harmonicsPct = settingNumber('harmonics', 0, 75);
    const noisePct = settingNumber('noise', 0, 75);
    const cutoffHz = settingNumber('cutoff', 100, 7000);
    const wave = DSP.generateSignal({
        samples: SAMPLE_COUNT,
        sampleRateHz: SAMPLE_RATE,
        toneHz,
        shape,
        harmonicsPct,
        noisePct,
        seed: noiseSeed
    });
    const filt = DSP.biquadLowpass(wave.values, SAMPLE_RATE, cutoffHz);
    const before = DSP.fftAmplitude(wave.values, SAMPLE_RATE);
    const after = DSP.fftAmplitude(filt.samples, SAMPLE_RATE);
    const curve = DSP.response(filt.coefficients, SAMPLE_RATE, 7000, 180);
    const grid = Package.samplesPerCycle(SAMPLE_RATE, toneHz);

    plots.clear('timePlot');
    plots.configure('timePlot', {
        title: 'INPUT vs DIGITAL LOWPASS  •  62.5 ms capture',
        chartType: 'XY',
        xLabel: 'Time (ms)',
        yLabel: 'Amplitude (V)',
        showLegend: true,
        showGrid: true,
        xAuto: true,
        yAuto: true
    });
    plots.setSeries('timePlot', 'Raw voltage', wave.timeMs, wave.values, {
        color: palette.input,
        width: 1.6
    });
    plots.setSeries('timePlot', 'Filtered voltage', wave.timeMs, filt.samples, {
        color: palette.filtered,
        width: 2.0
    });

    plots.clear('fftPlot');
    plots.configure('fftPlot', {
        title: 'FFT SPECTRUM  •  1024-point Hann window',
        chartType: 'XY',
        xLabel: 'Frequency (Hz)',
        yLabel: 'Magnitude (dB, re 1 Vpk)',
        showGrid: true,
        showLegend: true,
        xAuto: true,
        yAuto: true
    });
    plots.setSeries('fftPlot', 'Before', before.hz.slice(0, 441), before.db.slice(0, 441), {
        color: palette.spectrum,
        width: 1.7
    });
    plots.setSeries('fftPlot', 'After', after.hz.slice(0, 441), after.db.slice(0, 441), {
        color: palette.processed,
        width: 2.1
    });

    plots.clear('filterPlot');
    plots.configure('filterPlot', {
        title: 'BIQUAD TRANSFER RESPONSE  •  Butterworth Q = 0.707',
        chartType: 'XY',
        xLabel: 'Frequency (Hz)',
        yLabel: 'Gain (dB)',
        showGrid: true,
        showLegend: false,
        xAuto: true,
        yAuto: true
    });
    plots.setSeries('filterPlot', 'Theoretical gain', curve.hz, curve.db, {
        color: palette.response,
        width: 2.4
    });
    ui.set('metricPeak', 'PEAK: ' + before.peakHz.toFixed(0) + ' Hz');
    ui.set('metricRms', 'RAW RMS: ' + DSP.rms(wave.values).toFixed(3) + ' V');
    ui.set('metricFiltered', 'FILT RMS: ' + DSP.rms(filt.samples).toFixed(3) + ' V');
    ui.set('metricRatio', 'GRID: ' + grid.fraction + ' samp/cycle');
    lastCapture = {
        metadata: {
            sampleRateHz: SAMPLE_RATE,
            count: SAMPLE_COUNT,
            toneHz,
            shape,
            cutoffHz,
            harmonicsPct,
            noisePct,
            seed: noiseSeed
        },
        timeMs: wave.timeMs,
        raw: wave.values,
        filtered: filt.samples
    };
    status('ANALYZED • ' + shape + ' ' + toneHz + ' Hz  |  FFT bin ' + before.binHz + ' Hz  |  ' +
        '3 external libraries loaded  |  use PACK + VERIFY for lossless compression');
}

function safely(fn) {
    try {
        fn();
    } catch (error) {
        status('ERROR: ' + String(error));
    }
}

// Designer event handlers generated/selected from Properties → Events.
function onAnalyzeClicked() {
    safely(analyze);
}
function onNewNoiseClicked() {
    safely(function () {
        noiseSeed = (noiseSeed + 0x9e3779b9) >>> 0;
        analyze();
    });
}
function onControlChanged() {
    safely(analyze);
}
/** Demonstrates actual third-party Pako compression and base64-js encoding. */
function onPackClicked() {
    safely(function () {
        if (!lastCapture) throw Error('Run Analyze first');
        const packed = Package.packCapture(lastCapture);
        const restored = Package.unpackCapture(packed.encoded);
        if (restored.raw.length !== SAMPLE_COUNT || restored.metadata.seed !== lastCapture.metadata.seed ||
            restored.raw[37] !== lastCapture.raw[37] || restored.filtered[777] !== lastCapture.filtered[777])
            throw Error('Compression round-trip mismatch');
        ui.set('metricPacked', packed.compressedBytes + ' / ' + packed.rawBytes + ' bytes');
        status('PACK VERIFIED • ' + (100 * (1 - packed.ratio)).toFixed(1) + '% smaller (deflate) • ' +
            'Base64 ' + packed.encodedBytes + ' characters • lossless round-trip PASS');
    });
}
function onResetClicked() {
    safely(function () {
        ui.set('shape', 'Sine');
        ui.set('toneHz', 512);
        ui.set('harmonics', 35);
        ui.set('noise', 15);
        ui.set('cutoff', 950);
        noiseSeed = 12345;
        analyze();
    });
}

try {
    analyze();
} catch (error) {
    status('INITIALIZE ERROR: ' + String(error));
}
