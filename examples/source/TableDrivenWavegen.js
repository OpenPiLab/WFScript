/**
 * WFScript Example: Table-Driven Wavegen — Multi-Trace Sweep
 * Device: Simulation preview, or deliberate Physical Device selection + W1 command.
 * Wiring: On AD3 loopback W1 -> CH1+, GND -> CH1-. Do not wire to an energized circuit.
 * Simulation: PREVIEW ALL (NO I/O) creates calculated MODEL ONLY traces.
 * This is a standalone Panel demonstration, not a TestRunner/archived report.
 * v5.6.70.3 packaging: physical-control implementation preserved from the
 * user-tested multi-trace example; script logic is unchanged.
 */
/**
 * WFScript v5.6.70.2 — Table-Driven Wavegen MULTI-TRACE Lab
 * Demonstrates that the Experiment Data Table is an INPUT, not just an output.
 *
 * HOW IT WORKS:
 *  1. User edits the cells in the editable recipeTable widget.
 *  2. readRecipe() calls ui.get('recipeTable') and validates each row.
 *  3. Each entry becomes {frequencyHz, amplitudeVpk, offsetV, dwellMs, minRms, maxRms}.
 *  4. JavaScript uses those values to PREVIEW, configure Wavegen W1, or acquire CH1.
 *  5. Each enabled row adds its OWN named waveform series to the Panel plot.
 *     On a real AD3, plots.fromScope() copies the actual CH1 acquisition BEFORE
 *     the next row starts. In simulation it plots calculated, clearly labeled data.
 *  6. Step results are written to a separate read-only Experiment Data Table.
 *
 * Startup does not enable outputs. PREVIEW buttons never operate hardware.
 * Physical W1 operations require deliberate operator button clicks and are capped
 * to low amplitudes. Safely connect W1 -> CH1+ and GND -> CH1- before Run All.
 * Real output is stopped in every automatic sweep exit path and by Panel Stop.
 *
 * THIS IS AN EXPERIMENT SCRIPT, NOT THE NATIVE TEST EXECUTIVE. Its PASS/FAIL
 * compares to operator-entered limits and is not archived by TestRunner.
 */

const START_RECIPE = [
    ['100', '0.50', '0.00', '250', '0.29', '0.42', 'yes'],
    ['250', '0.75', '0.00', '250', '0.46', '0.59', 'yes'],
    ['500', '1.00', '0.00', '250', '0.64', '0.78', 'yes'],
    ['1000', '1.00', '0.00', '250', '0.64', '0.78', 'yes'],
    ['2000', '0.50', '0.00', '250', '0.29', '0.42', 'no'],
    ['', '', '', '', '', '', ''],
    ['', '', '', '', '', '', '']
];
let cursor = 0;
let activeW1 = false;
let results = [];
// The native Panel plot allows 12 series; this example allows 10 enabled rows.
const TRACE_COLORS = [
    '#47cce4', '#f3bf46', '#92d36e', '#ed85c0', '#a79be9',
    '#ff9167', '#5cc2a1', '#d4d66d', '#8aafe5', '#ee7c7c'
];
const SCOPE_RATE_HZ = 250000;
const SCOPE_SAMPLES = 8192;
function traceName(step, measured) {
    // Stable and unique across a sweep, including repeated frequency settings.
    return 'Row ' + step.sourceRow + ' | ' + step.hz + ' Hz | ' +
        (measured ? 'CH1 measured' : 'MODEL ONLY');
}
function traceColor(index) { return TRACE_COLORS[index % TRACE_COLORS.length]; }
function clearTraces() {
    plots.clear('wavePreview');
    plots.configure('wavePreview', {
        title: 'Recipe row waveforms — each trace retained',
        xLabel: 'Time (s)', yLabel: 'Voltage (V)',
        showLegend: true, showGrid: true, xAuto: true, yAuto: true
    });
}


