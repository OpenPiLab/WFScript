// ┌─ WFScript LEARNING LAB: SQLite Database & Data Logging
// │ Level: Beginner–Intermediate
// │
// │ GOAL: Persist synthetic readings in SQLite, run queries, and display results.
// │ BEFORE RUN: No instruments. The database is in WFScript’s shared PanelData directory.
// │ TRY FIRST: Press OPEN DATABASE.
// │ TRACE: `startSqlLog` → `startLogging()` → `Database.exec()`; query callbacks update
// │    table/chart widgets.
// │ EDIT SAFELY: Change the synthetic reading range or a query filter, then refresh the
// │    statistics.
// │ REMEMBER: SQL commands execute asynchronously: the result callback, not the line after
// │    `query()`, owns the returned rows.
// │
// │ EVENT TIP: Select `openSqlDb` in Design mode → Properties → Events → Go to Handler.
// │ Dynamic UI events can use `ui.on` — do NOT duplicate a saved designer binding.
// │ FULL LAB GUIDE: examples/guides/SQLiteDataLab.md (in the complete source ZIP).
// └────────────────────────────────────────────────────────────────────────────

/** WFScript v0.2-dev.5.1 — SQLite Data Lab.
 * Two-page Simulation-safe example: no hardware writes or acquisitions.
 * The database lives in shared PanelData, OUTSIDE .wfpanel; Panels can share it.
 * Database calls are async; ALL results are delivered through callbacks.
 * Page 01: persistent logging, query-driven Experiment Data Table.
 * Page 02: aggregate statistics and error-handling demonstration.
 */
let db = 0,
    opening = false,
    logging = 0,
    samplesThisRun = 0,
    sampleSequence = 0;
const databaseFile = 'engineering_lab.db';
function log(message, level) {
    const payload = {
        append: {
            level: level || 'INFO',
            source: 'SQLite Lab',
            message: String(message)
        }
    };
    ui.set('sqlEvents', payload);
    ui.set('statsEvents', payload);
    print('[SQLite] ' + message);
}
function showStatus(text) {
    ui.set('sqlStatus', text);
    ui.set('statsStatus', text);
}
function requireOk(result, what) {
    if (result.ok) return true;
    const msg = what + ': ' + (result.error || 'unknown error');
    log(msg, 'ERROR');
    showStatus(msg);
    return false;
}
/**
 * CORE LESSON — Starts the asynchronous database-open flow. Follow the callback before looking for
 * query results: opening a database is not a synchronous function return.
 */
