/**
 * WFScript Example: Table-to-Plot — Limits and Observations
 * Device: Simulation ONLY. No AD3 instrument is configured or enabled.
 * Purpose: Use an editable Experiment Table as an operator-owned dataset,
 *          validate every row, draw multiple XY series and build a result log.
 *
 * 1. Page 00: enter X, nominal, observed, lower and upper values.
 * 2. Press VALIDATE & PLOT (or RESTORE DEFAULTS).
 * 3. Page 01: inspect the plotted curves and calculated in/out-of-limit rows.
 *
 * Runtime table edits are not written into the project's design-time defaults.
 * For persistent defaults, edit the table's Initial Rows in Properties while
 * in Design mode; you can also export a runtime table with right-click > CSV.
 * Requires WFScript v5.6.70.2+ (ui.get() returns native JavaScript arrays).
 */
const FACTORY_DATA = [
    ['100', '1.00', '1.03', '0.90', '1.10'],
    ['200', '1.30', '1.28', '1.20', '1.40'],
    ['300', '1.50', '1.57', '1.40', '1.60'],
    ['400', '1.80', '1.65', '1.70', '1.90'],
    ['500', '2.00', '2.06', '1.90', '2.10'],
    ['600', '2.10', '2.12', '2.00', '2.20']
];
let current = [];
let inputDirty = true;

function status(message) {
    ui.set('editStatus', message);
    ui.set('plotStatus', message);
    print('[Table-to-Plot] ' + message);
}

function numericCell(raw, rowIndex, column) {
    const source = raw === null || raw === undefined ? '' : String(raw).trim();
    const value = Number(source);
    if (!source || !isFinite(value) || Math.abs(value) > 1000000)
        throw Error('Row ' + rowIndex + ': ' + column + ' must be a finite number (absolute value <= 1,000,000).');
    return value;
}

function readAndValidate() {
    const table = ui.get('limitInputTable');
    if (!Array.isArray(table))
        throw Error('The Experiment Table must return a native JavaScript array. Requires WFScript v5.6.70.2+.');
    if (table.length > 40)
        throw Error('The example supports up to 40 data rows.');
    const parsed = [];
    table.forEach(function(row, index) {
        const number = index + 1;
        if (!Array.isArray(row) || row.length !== 5)
            throw Error('Row ' + number + ' requires five cells (X, nominal, observed, lower, upper).');
        if (row.every(function(cell) { return cell === null || cell === undefined || String(cell).trim() === ''; }))
            return; // an entirely blank row is intentional and safely ignored
        const x = numericCell(row[0], number, 'X');
        const nominal = numericCell(row[1], number, 'Nominal');
        const observed = numericCell(row[2], number, 'Observed');
        const lower = numericCell(row[3], number, 'Lower');
        const upper = numericCell(row[4], number, 'Upper');
        if (lower > upper)
            throw Error('Row ' + number + ': lower limit is greater than upper limit.');
        if (nominal < lower || nominal > upper)
            throw Error('Row ' + number + ': nominal value must lie within its limits.');
        parsed.push({ sourceRow: number, x, nominal, observed, lower, upper,
                      inLimit: observed >= lower && observed <= upper });
    });
    if (parsed.length < 2)
        throw Error('Enter at least two complete data rows.');
    parsed.sort(function(a, b) { return a.x - b.x; });
    for (let i = 1; i < parsed.length; ++i)
        if (parsed[i].x === parsed[i - 1].x)
            throw Error('Duplicate X value: ' + parsed[i].x + ' (rows ' +
                        parsed[i - 1].sourceRow + ' and ' + parsed[i].sourceRow + ').');
    return parsed;
}

function drawAndSummarize(points) {
    current = points;
    inputDirty = false;
    const x = points.map(p => p.x);
    plots.clear('limitCurve');
    plots.configure('limitCurve', {
        chartType: 'XY',
        title: 'Entered data — observed vs. limits',
        xLabel: 'X (user-defined)',
        yLabel: 'Value (user-defined)',
        showGrid: true, showLegend: true, xAuto: true, yAuto: true
    });
    plots.setSeries('limitCurve', 'Upper limit', x, points.map(p => p.upper),
                    { color: '#efa46b', width: 1.5 });
    plots.setSeries('limitCurve', 'Nominal', x, points.map(p => p.nominal),
                    { color: '#8fa9f2', width: 2.0 });
    plots.setSeries('limitCurve', 'Lower limit', x, points.map(p => p.lower),
                    { color: '#efa46b', width: 1.5 });
    plots.setSeries('limitCurve', 'Observed (entered)', x, points.map(p => p.observed),
                    { color: '#46cfb8', width: 2.3, markerSize: 6 });
    const bad = points.filter(p => !p.inLimit);
    if (bad.length)
        plots.setSeries('limitCurve', 'Out-of-limit points', bad.map(p => p.x),
                        bad.map(p => p.observed),
                        { color: '#ff5e81', width: 0.5, markerSize: 9 });

    const results = points.map(function(p) {
        return [String(p.sourceRow), String(p.x), p.nominal.toFixed(3),
                p.observed.toFixed(3), p.lower.toFixed(3), p.upper.toFixed(3),
                p.inLimit ? 'IN LIMITS' : 'OUTSIDE'];
    });
    ui.set('limitResults', results);
    ui.set('pointCount', points.length);
    ui.set('outsideCount', bad.length);
    const maxDeviation = points.reduce((max, p) =>
        Math.max(max, Math.abs(p.observed - p.nominal)), 0);
    ui.set('largestDeviation', maxDeviation);
    status('EVALUATED ' + points.length + ' point(s): ' +
           (points.length - bad.length) + ' in limits, ' + bad.length +
           ' outside. These are user-entered observations, NOT hardware measurements.');
}

function validateAndPlot() {
    // Keep a failed validation from leaving an old trace labeled as current.
    current = [];
    inputDirty = true;
    plots.clear('limitCurve');
    ui.set('limitResults', []);
    ui.set('pointCount', 0);
    ui.set('outsideCount', 0);
    ui.set('largestDeviation', 0);
    drawAndSummarize(readAndValidate());
}

function resetValues() {
    ui.set('limitInputTable', FACTORY_DATA.map(row => row.slice()));
    validateAndPlot();
}

function safeClick(id, action) {
    ui.on(id, 'clicked', function() {
        try { action(); }
        catch (error) { status('ERROR: ' + String(error)); }
    });
}

safeClick('plotEnteredData', validateAndPlot);
safeClick('restoreLimitData', resetValues);
safeClick('clearLimitResults', function() {
    current = []; inputDirty = true;
    plots.clear('limitCurve');
    ui.set('limitResults', []);
    ui.set('pointCount', 0);
    ui.set('outsideCount', 0);
    ui.set('largestDeviation', 0);
    status('Plot and analysis cleared. Your entered table values were kept.');
});

ui.on('limitInputTable', 'valueChanged', function(rows) {
    // An incomplete edit should not be evaluated until the operator commits it.
    current = []; inputDirty = true;
    plots.clear('limitCurve');
    ui.set('limitResults', []);
    ui.set('pointCount', 0);
    ui.set('outsideCount', 0);
    ui.set('largestDeviation', 0);
    status('EDITED ' + rows.length + ' table row(s). Press VALIDATE & PLOT to refresh the graph.');
});

try { validateAndPlot(); }
catch (error) { status('ERROR: ' + String(error)); }