function status(message) {
    ui.set('recipeStatus', message);
    ui.set('runStatus', message);
    ui.set('resultsStatus', message);
    print('[Table Recipe] ' + message);
}
function physical() { return Device.SN !== 'DEMO'; }
function fixed(value, digits) { return Number(value).toFixed(digits); }
function stopOutput() {
    // Stop is safe and idempotent, even when W1 is already off.
    try { Scope1.stop(); } catch (e) { print('Scope stop: ' + String(e)); }
    try { Wavegen1.stop(); } finally { activeW1 = false; }
}
function numeric(cell, name, rowNumber, low, high) {
    const value = Number(String(cell).trim());
    if (!isFinite(value) || String(cell).trim() === '' || value < low || value > high)
        throw Error('Table row ' + rowNumber + ': ' + name + ' must be ' + low + ' to ' + high);
    return value;
}
// v5.6.70.2: ui.get() returns true native JavaScript arrays.
// No Qt-specific conversion helper is necessary.
function currentRecipeRows() {
    const rows = ui.get('recipeTable');
    if (!Array.isArray(rows) || rows.length > 20)
        throw Error('recipeTable must return an array of up to 20 rows');
    rows.forEach(function(row, i) {
        if (!Array.isArray(row) || row.length !== 7)
            throw Error('recipeTable row ' + (i + 1) + ' must have seven cells');
    });
    return rows;
}
function readRecipe() {
    // Read CURRENT operator-edited cells as native JavaScript arrays.
    const cells = currentRecipeRows();
    const out = [];
    for (let i = 0; i < cells.length; i++) {
        const row = cells[i];
        if (row.length !== 7)
            throw Error('Table row ' + (i + 1) + ': expected seven columns');
        const clean = row.map(function(v) { return String(v === null || v === undefined ? '' : v).trim(); });
        if (clean.every(function(v) { return v === ''; })) continue; // blank entry slot
        const runFlag = clean[6].toLowerCase();
        if (['yes','y','true','1','no','n','false','0'].indexOf(runFlag) < 0)
            throw Error('Table row ' + (i + 1) + ': Run? must be yes or no');
        const hz = numeric(clean[0], 'Frequency (Hz)', i + 1, 100, 5000);
        const amplitude = numeric(clean[1], 'Amplitude (Vpk)', i + 1, 0.1, 1.5);
        const offset = numeric(clean[2], 'Offset (V)', i + 1, -0.5, 0.5);
        const dwell = numeric(clean[3], 'Dwell (ms)', i + 1, 100, 1000);
        const minRms = numeric(clean[4], 'RMS min (V)', i + 1, 0, 2);
        const maxRms = numeric(clean[5], 'RMS max (V)', i + 1, 0, 2);
        if (Math.abs(offset) + amplitude > 2.0)
            throw Error('Table row ' + (i + 1) + ': |offset| + Vpk may not exceed 2 V');
        if (minRms >= maxRms)
            throw Error('Table row ' + (i + 1) + ': RMS min must be less than RMS max');
        out.push({sourceRow:i+1, hz:hz, amplitude:amplitude, offset:offset,
                  dwell:dwell, minRms:minRms, maxRms:maxRms,
                  enabled:['yes','y','true','1'].indexOf(runFlag) >= 0});
    }
    const enabled = out.filter(function(s) { return s.enabled; });
    if (enabled.length === 0) throw Error('Enter at least one enabled row (Run? = yes)');
    if (enabled.length > 10) throw Error('Maximum ten enabled rows for this demonstration');
    return enabled;
}
function toResult(step, actual, verdict) {
    return [String(step.sourceRow), String(step.hz), fixed(step.amplitude,2),
            fixed(step.amplitude / Math.SQRT2,4),
            actual === null ? '' : fixed(actual,4),
            fixed(step.minRms,3), fixed(step.maxRms,3), verdict];
}
function writeResults() {
    ui.set('sweepResults', results);
}
function updateReadouts(step) {
    ui.set('currentHz',step.hz);
    ui.set('currentVpk',step.amplitude);
    ui.set('currentExpectedRms',step.amplitude/Math.SQRT2);
}
function addModelTrace(step, index) {
    // Same elapsed-time span as an 8192-sample, 250 kS/s physical capture.
    // These are calculated predictions, never measured Scope samples.
    const x=[], y=[];
    const points = 2048;
    const duration = SCOPE_SAMPLES / SCOPE_RATE_HZ;
    for (let i=0; i<points; i++) {
        const t = duration*i/(points-1);
        x.push(t);
        y.push(step.offset + step.amplitude*Math.sin(2*Math.PI*step.hz*t));
    }
    plots.setSeries('wavePreview', traceName(step,false), x, y,
                    {color:traceColor(index),width:1.5});
}
function addMeasuredTrace(step, index) {
    // IMPORTANT: the actual native Scope buffer is overwritten by the NEXT capture.
    // fromScope() copies this capture into its own named Panel plot series NOW.
    // Never clear the plot here, and never substitute a calculated sine wave.
    const name = traceName(step,true);
    plots.fromScope('wavePreview', 1, name);
    plots.styleSeries('wavePreview', name,
                      {color:traceColor(index),width:1.6});
}
function validateAction() {
    const steps=readRecipe();
    cursor=0;
    status('VALIDATED: ' + steps.length + ' enabled rows. ' +
           'First: ' + steps[0].hz + ' Hz / ' + fixed(steps[0].amplitude,2) +
           ' Vpk. Values were read from the live editable table.');
    return steps;
}
function addRow() {
    const rows=currentRecipeRows();
    if (rows.length >= 20) throw Error('This example limits recipe tables to 20 rows');
    rows.push(['','','','','','','']);
    if(!ui.set('recipeTable',rows)) throw Error('Could not update recipeTable');
    cursor=0;results=[];writeResults();clearTraces();
    status('Added editable blank row ' + rows.length + '. Cleared stale traces.');
}
function resetRecipe() {
    stopOutput();
    ui.set('recipeTable',START_RECIPE.map(function(r){return r.slice();}));
    cursor=0;results=[];writeResults();clearTraces();
    status('Restored example recipe; previous traces cleared. No hardware output enabled.');
}
function previewAll() {
    const steps=validateAction();
    // A preview must not start hardware output. If manual W1 was already on,
    // stop it before plotting predictions.
    if(activeW1) stopOutput();
    clearTraces();
    results=[];
    for (let i=0; i<steps.length; i++) {
        addModelTrace(steps[i],i);
        results.push(toResult(steps[i],null,'MODEL ONLY'));
    }
    writeResults();
    updateReadouts(steps[0]);
    cursor=0;
    status('MODEL ONLY: '+steps.length+' separate predicted waveform traces retained. '+
           'No W1 output and NO CH1 acquisition occurred.');
}
function configureW1(step) {
    Wavegen1.stop();
    activeW1=false;
    Wavegen1.Channel1.enable=true;
    Wavegen1.Channel2.enable=false;
    Wavegen1.Channel1.Mode.text='Simple';
    Wavegen1.Channel1.Simple.Type.text='Sine';
    Wavegen1.Channel1.Simple.Frequency.value=step.hz;
    Wavegen1.Channel1.Simple.Amplitude.value=step.amplitude;
    Wavegen1.Channel1.Simple.Offset.value=step.offset;
    Wavegen1.Channel1.Simple.Phase.value=0;
    if (Wavegen1.Channel1.Simple.Type.text !== 'Sine')
        throw Error('SDK rejected sine-wave type');
    if (!Wavegen1.run()) throw Error('W1 failed: ' + Wavegen1.lastError);
    activeW1=true;
    const s=Wavegen1.status(1);
    if (s && s.carrierFunction && s.carrierFunction !== 'Sine')
        throw Error('SDK Wavegen type mismatch: ' + s.carrierFunction);
}
function applyNext() {
    const recipe=readRecipe();
    // Step through the recipe while retaining every completed row on the plot.
    // When cycling back to the beginning, start a clean set of traces.
    if (cursor>=recipe.length) {cursor=0;results=[];writeResults();clearTraces();}
    if (cursor===0 && results.length===0) clearTraces();
    const index=cursor;
    const step=recipe[index];
    updateReadouts(step);
    if (physical()) {
        // Deliberately requested operator action. This now performs one physical
        // capture as well as setting W1; W1 stays active until STOP or next row.
        try {
            configureScope();
            configureW1(step);
            wait(step.dwell/1000);
            const measured=acquireAcRms();
            addMeasuredTrace(step,index);
            const verdict=measured>=step.minRms && measured<=step.maxRms ? 'PASS' : 'FAIL';
            results.push(toResult(step,measured,verdict));
            writeResults();
            status('PHYSICAL row '+step.sourceRow+': CH1 CAPTURED '+step.hz+' Hz, RMS '+
                fixed(measured,4)+' V — '+verdict+'. Trace retained; W1 ACTIVE until STOP W1.');
        } catch(e) {stopOutput();throw e;}
    } else {
        addModelTrace(step,index);
        results.push(toResult(step,null,'MODEL ONLY'));
        writeResults();
        status('SIMULATED row '+step.sourceRow+': '+step.hz+
               ' Hz. Predicted waveform retained; no hardware output or capture.');
    }
    cursor++;
}
function configureScope() {
    Scope1.Time.Rate.value=SCOPE_RATE_HZ;
    Scope1.Time.Samples.value=SCOPE_SAMPLES;
    Scope1.Channel1.Range.value=5;
    Scope1.Channel1.Offset.value=0;
    Scope1.Channel1.Attenuation.value=1;
    Scope1.Trigger.Trigger.text='None';
}
function acquireAcRms() {
    if (!Scope1.single()) throw Error('CH1 acquisition start failed: '+Scope1.lastError);
    if (!Scope1.wait(3)) throw Error('CH1 acquisition timed out: '+Scope1.lastError);
    const m=Scope1.Channel1.statistics();
    if (!m || Number(m.count)<2048) throw Error('Not enough CH1 samples');
    const mean=Number(m.mean), rms=Number(m.rms);
    if (!isFinite(mean) || !isFinite(rms)) throw Error('Invalid CH1 statistics');
    const value=Math.sqrt(Math.max(0,rms*rms-mean*mean));
    return value;
}
function runAll() {
    const recipe=readRecipe();
    const real=physical();
    results=[];writeResults();cursor=0;
    stopOutput(); // always begin from safe state
    clearTraces(); // only clear ONCE per sweep; per-row traces remain
    let failures=0;
    try {
        if(real)configureScope();
        for(let i=0;i<recipe.length;i++) {
            const step=recipe[i];
            updateReadouts(step);
            if(real) {
                configureW1(step);
                wait(step.dwell/1000);
                const measured=acquireAcRms();
                // Copy real CH1 samples into a new series before the next capture.
                addMeasuredTrace(step,i);
                const verdict=measured>=step.minRms && measured<=step.maxRms ? 'PASS' : 'FAIL';
                if(verdict==='FAIL')failures++;
                results.push(toResult(step,measured,verdict));
                status('PHYSICAL row '+step.sourceRow+'/'+recipe.length+': '+step.hz+
                       ' Hz, captured CH1 RMS '+fixed(measured,4)+' V — '+verdict+
                       ' ('+(i+1)+' retained traces)');
            } else {
                // Explicitly a model, never present it as captured hardware data.
                addModelTrace(step,i);
                results.push(toResult(step,null,'MODEL ONLY'));
                status('MODEL ONLY row '+(i+1)+'/'+recipe.length+': '+step.hz+
                       ' Hz ('+(i+1)+' retained predicted traces)');
            }
            writeResults();
        }
        cursor=recipe.length;
    } finally {
        stopOutput(); // run failures, scope exceptions and end of sweep all stop W1
    }
    status(real ?
        'PHYSICAL sweep complete; '+recipe.length+' separate CH1 captures on Panel plot, '+
        failures+' limit failures. W1 OFF.' :
        'SIMULATION complete; '+recipe.length+
        ' separate MODEL ONLY traces. NO hardware measurement.');
}
function safeAction(id,fn) {
    ui.on(id,'clicked',function(){
        try {fn();}
        catch(e) {
            try{stopOutput();}catch(stopError){print('Stop warning: '+String(stopError));}
            status('ERROR: '+String(e));
        }
    });
}
safeAction('validateRecipe',validateAction);
safeAction('addRecipeRow',addRow);
safeAction('resetRecipe',resetRecipe);
safeAction('previewRecipe',previewAll);
safeAction('applyNext',applyNext);
safeAction('runAll',runAll);
safeAction('stopW1',function(){stopOutput();status('W1 STOPPED by operator.');});
safeAction('resetCursor',function(){stopOutput();cursor=0;results=[];writeResults();clearTraces();status('Back to first enabled recipe row. Traces cleared; W1 OFF.');});

// Edited rows arrive as native JavaScript arrays, just like ui.get().
// A user edit updates the pending recipe but does not enable hardware.
ui.on('recipeTable','valueChanged',function(rows){
    cursor=0;
    // Stop active hardware FIRST; even a plot error must not leave W1 running.
    try { if(activeW1)stopOutput(); }
    catch(e) {print('Stop after edit: '+String(e));}
    results=[];writeResults();
    try {clearTraces();}catch(e){print('Clear stale plot: '+String(e));}
    try {
        if (!Array.isArray(rows) || !rows.every(Array.isArray))
            throw Error('Expected native row arrays from table edit');
        const count = rows.length;
        status('RECIPE EDITED: '+count+' table rows. Old traces cleared; validate before running.');
    } catch(e) {
        status('RECIPE EDIT received invalid rows: '+String(e));
    }
});

// STARTUP: Show mode and initialize read-only results. NO hardware calls.
writeResults();
status('READY — '+(physical()?'PHYSICAL DEVICE SELECTED: only explicit W1 buttons enable output.':
    'SIMULATION MODE: Preview/Run All will not access hardware.')+
    ' Edit the recipe, then click VALIDATE & READ.');