function openDatabase() {
    if (db) {
        log('Database is already open (handle ' + db + ')');
        return;
    }
    if (opening) return;
    opening = true;
    showStatus('Opening SQLite database asynchronously...');
    Database.open(databaseFile, function (result) {
        opening = false;
        if (!requireOk(result, 'Open database')) return;
        db = result.db;
        log('Database opened: ' + result.path, 'PASS');
        Database.exec(db,
            'CREATE TABLE IF NOT EXISTS samples (' +
            'id INTEGER PRIMARY KEY, timestamp TEXT NOT NULL, voltage REAL NOT NULL, source TEXT NOT NULL)',
            [], function (created) {
                if (!requireOk(created, 'Create table')) return;
                Database.exec(db, 'CREATE INDEX IF NOT EXISTS idx_samples_time ON samples(timestamp)', [], function (indexed) {
                    if (!requireOk(indexed, 'Create index')) return;
                    showStatus('Open: ' + databaseFile + ' — ready to log and query');
                    refreshRecent();
                    refreshStats();
                });
            });
    });
}
function stopLogging() {
    if (logging) {
        clearInterval(logging);
        logging = 0;
        log('250 ms sample timer stopped');
    }
}
function closeDatabase() {
    stopLogging();
    if (!db) {
        log('Database already closed');
        return;
    }
    const handle = db;
    db = 0;
    Database.close(handle, function (result) {
        if (!requireOk(result, 'Close database')) return;
        showStatus('Database closed. Saved records remain on disk.');
        log('SQLite database closed', 'PASS');
    });
}
function startLogging() {
    if (!db) {
        log('OPEN DATABASE before starting', 'WARN');
        return;
    }
    if (logging) return;
    samplesThisRun = 0;
    log('Started 250 ms simulated voltage logging');
    logging = setInterval(function () {
        if (!db) {
            stopLogging();
            return;
        }
        // Synthetic signal; safe even if a physical AD3 is connected.
        const t = Date.now() / 1000;
        const v = 3.3 + 0.042 * Math.sin(t * 2.1 + sampleSequence * 0.2);
        sampleSequence++;
        const stamp = new Date().toISOString();
        Database.exec(db, 'INSERT INTO samples(timestamp,voltage,source) VALUES(?,?,?)',
            [stamp, Number(v.toFixed(5)), 'Synthetic'], function (result) {
                if (!requireOk(result, 'Insert sample')) {
                    stopLogging();
                    return;
                }
                samplesThisRun++;
                ui.set('sampleCount', samplesThisRun);
                ui.set('sampleVoltage', v);
                if (samplesThisRun % 8 === 0) refreshRecent();
                if (samplesThisRun >= 200) {
                    stopLogging();
                    refreshRecent();
                    refreshStats();
                }
            });
    }, 250);
}
function refreshRecent() {
    if (!db) {
        log('Open database before querying', 'WARN');
        return;
    }
    Database.query(db,
        'SELECT timestamp,voltage,source FROM samples ORDER BY id DESC LIMIT 40', [], function (result) {
            if (!requireOk(result, 'Query recent data')) return;
            // The existing Experiment Data Table accepts arrays of row arrays.
            const rows = result.rows.map(function (r) {
                return [String(r[0]), Number(r[1]).toFixed(5), String(r[2])];
            });
            ui.set('recentSamplesTable', rows);
            log('Displayed ' + rows.length + ' recent rows (persisted on disk)');
        });
}
function filterHigh() {
    if (!db) {
        log('Open database before querying', 'WARN');
        return;
    }
    // Bound value is a SQL parameter, not concatenated into SQL text.
    const minV = Number(ui.get('filterMinVoltage'));
    if (!Number.isFinite(minV)) return;
    Database.query(db,
        'SELECT timestamp,voltage,source FROM samples WHERE voltage>=? ORDER BY id DESC LIMIT 40',
        [minV], function (result) {
            if (!requireOk(result, 'Filtered query')) return;
            ui.set('recentSamplesTable', result.rows.map(r => [String(r[0]), Number(r[1]).toFixed(5), String(r[2])]));
            log('Filter >= ' + minV.toFixed(3) + ' V returned ' + result.rowCount + ' rows');
        });
}
function refreshStats() {
    if (!db) {
        log('Open database to display statistics', 'WARN');
        return;
    }
    Database.query(db,
        'SELECT COUNT(*), MIN(voltage), AVG(voltage), MAX(voltage) FROM samples', [], function (result) {
            if (!requireOk(result, 'Statistics query')) return;
            const first = result.rows[0] || [0, null, null, null];
            const count = Number(first[0]);
            const fmt = v => (v === null ? '—' : Number(v).toFixed(5) + ' V');
            ui.set('statsTable', [
                ['Total persisted samples', String(count)],
                ['Minimum measured voltage', fmt(first[1])],
                ['Average measured voltage', fmt(first[2])],
                ['Maximum measured voltage', fmt(first[3])],
                ['Storage', databaseFile]
            ]);
            ui.set('totalRecords', count);
            log('Statistics refreshed; ' + count + ' measurements in persistent SQLite storage');
        });
}
function demonstrateSafety() {
    if (!db) {
        log('Open database first', 'WARN');
        return;
    }
    // A deliberately prohibited operation, to demonstrate the safety boundary.
    Database.exec(db, "ATTACH DATABASE 'other.db' AS other", [], function (result) {
        if (!result.ok) log('Expected restriction: ' + result.error, 'PASS');
        else log('Unexpected: ATTACH was accepted!', 'FAIL');
    });
}

// UI actions use Property Editor > Events (select a control, then Go to Handler).
// Asynchronous Database callbacks stay in JavaScript; these are not widget events.
function onInitialize() {
    showStatus('Database closed — press OPEN DATABASE to access shared PanelData');
    ui.set('sampleCount', 0);
    ui.set('sampleVoltage', 0);
    ui.set('totalRecords', 0);
    log('SQLite example ready. No database opened and no timer started automatically.');
}
function onShutdown() {
    // Pending DB callbacks are cancelled by the native Panel lifecycle.
    stopLogging();
    print('[SQLite] Panel stopped; database worker closes its connections');
}
