// ┌─ WFScript LEARNING LAB: File I/O & Operator Dialogs
// │ Level: Beginner
// │
// │ GOAL: Write and read CSV, JSON and binary data; use asynchronous operator dialogs.
// │ BEFORE RUN: No connected device. FileIO writes only to the application-managed data
// │    directory unless a user explicitly grants another file.
// │ TRY FIRST: On Data Logging, press START LOGGING and then STOP LOGGING.
// │ TRACE: `startLog` → `onStartLogClicked()` → timer/file work; dialog buttons → `Tool`
// │    callbacks → label updates.
// │ EDIT SAFELY: Change one field in the saved JSON configuration and inspect the value
// │    after reading it back.
// │ REMEMBER: Dialog results arrive through callbacks; saving a file and asking an operator
// │    are separate operations.
// │
// │ EVENT TIP: Select `startLog` in Design mode → Properties → Events → Go to Handler.
// │ Dynamic UI events can use `ui.on` — do NOT duplicate a saved designer binding.
// │ FULL LAB GUIDE: examples/guides/ScriptUtilityLab.md (in the complete source ZIP).
// └────────────────────────────────────────────────────────────────────────────

/** WFScript v0.2-dev.3 — File & Operator Utilities (Simulation only). */
let logger = 0,
    samples = 0,
    rows = [['seconds', 'voltage_V']],
    startMs = 0;
const csvPath = FileIO.dataPath('measurements.csv');
const jsonPath = FileIO.dataPath('config.json');
const binPath = FileIO.dataPath('packet.bin');
function log(msg, level) {
    const record = {
        append: {
            level: level || 'INFO',
            source: 'Script Utilities',
            message: String(msg)
        }
    };
    ui.set('fileEventLog', record);
    ui.set('toolEventLog', record);
    print('[Utilities] ' + msg);
}
function status(msg) {
    ui.set('fileStatus', msg);
}
function update() {
    ui.set('fileSampleCount', samples);
}
function stopLogging() {
    if (logger) {
        clearInterval(logger);
        logger = 0;
        log('250 ms acquisition timer stopped');
    }
}
/**
 * CORE LESSON — The button initiates the synthetic CSV logger. A timer generates readings after
 * this function returns; Stop / Panel shutdown must cancel that timer.
 */
function onStartLogClicked() {
    if (logger) return;
    // Start fresh; use QSaveFile for initial creation and bounded append thereafter.
    samples = 0;
    rows = [['seconds', 'voltage_V']];
    startMs = Date.now();
    FileIO.writeCsv(csvPath, rows);
    update();
    logger = setInterval(function () {
        let t = (Date.now() - startMs) / 1000;
        let v = 3.3 + 0.04 * Math.sin(t * 2.3);
        if (!Number.isFinite(v)) return;
        FileIO.appendText(csvPath, t.toFixed(3) + ',' + v.toFixed(5) + '\n');
        rows.push([t.toFixed(3), v.toFixed(5)]);
        samples++;
        ui.set('fileVoltage', v);
        update();
        if (samples >= 500) {
            stopLogging();
            log('Stopped at 500 sample example limit', 'WARN');
        }
    }, 250);
    status('Logging every 250 ms to shared PanelData directory');
    log('Started CSV logging to ' + csvPath);
}

function onSaveConfigClicked() {
    FileIO.writeJson(jsonPath, {
        name: 'Synthetic voltage',
        intervalMs: 250,
        limits: {
            low: 3.2,
            high: 3.4
        }
    });
    log('Saved JSON config: ' + jsonPath, 'PASS');
}
function onReadConfigClicked() {
    if (!FileIO.exists(jsonPath)) {
        log('Save JSON first', 'WARN');
        return;
    }
    const c = FileIO.readJson(jsonPath);
    log('JSON configuration: ' + JSON.stringify(c));
}
function onReadCsvClicked() {
    if (!FileIO.exists(csvPath)) {
        log('Start logging first', 'WARN');
        return;
    }
    const data = FileIO.readCsv(csvPath);
    log('CSV records: ' + data.length + '; last row: ' + JSON.stringify(data[data.length - 1]));
}
function onWriteBinaryClicked() {
    const bytes = [0x00, 0x7F, 0x80, 0xFF];
    FileIO.writeBytes(binPath, bytes);
    const back = FileIO.readBytes(binPath);
    log('Binary round trip: ' + JSON.stringify(back), JSON.stringify(back) === JSON.stringify(bytes) ? 'PASS' : 'FAIL');
}
function onExportCsvClicked() {
    Tool.getSaveFile('Save measurements', 'CSV files (*.csv)', function (result) {
        if (!result.accepted) {
            log('CSV export cancelled');
            return;
        }
        FileIO.writeCsv(result.value, rows);
        log('Exported ' + rows.length + ' rows to ' + result.value, 'PASS');
    });
}
function onImportCsvClicked() {
    Tool.getOpenFile('Import CSV', 'CSV files (*.csv)', function (result) {
        if (!result.accepted) {
            log('CSV import cancelled');
            return;
        }
        const data = FileIO.readCsv(result.value);
        log('Imported ' + data.length + ' rows from ' + result.value, 'PASS');
    });
}
function showPrompt(r) {
    const text = r.accepted ? 'Accepted: ' + JSON.stringify(r.value) : 'Operator cancelled/declined';
    ui.set('toolResponse', text);
    log(r.kind + ': ' + text, r.accepted ? 'PASS' : 'INFO');
}
function onAskConfirmClicked() {
    Tool.question('Continue test?', showPrompt);
}
function onAskTextClicked() {
    Tool.getText('Enter unit serial number', 'UUT-001', showPrompt);
}
function onAskNumberClicked() {
    Tool.getNumber('Nominal voltage', 3.3, 0, 5, showPrompt);
}
function onAskItemClicked() {
    Tool.getItem('Select test range', ['5 V', '50 V'], showPrompt);
}
function onAskFileOpenClicked() {
    Tool.getOpenFile('Select an input', 'All files (*)', showPrompt);
}
function onAskFileSaveClicked() {
    Tool.getSaveFile('Select an output', 'All files (*)', showPrompt);
}
function onInitialize() {
    update();
    status('Ready; no file written until you press a button.');
    log('File & Operator Lab ready (Simulation safe)');
}
function onShutdown() {
    stopLogging();
    print('[Utilities] Panel cleanup complete');
}
