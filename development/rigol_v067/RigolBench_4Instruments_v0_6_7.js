/**
 * Rigol Four-Instrument Operator Bench v0.6.6
 *
 * Five Pages: MHO954, DG902 Pro, DP932A, DM858 and Native Automated Tests. The visual designer binds
 * buttons and selectors to the named handlers in this script. There are no
 * duplicate ui.on() registrations.
 *
 * RC1.3.1 native text and IPv4 fields replace all connection tables.
 * DP932A Numeric Steppers replace the shared setpoint table.
 *
 * SAFE STARTUP: import libraries, initialize plots and labels only. Connecting,
 * configuring and enabling any external output always requires a user action.
 * WFScript's SIMULATION banner does NOT prevent commands sent over Ethernet.
 *
 * Architecture: JavaScript orchestrates asynchronous SCPI commands; the
 * unchanged Rigol driver modules and Qt native Binary API handle transport,
 * framing, decoding and full-resolution statistics. Nothing uses VISA.
 */
var DEVICE = { scope: 'MHO954', gen: 'DG902', meter: 'DM858', psu: 'DP932A' };
var LIB = { scope: 'RigolMHO900.mjs', gen: 'RigolDG900Pro.mjs', meter: 'RigolDM858.mjs', psu: 'RigolDP900.mjs' };
var CLASS = { scope: 'MHO954', gen: 'DG902Pro', meter: 'DM858', psu: 'DP932A' };
var SCPI_CORE = null;
var MOD = {}, dev = { scope: null, gen: null, meter: null, psu: null };
var busy = { scope: false, gen: false, meter: false, psu: false };
var operations = { scope: null, gen: null, meter: null, psu: null }, runtimeStopping = false;
var loadingError = {};
var preparedGen = { 1: false, 2: false }, preparedPsu = { 1: false, 2: false, 3: false };
var genSettingsRevision = 0, psuSettingsRevision = 0;
var benchSeq = 0, lastFullRawRecord = null;
var scopeCaptureSerial = 0;
// Waveform download settings. 100k remains the conservative default, while
// larger blocks were physically tested on one MHO954; other firmware may differ.
var BINARY_BLOCK_CHOICES = {
    '100k  •  conservative': 100000,
    '250k  •  bench-tested': 250000,
    '500k  •  bench-tested': 500000,
    '1M  •  bench-tested': 1000000
};
function selectedBinaryBlock() {
    var choice = sel('scopeBinaryBlock');
    var count = BINARY_BLOCK_CHOICES[choice];
    if (!Number.isSafeInteger(count))
        throw Error('Invalid RAW block size: ' + choice);
    return count;
}
function requestedScopeTransport() {
    return sel('scopeBinaryMode') === 'Native Binary RC1.3' ? 'native' : 'legacy';
}
function activeScopeTransport() {
    return dev.scope && dev.scope.transport && dev.scope.transport.nativeTransport ? 'native' : 'legacy';
}
function updateBinaryState() {
    var connected = dev.scope && dev.scope.connected;
    var requested = requestedScopeTransport();
    var text = (connected ? activeScopeTransport().toUpperCase() + ' connected' : requested.toUpperCase() + ' selected');
    if (connected && activeScopeTransport() !== requested)
        text += ' · Reconnect to change transport';
    text += ' · ' + selectedBinaryBlock() + ' pts/block';
    put('scopeTransportStatus', text);
}
function onScopeBinaryModeChanged() { updateBinaryState(); }
function onScopeBinaryBlockChanged() { updateBinaryState(); }
/** The operator's FULL RECORD button. It downloads an existing STOP record;
 * it does not arm a new capture or alter acquisition settings.
 * Native/legacy transport is fixed when CONNECT is pressed.
 */
function onScopeBinaryFull() {
    try {
        if (!dev.scope || !dev.scope.connected)
            throw Error('Connect the MHO954 first.');
        if (requestedScopeTransport() !== activeScopeTransport())
            throw Error('Disconnect and reconnect to apply the selected TCP receive mode.');
        var points = selectedBinaryBlock();
        log('CAPTURE', 'Requested ' + points + ' samples/block using ' + activeScopeTransport() + ' transport');
        return onScopeFull1(points);
    }
    catch (error) {
        message('SCOPE', 'ERROR download: ' + textErr(error));
    }
}
function scopeFormat() { return sel('scopeTransferFormat') === '16-bit WORD' ? 'WORD' : 'BYTE'; }
function formatRate(v) { if (!Number.isFinite(v) || v <= 0)
    return '—'; return v >= 1e9 ? (v / 1e9).toPrecision(4) + ' GSa/s' : v >= 1e6 ? (v / 1e6).toPrecision(4) + ' MSa/s' : v >= 1e3 ? (v / 1e3).toPrecision(4) + ' kSa/s' : v.toPrecision(4) + ' Sa/s'; }
var meterMonitorTimer = null, meterMonitorReads = 0, meterSkipped = 0;
var traceEpoch = Date.now();
var COLORS = { 1: '#f5d23f', 2: '#40d6ef', 3: '#db6ad9', 4: '#70d68b', meter: '#ffffff' };
// Instrument state is not inferred from selector defaults. Update only from verified readbacks.
var scopeChannelCache = {}, scopeTriggerCache = null, scopeSyncingControls = false;
var histories = { psuV: { 1: [], 2: [], 3: [] }, psuI: { 1: [], 2: [], 3: [] }, meter: [] };
function log(kind, msg) { print('[RIGOL BENCH][' + kind + '] ' + String(msg)); }
function textErr(e) { return String(e && (e.message || e.stack) || e); }
function put(id, value) { ui.set(id, String(value)); }
/** Route status messages to the page that owns the physical instrument. */
function message(kind, msg) {
    log(kind, msg);
    var targets = { GEN: 'genStatus', SCOPE: 'scopeStatus', PSU: 'psuStatus', METER: 'meterStatus' };
    if (targets[kind])
        put(targets[kind], msg);
    if (kind === 'SCOPE' && String(msg).indexOf('ERROR ') === 0)
        put('scopeReadback', 'MHO954 · ERROR — ' + String(msg).slice(0, 190));
}
function failure(kind, op, error) { message(kind, 'ERROR ' + op + ': ' + textErr(error)); }
function decodeModule(kind) {
    try {
        var m = modules.load(LIB[kind]);
        if (!m || typeof m[CLASS[kind]] !== 'function') {
            throw new Error('module ' + LIB[kind] + ' lacks constructor ' + CLASS[kind] + '; ' + String(m && (m.stack || m.message) || m));
        }
        MOD[kind] = m;
        log('INIT', kind + ' ' + LIB[kind] + ': export ' + CLASS[kind] + '=' + typeof m[CLASS[kind]] + '; VERSION=' + String(m.VERSION || '?'));
        if (typeof m.diagnoseStartup === 'function') {
            var diag = m.diagnoseStartup();
            log('INIT', kind + ' constructor self-test: ' + JSON.stringify(diag));
            if (diag && !diag.ok)
                throw new Error('self-test failed: ' + JSON.stringify(diag));
        }
    }
    catch (e) {
        loadingError[kind] = textErr(e);
        log('INIT', 'ERROR ' + kind + ' ' + loadingError[kind]);
    }
}
/** Per-instrument network controls are native RC1.3.1 widgets, not tables.
 * IP Input validates complete IPv4 addresses before publishing their values;
 * the TCP port is free text and is independently validated here. We never
 * silently fall back to a hard-coded endpoint after invalid operator input.
 */
function endpoint(which) {
    var prefix = { scope: 'scope', gen: 'gen', psu: 'psu', meter: 'meter' }[which];
    if (!prefix) throw Error('Unknown instrument: ' + which);
    var host = String(ui.get(prefix + 'Ip') || '').trim();
    var portText = String(ui.get(prefix + 'Port') || '').trim();
    var port = Number(portText);
    // Enforce dotted-quad here as defense in depth, including the unusual case
    // that a project is modified outside WFScript and bypasses GUI validation.
    var parts = host.split('.');
    if (parts.length !== 4 || !parts.every(function (part) {
        return /^(0|[1-9]\d{0,2})$/.test(part) && Number(part) <= 255;
    })) throw Error('Enter a valid IPv4 address for ' + DEVICE[which] + ': ' + host);
    if (!/^\d{1,5}$/.test(portText) || !Number.isInteger(port) || port < 1 || port > 65535)
        throw Error('TCP port for ' + DEVICE[which] + ' must be 1–65535: ' + portText);
    return { host: host, port: port };
}
function deviceReady(which) {
    if (loadingError[which]) {
        message(which.toUpperCase(), 'Library unavailable: ' + loadingError[which]);
        return null;
    }
    if (!dev[which] || !dev[which].connected) {
        message(which.toUpperCase(), 'Not connected; use CONNECT first.');
        return null;
    }
    return dev[which];
}
/** Normal operator sessions keep diagnostic traffic logging disabled.
 * The untouched v0.5.1 companion Panel retains full SCPI trace controls.
 */
function traceOptions(which, port) {
    return { port: port, connectTimeoutMs: 8000, requestTimeoutMs: 15000, maxQueue: 256 };
}
function connect(which) {
    if (rigolTestRunning) return message(which.toUpperCase(), 'Test Executive running — connections locked');
    var tag = which.toUpperCase();
    if (runtimeStopping)
        return message(tag, 'Panel stopping; connection denied');
    if (loadingError[which])
        return message(tag, 'Cannot connect: ' + loadingError[which]);
    if (busy[which])
        return message(tag, 'Already processing a command.');
    if (dev[which] && dev[which].connected)
        return message(tag, 'Already connected; disconnect before changing the address.');
    var ep, obj;
    try {
        ep = endpoint(which);
        var opts = traceOptions(which, ep.port);
        if (which === 'scope') {
            opts.nativeTransport = requestedScopeTransport() === 'native';
        }
        if (which === 'gen' && typeof MOD.gen.createDG902Pro === 'function')
            obj = MOD.gen.createDG902Pro(ep.host, opts);
        else if (which === 'scope' && typeof MOD.scope.createMHO954 === 'function')
            obj = MOD.scope.createMHO954(ep.host, opts);
        else
            obj = new MOD[which][CLASS[which]](ep.host, opts);
        dev[which] = obj;
        if (which === 'gen') {
            preparedGen[1] = false;
            preparedGen[2] = false;
        }
        if (which === 'psu') {
            preparedPsu[1] = false;
            preparedPsu[2] = false;
            preparedPsu[3] = false;
        }
    }
    catch (e) {
        return failure(tag, 'endpoint/constructor', e);
    }
    log('CONNECT', tag + ' ' + ep.host + ':' + ep.port + ' -> opening TCP and querying *IDN?; no output commands');
    runP(which, 'CONNECT ' + ep.host + ':' + ep.port, function (d, op) { return callP(d, 'connect'); }, { allowUnconnected: true, timeoutMs: 20000, quiet: true })
        .then(function (id) {
        if (dev[which] !== obj || runtimeStopping)
            return;
        log('CONNECT', tag + ' OK model=' + String(id.model) + ' SN=' + String(id.serial) + ' FW=' + String(id.firmware));
        message(tag, 'Connected ' + String(id.model) + ' at ' + ep.host + ':' + ep.port + '. Outputs left unchanged.');
        if (which === 'gen')
            readGenerator();
        if (which === 'scope') {
            updateBinaryState();
        }
        if (which === 'scope')
            onScopeSync(); // Populate all controls from physical readbacks; NEVER configure on connect.
        if (which === 'psu')
            readSupplyAll();
        if (which === 'meter')
            put('meterValue', 'CONNECTED · press READ METER');
    }, function (e) { if (dev[which] === obj) {
        try {
            obj.close();
        }
        catch (_) { }
        dev[which] = null;
        failure(tag, 'CONNECT ' + ep.host + ':' + ep.port, e);
    }
    else
        log('CONNECT', tag + ' previous connection canceled/failed: ' + textErr(e)); });
}
function stopMeterMonitoring() {
    if (meterMonitorTimer !== null) {
        clearInterval(meterMonitorTimer);
        meterMonitorTimer = null;
        log('MONITOR', 'DM858 monitoring stopped');
    }
}
function disconnect(which) {
    if (rigolTestRunning) return message(which.toUpperCase(), 'Test Executive running — press STOP and verify safe OFF before disconnect');
    cancelOperation(which, 'Operator disconnected ' + DEVICE[which], false);
    if (which === 'scope') {
        scopeCaptureSerial++;
        put('scopeReadback', 'MHO954 · DISCONNECTED · last plot retained (not live)');
        scopeChannelCache = {};
        scopeTriggerCache = null;
        syncScopeChannelSwitches(); // Disconnected means UNKNOWN, not a verified physical OFF.
        showScopeChannelCards();
    }
    if (which === 'meter')
        stopMeterMonitoring();
    var d = dev[which];
    dev[which] = null;
    busy[which] = false;
    if (which === 'scope')
        updateBinaryState();
    if (d) {
        try {
            d.close();
        }
        catch (e) {
            failure(which.toUpperCase(), 'CLOSE', e);
        }
    }
    if (which === 'gen') {
        preparedGen[1] = false;
        preparedGen[2] = false;
    }
    if (which === 'psu') {
        preparedPsu[1] = false;
        preparedPsu[2] = false;
        preparedPsu[3] = false;
    }
    message(which.toUpperCase(), 'Disconnected; pending operations canceled. External outputs NOT changed.');
}
/* Single operation lifecycle for callback and Promise applications. All devices get
 * the same ownership token, watchdog, cancellation, and transport quarantine policy.
 * Quarantine on timeout: never let a late SCPI reply be read as a new query.
 * Normal disconnect/shutdown close the socket and invalidate all pending continuations.
 * This is Panel-only; no modifications to C++ communication bridges.
 */
function callP(target, method, args) {
    if (!target || typeof target[method] !== 'function')
        return Promise.reject(new Error('Missing driver method ' + method));
    if (SCPI_CORE && typeof SCPI_CORE.invoke === 'function')
        return SCPI_CORE.invoke(target, method, args);
    return new Promise(function (resolve, reject) {
        var a = Array.isArray(args) ? args.slice() : [];
        a.push(function (e, v) { if (e)
            reject(e);
        else
            resolve(v); });
        try {
            target[method].apply(target, a);
        }
        catch (e) {
            reject(e);
        }
    });
}
function sequenceP(op, items, fn) {
    return items.reduce(function (p, item, index) {
        return p.then(function () { op.ensure(); return fn(item, index); });
    }, Promise.resolve());
}
function cancelOperation(which, why, quarantine) {
    var op = operations[which];
    if (op && op.active)
        op.cancel(why, !!quarantine);
}
function runP(which, label, task, options) {
    options = options || {};
    if (rigolTestRunning && !options.testOwned)
        return Promise.reject(new Error('Test Executive owns instrument commands during this run'));
    var tag = which.toUpperCase(), d = dev[which];
    if (runtimeStopping)
        return Promise.reject(new Error('Panel is shutting down'));
    if (!d || (!options.allowUnconnected && !d.connected)) {
        var notReady = new Error(DEVICE[which] + ' not connected; use CONNECT first');
        if (!options.quiet)
            message(tag, notReady.message);
        return Promise.reject(notReady);
    }
    if (operations[which] && operations[which].active) {
        var occupied = new Error(DEVICE[which] + ' BUSY (' + operations[which].label + '); cannot start ' + label);
        if (!options.quiet)
            message(tag, occupied.message);
        return Promise.reject(occupied);
    }
    var timeoutMs = options.timeoutMs || 60000;
    if (!Number.isSafeInteger(timeoutMs) || timeoutMs < 1000 || timeoutMs > 300000)
        return Promise.reject(new Error('Invalid operation timeout ' + timeoutMs));
    var id = ++benchSeq, started = Date.now(), ok, fail;
    var result = new Promise(function (resolve, reject) { ok = resolve; fail = reject; });
    var op = { id: id, label: label, which: which, device: d, active: true, timer: null, delayWaits: [],
        isActive: function () { return op.active && operations[which] === op && dev[which] === d && !runtimeStopping; },
        ensure: function () { if (!op.isActive())
            throw new Error(DEVICE[which] + ' operation #' + id + ' was canceled'); },
        delay: function (ms) {
            return new Promise(function (resolve, reject) {
                if (!op.isActive())
                    return reject(new Error('Operation canceled before timer'));
                var item = { timer: null, reject: reject };
                item.timer = setTimeout(function () {
                    var pos = op.delayWaits.indexOf(item);
                    if (pos >= 0)
                        op.delayWaits.splice(pos, 1);
                    if (op.isActive())
                        resolve(true);
                    else
                        reject(new Error('Operation canceled during timer'));
                }, ms);
                op.delayWaits.push(item);
            });
        }
    };
    function finish(err, value) {
        if (!op.active)
            return;
        op.active = false;
        if (op.timer !== null) {
            clearTimeout(op.timer);
            op.timer = null;
        }
        op.delayWaits.forEach(function (item) { clearTimeout(item.timer); item.reject(new Error('Operation ended during timer')); });
        op.delayWaits = [];
        if (operations[which] === op) {
            operations[which] = null;
            busy[which] = false;
        }
        if (err) {
            var issue = err instanceof Error ? err : new Error(String(err && err.message || err));
            if (runtimeStopping)
                log('OP', '#' + id + ' ' + tag + ' ' + label + ' canceled during shutdown: ' + textErr(issue));
            else if (!options.quiet)
                failure(tag, '#' + id + ' ' + label, issue);
            else
                log('OP', '#' + id + ' ' + tag + ' ' + label + ' ERROR ' + textErr(issue));
            fail(issue);
        }
        else {
            if (!options.quiet) {
                log('DONE', '#' + id + ' ' + tag + ' ' + label + ' in ' + (Date.now() - started) + ' ms');
                if (value !== undefined && value !== null)
                    message(tag, String(value));
            }
            ok(value);
        }
    }
    op.cancel = function (reason, quarantine) {
        if (!op.active)
            return;
        var e = new Error('CANCELED #' + id + ' ' + tag + ' ' + label + ': ' + reason);
        finish(e);
        if (quarantine && dev[which] === d) {
            // In-flight TCP data belongs to the failed transaction. No reuse of that socket.
            dev[which] = null;
            try {
                d.close();
            }
            catch (closeError) {
                log('OP', 'Close after cancellation: ' + textErr(closeError));
            }
            if (which === 'scope') {
                scopeCaptureSerial++;
            }
            if (which === 'meter')
                stopMeterMonitoring();
            if (which === 'gen') {
                preparedGen[1] = false;
                preparedGen[2] = false;
            }
            if (which === 'psu') {
                preparedPsu[1] = false;
                preparedPsu[2] = false;
                preparedPsu[3] = false;
            }
            message(tag, 'Transport quarantined; reconnect required. External output states unchanged/unknown.');
        }
    };
    operations[which] = op;
    busy[which] = true;
    if (!options.quiet)
        log('START', '#' + id + ' ' + tag + ' ' + label);
    op.timer = setTimeout(function () { op.cancel('operation watchdog ' + timeoutMs + ' ms', true); }, timeoutMs);
    var startedWork;
    try {
        startedWork = task(d, op);
    }
    catch (e) {
        finish(e);
        return result;
    }
    Promise.resolve(startedWork).then(function (value) { if (op.isActive())
        finish(null, value); }, function (e) { if (op.isActive())
        finish(e); });
    return result;
}
function launch(which, label, task, options) {
    // Visual event handlers intentionally consume rejections: runP has already logged them.
    var p = runP(which, label, task, options);
    p.then(function () { }, function () { });
    return p;
}
function run(which, label, invoke, options) {
    // Compatibility adapter for short callback-only routines; shares the same lifecycle.
    return launch(which, label, function (d, op) {
        return new Promise(function (resolve, reject) {
            var settled = false;
            function done(e, r) {
                if (settled)
                    return;
                settled = true;
                if (!op.isActive())
                    return;
                if (e)
                    reject(e);
                else
                    resolve(r);
            }
            try {
                invoke(d, done, op.id);
            }
            catch (e) {
                done(e);
            }
        });
    }, options);
}
function limitNumber(id, min, max) {
    var x = Number(ui.get(id));
    if (!Number.isFinite(x) || x < min || x > max)
        throw new Error(id + ' must be between ' + min + ' and ' + max);
    return x;
}
function sel(id) { return String(ui.get(id)).trim(); }
function genChannel(d) { return sel('genCh') === 'CH2' ? d.ch2 : d.ch1; }
function onGenEdited() { genSettingsRevision++; preparedGen[1] = false; preparedGen[2] = false; put('genReadback', 'Settings changed: APPLY while output OFF before enabling.'); }
function onConnectGen() { connect('gen'); }
function onConnectScope() { connect('scope'); }
function onConnectMeter() { connect('meter'); }
function onConnectPsu() { connect('psu'); }
function onDisconnectGen() { disconnect('gen'); }
function onDisconnectScope() { disconnect('scope'); }
function onDisconnectMeter() { disconnect('meter'); }
function onDisconnectPsu() { disconnect('psu'); }
// =============================================================================
// DG902 PRO — Waveform setup and explicitly verified output control
// =============================================================================
function onGenRead() { readGenerator(); }
function readGenerator() {
    return launch('gen', 'read both channels', function (d, op) {
        var lines = [];
        return sequenceP(op, [1, 2], function (n) {
            var c = d.channel(n);
            return callP(c, 'getOutput').then(function (on) {
                op.ensure();
                return callP(c, 'getWaveform').then(function (w) {
                    op.ensure();
                    var line = 'CH' + n + ': ' + (on ? 'ON' : 'OFF') + ' ' + w.shape + ' ' + w.frequency + ' Hz / ' + w.amplitude + ' Vpp / ' + w.offset + ' V';
                    lines.push(line);
                    log('GEN', line);
                });
            });
        }).then(function () { op.ensure(); put('genReadback', lines.join('  |  ')); return 'CH1/CH2 readback complete'; });
    });
}
function onGenApply() {
    return launch('gen', 'apply output-OFF waveform configuration', function (d, op) {
        var c = genChannel(d), n = c.number, editRevision = genSettingsRevision;
        preparedGen[n] = false;
        var shapeMap = { Sine: 'sine', Square: 'square', Pulse: 'pulse', Ramp: 'ramp', DC: 'dc' };
        var shape = shapeMap[sel('genShape')];
        if (!shape)
            throw new Error('Unsupported waveform');
        var f = limitNumber('genHz', 1, 1000000), amp = limitNumber('genVpp', 0.01, 10), offset = limitNumber('genOffset', -3, 3), duty = limitNumber('genDuty', 1, 99);
        var load = sel('genLoad') === '50 Ohm' ? 50 : 'HIGHZ';
        var o = { shape: shape, frequency: f, amplitude: amp, offset: offset, phase: 0, load: load };
        if (shape === 'square')
            o.duty = duty;
        log('GEN', 'CH' + n + ' APPLY ' + JSON.stringify(o) + '; requires output OFF');
        return callP(c, 'configure', [o]).then(function (actual) {
            op.ensure();
            if (editRevision !== genSettingsRevision)
                throw new Error('Generator controls changed during APPLY; apply again before enabling');
            preparedGen[n] = true;
            var line = 'CH' + n + ' ' + shape.toUpperCase() + ' ' + (shape === 'dc' ? 'DC' : f + ' Hz / ' + amp + ' Vpp') +
                ' / ' + offset + ' V offset / load ' + load + '; OUTPUT OFF';
            if (shape === 'square')
                line += ' / duty ' + String(actual && actual.duty) + '% (after APPLY ' + String(actual && actual.dutyAfterApply) + '%)';
            put('genReadback', line);
            log('GEN', 'CONFIGURED ' + line);
            return 'CH' + n + ' configured and still OFF. ENABLE separately.';
        });
    });
}
function onGenOn() {
    return launch('gen', 'explicit OUTPUT ON', function (d, op) {
        var c = genChannel(d), n = c.number;
        if (!preparedGen[n])
            throw new Error('CH' + n + ' requires APPLY configuration while output OFF before ENABLE');
        log('SAFETY', 'Operator requested DG902 CH' + n + ' ON');
        return callP(c, 'enable').then(function () { op.ensure(); return callP(c, 'getOutput'); })
            .then(function (on) {
            op.ensure();
            if (!on)
                throw new Error('CH' + n + ' did not read back ON');
            put('genReadback', 'CH' + n + ' OUTPUT ON (verified)');
            return 'CH' + n + ' ON verified';
        });
    });
}
function onGenOff() {
    return launch('gen', 'explicit selected OUTPUT OFF', function (d, op) {
        var c = genChannel(d), n = c.number;
        return callP(c, 'disable').then(function () { op.ensure(); return callP(c, 'getOutput'); })
            .then(function (on) {
            op.ensure();
            if (on)
                throw new Error('CH' + n + ' still ON after OFF');
            put('genReadback', 'CH' + n + ' OUTPUT OFF (verified)');
            return 'CH' + n + ' OFF verified';
        });
    });
}
function onGenAllOff() {
    return launch('gen', 'ALL OUTPUTS OFF', function (d, op) {
        log('SAFETY', 'Operator requested DG902 CH1/CH2 all OFF');
        return callP(d.output, 'allOff').then(function () {
            op.ensure();
            preparedGen[1] = false;
            preparedGen[2] = false;
            put('genReadback', 'CH1 OFF, CH2 OFF (verified)');
            return 'Both generator outputs reported OFF';
        });
    });
}
/** Display a channel's actual state; never derive channel status from the plot. */
function formatScopeVolts(v) {
    var n = Number(v);
    if (!Number.isFinite(n)) return '?';
    return n < 1 ? (n * 1000).toPrecision(3) + ' mV' : n.toPrecision(3) + ' V';
}
function setScopeSelection(id, text) {
    // RC1.3.1 OptionSelectorWidget::setSelection blocks QComboBox signals.
    // Also guard events in JavaScript for host variants / future changes.
    var wasSyncing = scopeSyncingControls;
    scopeSyncingControls = true;
    try { ui.set(id, text); }
    finally { scopeSyncingControls = wasSyncing; }
}
/** A latching switch reflects the physical :CHANn:DISP? state, not a local
 * display filter. Programmatic readbacks must never issue SCPI writes.
 */
function setScopeChannelSwitch(n, enabled) {
    var previous = scopeSyncingControls;
    scopeSyncingControls = true;
    try { ui.set('scopeEnable' + n, Boolean(enabled)); }
    finally { scopeSyncingControls = previous; }
}
function syncScopeChannelSwitches() {
    for (var n = 1; n <= 4; n++) {
        var data = scopeChannelCache[n];
        setScopeChannelSwitch(n, Boolean(data && data.enabled));
    }
}
function matchScopeScale(value, choices) {
    var actual = Number(value);
    if (!(actual > 0) || !Number.isFinite(actual)) return null;
    var best = null, difference = Infinity;
    Object.keys(choices).forEach(function (label) {
        var relative = Math.abs(actual - choices[label]) / actual;
        if (relative < difference) { best = label; difference = relative; }
    });
    // Don't lie about an unusual manual fine-scale setting.
    return difference <= .035 ? best : null;
}
function showScopeSelectedChannel() {
    var n = Number(String(sel('scopePick')).replace('CH', ''));
    var data = scopeChannelCache[n];
    if (!data) return;
    var scaleOption = matchScopeScale(data.scale, VDIV_CHOICE);
    if (scaleOption) setScopeSelection('scopeVScale', scaleOption);
    // The four channel cards show scale/coupling/probe; the selector follows readback.
    if (!scaleOption) log('SCOPE', 'CH' + n + ' vertical scale ' + data.scale +
        ' V/div is not represented by the selector; physical setting unchanged');
}
function showScopeChannelCards() {
    for (var n = 1; n <= 4; n++) {
        var c = scopeChannelCache[n];
        var trig = scopeTriggerCache && scopeTriggerCache.mode === 'EDGE' && scopeTriggerCache.source === 'CH' + n;
        put('scopeCh' + n + 'Status', !c ?
            '<b style="color:' + COLORS[n] + '">CH' + n + '</b> · NO READBACK' :
            '<b style="color:' + COLORS[n] + '">CH' + n + '</b>  ' +
            (c.enabled ? 'ON' : 'OFF') + (trig ? '  · <b>TRIG</b>' : '') + '<br/>' +
            formatScopeVolts(c.scale) + '/div · ' + String(c.coupling).toUpperCase().replace(/[&<>"]/g, '') +
            ' · ×' + Number(c.probe));
    }
}
/** Query all four physical channels; share cache with channel controls and cards. */
function scopeQueryChannels(d, op) {
    var result = {};
    return sequenceP(op, [1, 2, 3, 4], function (n) {
        return callP(d.channel(n), 'getSettings').then(function (settings) {
            op.ensure();
            if (!Number.isFinite(Number(settings.scale)) || Number(settings.scale) <= 0)
                throw Error('CH' + n + ' reported an invalid vertical scale');
            result[n] = settings;
            log('SCOPE', 'READ CH' + n + ' ' + (settings.enabled ? 'ON' : 'OFF') +
                ', ' + settings.scale + ' V/div, ' + settings.coupling + ', probe ' + settings.probe);
        });
    }).then(function () {
        op.ensure();
        scopeChannelCache = result; // atomic publication; partial query cannot overwrite previous state
        syncScopeChannelSwitches();
        showScopeChannelCards();
        showScopeSelectedChannel();
        return result;
    });
}
function readScopeChannels() {
    return launch('scope', 'read CH1–CH4 states', function (d, op) {
        return scopeQueryChannels(d, op).then(function () { return 'All four channel readbacks updated'; });
    });
}
function normalizeTriggerSource(source) {
    var value = String(source).trim().toUpperCase();
    var match = /^(?:CHAN(?:NEL)?|CH)([1-4])$/.exec(value);
    return match ? 'CH' + match[1] : value;
}
function normalizeTriggerSlope(slope) {
    var v = String(slope).trim().toUpperCase();
    if (v === 'POS' || v === 'POSITIVE') return 'Rising';
    if (v === 'NEG' || v === 'NEGATIVE') return 'Falling';
    if (v === 'RFAL' || v === 'RFALL') return 'Either';
    return null;
}
/** Query the actual trigger type first, because level/source settings are
 * meaningful only when the instrument is currently in EDGE trigger mode.
 * No write occurs during a readback, including when controls are populated.
 */
function scopeQueryTrigger(d, op) {
    var mode, source = '', slope = '', level = NaN, sweep = '';
    return callP(d.scpi, 'query', [':TRIG:MODE?']).then(function (m) {
        op.ensure();
        mode = String(m).trim().toUpperCase();
        return callP(d.scpi, 'query', [':TRIG:SWE?']);
    }).then(function (v) {
        op.ensure(); sweep = String(v).trim().toUpperCase();
        if (mode !== 'EDGE') return null;
        return callP(d.scpi, 'query', [':TRIG:EDGE:SOUR?'])
            .then(function (v) { op.ensure(); source = normalizeTriggerSource(v); return callP(d.scpi, 'query', [':TRIG:EDGE:SLOP?']); })
            .then(function (v) { op.ensure(); slope = normalizeTriggerSlope(v); return callP(d.scpi, 'query', [':TRIG:EDGE:LEV?']); })
            .then(function (v) { op.ensure(); level = Number(v); });
    }).then(function () {
        op.ensure();
        if (mode === 'EDGE' && (!slope || !Number.isFinite(level)))
            throw Error('Incomplete EDGE trigger readback: source=' + source + ' slope=' + slope + ' level=' + level);
        var info = { mode: mode, source: source, slope: slope, level: level, sweep: sweep };
        scopeTriggerCache = info;
        if (mode === 'EDGE') {
            if (/^CH[1-4]$/.test(source)) setScopeSelection('scopeTrigSource', source);
            if (slope) setScopeSelection('scopeTrigSlope', slope);
            ui.set('scopeTrigLevel', level);
        }
        var label = mode === 'EDGE' ?
            'TRIGGER  EDGE  ·  ' + source + '  ·  ' + slope + '  ·  ' + level.toPrecision(3) + ' V  ·  ' + sweep :
            'TRIGGER  ' + mode + '  ·  ' + sweep + '  (read-only: Edge controls do not apply)';
        put('scopeTriggerInfo', label);
        showScopeChannelCards();
        log('SCOPE', 'Trigger readback ' + JSON.stringify(info));
        return info;
    });
}
function onScopeTrigger() {
    return launch('scope', 'READ TRIGGER settings', function (d, op) {
        return scopeQueryTrigger(d, op).then(function (v) {
            return 'Physical trigger ' + v.mode + ' settings read; instrument unchanged';
        });
    });
}
function summarize(values) {
    if (!values || !values.length)
        throw new Error('No waveform samples');
    var low = Infinity, high = -Infinity, total = 0;
    for (var i = 0; i < values.length; i++) {
        var x = Number(values[i]);
        if (!Number.isFinite(x))
            throw new Error('Nonfinite waveform at index ' + i);
        if (x < low)
            low = x;
        if (x > high)
            high = x;
        total += x * x;
    }
    return { min: low, max: high, vpp: high - low, rms: Math.sqrt(total / values.length) };
}
/** Configure charts once. Updates are explicit, not polled from startup. */
function setupPlots() {
    plots.configure('scopeOverlay', { chartType: 'XY', title: 'MHO954 · waveform',
        xLabel: 'Time (ms)', yLabel: 'Voltage (V)', showGrid: true, showLegend: true, xAuto: true, yAuto: true });
    plots.configure('powerVoltage', { chartType: 'XY', title: 'DP932A · voltage history',
        xLabel: 'Elapsed time (s)', yLabel: 'Voltage (V)', showGrid: true, showLegend: true, xAuto: true, yAuto: true });
    plots.configure('powerCurrent', { chartType: 'XY', title: 'DP932A · current history',
        xLabel: 'Elapsed time (s)', yLabel: 'Current (A)', showGrid: true, showLegend: true, xAuto: true, yAuto: true });
    plots.configure('meterTrend', { chartType: 'XY', title: 'DM858 · voltage history',
        xLabel: 'Elapsed time (s)', yLabel: 'Voltage (V)', showGrid: true, showLegend: true, xAuto: true, yAuto: true });
}
/** Peak-preserving reduction; chart points are never substituted for original RAW data. */
function chartWave(n, time, values) {
    if (time.length !== values.length || time.length < 2)
        throw new Error('CH' + n + ' invalid waveform lengths');
    var stride = Math.max(1, Math.ceil(values.length / 900)), x = [], y = [];
    for (var i = 0; i < values.length; i += stride) {
        var until = Math.min(i + stride, values.length), lo = i, hi = i;
        for (var j = i + 1; j < until; j++) {
            if (values[j] < values[lo])
                lo = j;
            if (values[j] > values[hi])
                hi = j;
        }
        var a = Math.min(lo, hi), b = Math.max(lo, hi);
        x.push(time[a] * 1000);
        y.push(values[a]);
        if (a !== b) {
            x.push(time[b] * 1000);
            y.push(values[b]);
        }
    }
    plots.setSeries('scopeOverlay', 'CH' + n, x, y, { color: COLORS[n], width: 2 });
    return x.length;
}
/** Scope view modes:
 *  SCREEN reads all physically enabled channels (~1000 points/channel)
 *  across the physical displayed timebase. No 10k-point center-slice artifact.
 *  FULL reads ALL actual RAW memory samples (up to the explicit 1M/ch bound),
 *  with verified, independently bounded SCPI blocks. It preserves every raw
 *  BYTE/WORD sample code in a chunked in-memory record for lossless binary export.
 *  A normal SCREEN/SINGLE capture may arm the scope; FULL RECORD only reads an existing stopped record.
 */
var FULL_RAW_MAX_PER_CH = 1000000, FULL_RAW_CHUNK = 100000;
function makeFullRecord(channels, depth, rate, serial, firstIndex) {
    firstIndex = firstIndex || 1;
    var bins = Math.min(900, Math.ceil(depth / 2));
    var result = { channels: channels.slice(), pointsPerChannel: depth, sampleRate: rate, serial: serial,
        mode: 'RAW', format: scopeFormat(), firstIndex: firstIndex, timestamp: Date.now(), channelsData: {}, dt: null,
        nativeProcessing: false };
    for (var j = 0; j < channels.length; j++) {
        var n = channels[j];
        result.channelsData[n] = { channel: n, next: firstIndex, count: 0, chunks: [], bins: new Array(bins),
            min: Infinity, max: -Infinity, squares: 0, preamble: null, nativePoints: [] };
    }
    result.add = function (n, at, frame) {
        var r = this.channelsData[n], c = frame && frame['ch' + n];
        if (!c || !c.samples || !c.voltage || !c.preamble || r.next !== at)
            throw Error('RAW CH' + n + ' incomplete or out-of-order block at ' + at);
        var len = c.samples.length;
        if (len < 1 || len !== c.voltage.length || len !== frame.points || at + len - 1 > firstIndex + depth - 1)
            throw Error('RAW CH' + n + ' invalid block size ' + len + ' at ' + at);
        if (!r.preamble) {
            r.preamble = c.preamble;
            if (this.dt === null)
                this.dt = c.preamble.xIncrement;
        }
        else {
            var p = r.preamble, q = c.preamble;
            if (Math.abs(p.xIncrement - q.xIncrement) > Math.max(1e-15, p.xIncrement * 1e-6) ||
                Math.abs(p.xOrigin - q.xOrigin) > Math.max(1e-12, p.xIncrement * .01) ||
                p.yIncrement !== q.yIncrement || p.yReference !== q.yReference || p.yOrigin !== q.yOrigin)
                throw Error('RAW CH' + n + ' acquisition calibration changed during transfer');
        }
        if (Math.abs(this.dt - c.preamble.xIncrement) > Math.max(1e-15, Math.abs(this.dt) * 1e-6))
            throw Error('RAW channel sample intervals differ');
        // Retain raw integer sample codes in nonduplicated chunks, not a million JavaScript time floats.
        r.chunks.push(c.samples);
        for (var i = 0; i < len; i++) {
            var raw = c.samples[i], v = c.voltage[i], index = at - firstIndex + i;
            if (!Number.isInteger(raw) || raw < 0 || raw > (this.format === 'WORD' ? 65535 : 255) || !Number.isFinite(v))
                throw Error('Invalid RAW sample/voltage CH' + n + ' index ' + index);
            r.min = Math.min(r.min, v);
            r.max = Math.max(r.max, v);
            r.squares += v * v;
            var binIndex = Math.min(bins - 1, Math.floor(index * bins / depth));
            var b = r.bins[binIndex];
            if (!b)
                r.bins[binIndex] = { low: v, high: v, lowIndex: index, highIndex: index };
            else {
                if (v < b.low) {
                    b.low = v;
                    b.lowIndex = index;
                }
                if (v > b.high) {
                    b.high = v;
                    b.highIndex = index;
                }
            }
        }
        r.next += len;
        r.count += len;
    };
    // Native payloads remain BinaryBuffer objects. Decode, stats, and min/max reduction
    // happen in C++; only <=~900 display bins per channel enter JavaScript.
    result.addNative = function (n, at, frame) {
        var r = this.channelsData[n], c = frame && frame['ch' + n];
        if (!c || !c.native || !c.rawBuffer || !c.preamble || r.next !== at)
            throw Error('Native RAW CH' + n + ' missing/out-of-order block at ' + at);
        var width = this.format === 'WORD' ? 2 : 1, len = c.points, raw = c.rawBuffer;
        if (len < 1 || len !== frame.points || raw.length !== len * width || at + len - 1 > firstIndex + depth - 1)
            throw Error('Native RAW CH' + n + ' incorrect payload ' + raw.length + ' at ' + at);
        if (!r.preamble) {
            r.preamble = c.preamble;
            if (this.dt === null)
                this.dt = c.preamble.xIncrement;
        }
        else {
            var p = r.preamble, q = c.preamble;
            if (Math.abs(p.xIncrement - q.xIncrement) > Math.max(1e-15, p.xIncrement * 1e-6) ||
                Math.abs(p.xOrigin - q.xOrigin) > Math.max(1e-12, p.xIncrement * .01) ||
                p.yIncrement !== q.yIncrement || p.yReference !== q.yReference || p.yOrigin !== q.yOrigin)
                throw Error('Native RAW CH' + n + ' calibration changed during transfer');
        }
        if (Math.abs(this.dt - c.preamble.xIncrement) > Math.max(1e-15, Math.abs(this.dt) * 1e-6))
            throw Error('Native RAW sample intervals differ');
        var p = c.preamble, scale = p.yIncrement, bias = -(p.yOrigin + p.yReference) * scale;
        var samples = raw.decodeSamples(width === 2 ? 'u16le' : 'u8', 0, len, scale, bias);
        if (!samples)
            throw Error('Native samples decode failed: ' + raw.lastError);
        try {
            var stats = samples.stats();
            if (!stats || stats.count !== len || !Number.isFinite(stats.min) || !Number.isFinite(stats.max) || !Number.isFinite(stats.rms))
                throw Error('Native sample statistics invalid: ' + samples.lastError);
            // Plot FIX v0.5.0.1: RC1.3 minMaxBins returns min/max VALUES and
            // [start,end) bounds, but not the extrema's actual sample indices.
            // v0.5.0 incorrectly plotted min at start and max at end, reversing the
            // falling half of a sine wave inside every display bin.
            // Draw actual samples at chronological bin midpoints instead. This is a
            // preview decimation, NOT an extrema-preserving envelope; retain native
            // full-resolution stats and untouched RAW chunks for faithful exports.
            // Only <=1700 scalar native reads per full record; never loop through all
            // 1M samples via the QJSEngine->QObject method boundary.
            // For Native Chart API builds, the full RAW dataset will be given
            // to the C++ chart after the record is complete. Avoid generating
            // a second, low-resolution JavaScript display copy here.
            if (typeof plots.setNativeSamples !== 'function') {
            var nBins = Math.min(2048, len, Math.max(1, Math.ceil(len * 1700 / depth)));
            var reduced = samples.minMaxBins(nBins);
            if (!reduced || !reduced.length)
                throw Error('Native plot-bin reduction failed: ' + samples.lastError);
            for (var bi = 0; bi < reduced.length; bi++) {
                var b = reduced[bi];
                if (!Number.isInteger(b.start) || !Number.isInteger(b.end) ||
                    b.start < 0 || b.end > len || b.end <= b.start)
                    throw Error('Native plot-bin boundaries invalid at ' + bi);
                var middle = Math.floor((b.start + b.end - 1) / 2);
                var code = width === 2 ? raw.readU16LE(middle * 2) : raw.readU8(middle);
                if (!Number.isFinite(code))
                    throw Error('Native plot sample unreadable at ' + middle);
                r.nativePoints.push({ index: at - firstIndex + middle, value: code * scale + bias });
            }
            }
            r.min = Math.min(r.min, stats.min);
            r.max = Math.max(r.max, stats.max);
            r.squares += stats.rms * stats.rms * len;
        }
        finally {
            samples.release();
        }
        r.chunks.push(raw); // Exact original byte payload for later lossless export.
        r.next += len;
        r.count += len;
        this.nativeProcessing = true;
    };
    result.complete = function () {
        for (var j = 0; j < this.channels.length; j++) {
            var n = this.channels[j], r = this.channelsData[n];
            if (r.count !== depth)
                throw Error('Full RAW CH' + n + ' incomplete: ' + r.count + '/' + depth);
        }
    };
    result.plot = function (target, prefix, preserve) {
        target = target || 'scopeOverlay';
        prefix = prefix || 'CH';
        if (!preserve)
            plots.clear(target);
        var summaries = [];
        for (var j = 0; j < this.channels.length; j++) {
            var n = this.channels[j], r = this.channelsData[n], p = r.preamble, x = [], y = [];
            var fullNativeChart = this.nativeProcessing && typeof plots.setNativeSamples === 'function';
            if (fullNativeChart) {
                // Original RAW chunks remain available for lossless FileIO export.
                // Native samples are Qt implicitly-shared across the worker/GUI
                // boundary, so the chart retains all points after release().
                var joined = Binary.concat(r.chunks);
                if (!joined || joined.length !== r.count * (this.format === 'WORD' ? 2 : 1))
                    throw Error('CH' + n + ' native chart concatenation failed: ' + Binary.lastError);
                try {
                    var scale = p.yIncrement, bias = -(p.yOrigin + p.yReference) * scale;
                    var data = joined.decodeSamples(this.format === 'WORD' ? 'u16le' : 'u8',
                        0, r.count, scale, bias);
                    if (!data)
                        throw Error('CH' + n + ' native chart decode failed: ' + joined.lastError);
                    try {
                        var xStartMs = (p.xOrigin + (firstIndex - 1 - p.xReference) * p.xIncrement) * 1000;
                        plots.setNativeSamples(target, prefix + n, data, p.xIncrement * 1000,
                            xStartMs, { color: COLORS[n], width: 2 });
                    } finally {
                        data.release();
                    }
                } finally {
                    joined.release();
                }
            }
            if (this.nativeProcessing && !fullNativeChart) {
                for (var ni = 0; ni < r.nativePoints.length; ni++) {
                    var e = r.nativePoints[ni];
                    x.push((p.xOrigin + (firstIndex - 1 + e.index - p.xReference) * p.xIncrement) * 1000);
                    y.push(e.value);
                }
            }
            for (var i = 0; !this.nativeProcessing && i < r.bins.length; i++) {
                var b = r.bins[i];
                if (!b)
                    continue;
                var first = b.lowIndex <= b.highIndex ? { at: b.lowIndex, v: b.low } : { at: b.highIndex, v: b.high };
                var second = b.lowIndex > b.highIndex ? { at: b.lowIndex, v: b.low } : { at: b.highIndex, v: b.high };
                x.push((p.xOrigin + (firstIndex - 1 + first.at - p.xReference) * p.xIncrement) * 1000);
                y.push(first.v);
                if (second.at !== first.at) {
                    x.push((p.xOrigin + (firstIndex - 1 + second.at - p.xReference) * p.xIncrement) * 1000);
                    y.push(second.v);
                }
            }
            if (!fullNativeChart)
                plots.setSeries(target, prefix + n, x, y, { color: COLORS[n], width: 2 });
            var stats = { vpp: r.max - r.min, rms: Math.sqrt(r.squares / r.count) };
            r.stats = stats;
            summaries.push('CH' + n + ' ' + stats.vpp.toFixed(3) + ' Vpp');
            log('CAPTURE', 'FULL CH' + n + ' raw=' + r.count +
                ' plotted=' + (fullNativeChart ? ('native-full-' + r.count) : x.length) +
                ' RMS=' + stats.rms.toFixed(4) + ' Vpp=' + stats.vpp.toFixed(4));
        }
        return summaries;
    };
    return result;
}
/** OPERATOR SCOPE CONTROL: every button has one action; NO mandatory PREPARE.
 * The physical oscilloscope retains control of timebase, vertical scales,
 * channels, trigger and memory unless the operator explicitly changes one.
 * READ DISPLAY / FULL RECORD do not start, stop, or reconfigure acquisition.
 */
var TIMEBASE_CHOICE = {
    '100 us/div': .0001, '200 us/div': .0002, '500 us/div': .0005,
    '1 ms/div': .001, '2 ms/div': .002, '5 ms/div': .005,
    '10 ms/div': .01, '20 ms/div': .02, '50 ms/div': .05,
    '100 ms/div': .1, '200 ms/div': .2, '500 ms/div': .5, '1 s/div': 1
};
var VDIV_CHOICE = { '10 mV/div': .01, '20 mV/div': .02, '50 mV/div': .05,
    '100 mV/div': .1, '200 mV/div': .2, '500 mV/div': .5,
    '1 V/div': 1, '2 V/div': 2, '5 V/div': 5 };
function scopeSelectedChannel() {
    var s = sel('scopePick'), n = Number(String(s).replace('CH', ''));
    if (![1, 2, 3, 4].includes(n))
        throw Error('Select scope CH1, CH2, CH3 or CH4');
    return n;
}
/** Always acquire each PHYSICALLY enabled input channel. Never infer the
 * acquisition list from cached toggles or a CH1-only display choice.
 * Re-query DISPlay before every SCREEN / SINGLE / FULL RECORD operation so
 * front-panel changes made outside WFScript are honored too.
 */
function scopeReadChannels(d, op) {
    var enabled = [];
    return sequenceP(op, [1, 2, 3, 4], function (n) {
        return callP(d.channel(n), 'getEnabled').then(function (on) {
            op.ensure();
            var state = Boolean(on);
            if (scopeChannelCache[n]) scopeChannelCache[n].enabled = state;
            setScopeChannelSwitch(n, state);
            if (state) enabled.push(n);
        });
    }).then(function () {
        showScopeChannelCards();
        if (!enabled.length)
            throw Error('No scope channels are ON. Enable CH1–CH4 before acquiring.');
        log('SCOPE', 'Physical enabled channels for capture: ' + enabled.map(function (n) { return 'CH' + n; }).join(', '));
        return enabled;
    });
}
/** Physical channel display ON/OFF. A user toggle is an explicit write;
 * on connect/readback/reconnect setScopeChannelSwitch is read-only.
 * A changed channel lineup invalidates previously captured plot/RAW data.
 */
function scopeToggleChannel(n, checked) {
    if (scopeSyncingControls) return;
    var before = scopeChannelCache[n];
    if (!before || !dev.scope || !dev.scope.connected) {
        setScopeChannelSwitch(n, before ? !!before.enabled : false);
        return message('SCOPE', 'Connect and READ SETTINGS before changing CH' + n + '.');
    }
    var requested = typeof checked === 'boolean' ? checked : Boolean(ui.get('scopeEnable' + n));
    if (requested === !!before.enabled) {
        setScopeChannelSwitch(n, before.enabled);
        return;
    }
    if (busy.scope) {
        setScopeChannelSwitch(n, before.enabled);
        return message('SCOPE', 'Scope busy. Wait for the current operation before changing CH' + n + '.');
    }
    return launch('scope', 'CH' + n + ' physical display ' + (requested ? 'ON' : 'OFF'), function (d, op) {
        return callP(d.channel(n), requested ? 'enable' : 'disable')
            .then(function () { op.ensure(); return callP(d.channel(n), 'getSettings'); })
            .then(function (actual) {
                op.ensure();
                scopeChannelCache[n] = actual;
                setScopeChannelSwitch(n, actual.enabled);
                showScopeChannelCards();
                if (!!actual.enabled !== requested)
                    throw Error('CH' + n + ' physical display did not confirm ' + (requested ? 'ON' : 'OFF'));
                // Channel count affects attainable depth/sample rate. Refresh
                // physical acquisition/trigger readback, never configure them.
                return scopeReadState(d, op).then(function () { return scopeQueryTrigger(d, op); });
            }).then(function () {
                op.ensure();
                plots.clear('scopeOverlay');
                lastFullRawRecord = null;
                put('scopeMeasurements', 'Channel selection changed · previous waveform cleared.');
                put('scopeCapture', 'CH' + n + ' ' + (requested ? 'ON' : 'OFF') + ' verified · SINGLE + VIEW or READ DISPLAY to refresh.');
                return 'CH' + n + ' physical display ' + (requested ? 'ON' : 'OFF') + ' verified. Capture again to display enabled channels.';
            }).catch(function (error) {
                // The hardware might have accepted a write before rejecting a
                // later query; reconcile the switch rather than guessing.
                if (!op.isActive()) throw error;
                return callP(d.channel(n), 'getSettings').then(function (actual) {
                    op.ensure();
                    scopeChannelCache[n] = actual;
                    setScopeChannelSwitch(n, actual.enabled);
                    showScopeChannelCards();
                }, function () {
                    setScopeChannelSwitch(n, before.enabled);
                }).then(function () { throw error; });
            });
    });
}
function onScopeChannel1Toggle(checked) { return scopeToggleChannel(1, checked); }
function onScopeChannel2Toggle(checked) { return scopeToggleChannel(2, checked); }
function onScopeChannel3Toggle(checked) { return scopeToggleChannel(3, checked); }
function onScopeChannel4Toggle(checked) { return scopeToggleChannel(4, checked); }
function scopeReadState(d, op) {
    return Promise.all([callP(d.timebase, 'getScale'), callP(d.acquisition, 'status'), callP(d.acquisition, 'memoryDepth'),
        callP(d.scpi, 'query', [':TIM:ROLL?']), callP(d.acquisition, 'sampleRate'), callP(d.acquisition, 'type'), callP(d.acquisition, 'bits')]).then(function (r) {
        op.ensure();
        var scale = Number(r[0]), state = String(r[1]), depth = Number(r[2]), roll = String(r[3]).trim();
        var rate = Number(r[4]), type = String(r[5]).trim().toUpperCase(), bits = Number(r[6]);
        var slow = scale >= .05 && roll !== '0';
        // Read-only synchronization: never mutate the physical instrument on CONNECT or READ SETTINGS.
        var match = matchScopeScale(scale, TIMEBASE_CHOICE);
        if (match) setScopeSelection('scopeTimeScale', match);
        else log('SCOPE', 'Physical timebase ' + scale + ' s/div not represented by selector. Instrument unchanged.');
        setScopeSelection('scopeRollSetting', roll === '0' ? 'Off' : 'Auto');
        var acqName = type === 'HRES' ? 'HRES ' + bits + '-bit' : 'NORMAL 12-bit';
        if (acqName === 'NORMAL 12-bit' || acqName === 'HRES 14-bit' || acqName === 'HRES 16-bit')
            setScopeSelection('scopeAcqMode', acqName);
        Object.keys(ACQ_DEPTH_CHOICE).forEach(function (label) {
            var requested = ACQ_DEPTH_CHOICE[label];
            var expected = ({ '1K': 1000, '10K': 10000, '100K': 100000, '1M': 1000000, '10M': 10000000,
                '25M': 25000000, '50M': 50000000, '100M': 100000000, '250M': 250000000, '500M': 500000000 })[requested];
            if (requested && expected === depth) setScopeSelection('scopeMemoryChoice', label);
        });
        put('scopeReadback', 'MHO954 ' + state + '  |  ' + (scale * 1000).toPrecision(4) + ' ms/div  |  ' +
            depth + ' pts  |  ' + type + (type === 'HRES' ? ' ' + bits + '-bit' : ' 12-bit') +
            '  |  Auto Roll ' + (roll === '0' ? 'Off' : 'Auto') + (slow ? ' (slow sweep)' : ''));
        put('scopeLiveRate', 'SCOPE READBACK  ' + formatRate(rate) + '  |  ' + depth + ' pts  |  ' + state);
        return { state: state, scale: scale, depth: depth, roll: roll, rate: rate, type: type, bits: bits };
    });
}
/** One serialized operation for all physical scope state including trigger. */
function onScopeSync() {
    return launch('scope', 'SYNC physical MHO954 readbacks', function (d, op) {
        return scopeReadState(d, op)
            .then(function () { return scopeQueryChannels(d, op); })
            .then(function () { return scopeQueryTrigger(d, op); })
            .then(function () { return 'Timebase, four channels, acquisition and trigger synchronized; no scope settings changed'; });
    });
}
function onScopePickChanged() {
    if (scopeSyncingControls) return;
    if (!dev.scope || !dev.scope.connected) {
        showScopeSelectedChannel();
        return;
    }
    return launch('scope', 'selected channel scale readback', function (d, op) {
        var n = scopeSelectedChannel();
        return callP(d.channel(n), 'getSettings').then(function (v) {
            op.ensure();
            scopeChannelCache[n] = v;
            showScopeChannelCards();
            showScopeSelectedChannel();
            return 'CH' + n + ' physical settings read; no settings changed';
        });
    });
}
function onScopeTimeEdited() {
    if (scopeSyncingControls) return;
    var text = sel('scopeTimeScale'), wanted = TIMEBASE_CHOICE[text];
    if (!Number.isFinite(wanted)) return message('SCOPE', 'Unknown timebase selection ' + text);
    return launch('scope', 'SET TIMEBASE ' + text, function (d, op) {
        return callP(d.timebase, 'configure', [{ scale: wanted }])
            .then(function () { op.ensure(); return callP(d.timebase, 'getScale'); })
            .then(function (actual) {
                op.ensure();
                if (Math.abs(actual - wanted) > Math.max(1e-9, wanted * .04))
                    throw Error('Timebase readback ' + actual + ' differs from ' + wanted);
                return scopeReadState(d, op).then(function (s) {
                    return 'Timebase ' + text + ' verified' +
                        (s.scale >= .05 && s.roll !== '0' ? '; Auto Roll enabled at slow sweep' : '');
                });
            });
    });
}
function onScopeVerticalEdited() {
    if (scopeSyncingControls) return;
    var text = sel('scopeVScale'), wanted = VDIV_CHOICE[text];
    if (!Number.isFinite(wanted)) return message('SCOPE', 'Unknown V/div selection ' + text);
    var selected = scopeSelectedChannel();
    // The physical MHO954 ignores/does not confirm scale updates to an OFF
    // channel. Revert the widget immediately instead of presenting an invalid
    // setpoint while the serial readback catches up.
    if (scopeChannelCache[selected] && !scopeChannelCache[selected].enabled) {
        showScopeSelectedChannel();
        return message('SCOPE', 'CH' + selected + ' is OFF. Enable its latching Display switch before adjusting V/div. No scale write sent.');
    }
    return launch('scope', 'SET CH' + selected + ' ' + text, function (d, op) {
        var n = selected;
        return callP(d.channel(n), 'getEnabled').then(function (on) {
            op.ensure();
            if (!on) {
                if (scopeChannelCache[n]) scopeChannelCache[n].enabled = false;
                setScopeChannelSwitch(n, false);
                showScopeSelectedChannel();
                showScopeChannelCards();
                throw Error('CH' + n + ' is physically OFF. Enable CH' + n + ' before changing V/div; no scale write sent.');
            }
            return callP(d.channel(n), 'setScale', [wanted]);
        }).then(function () {
            op.ensure();
            return callP(d.channel(n), 'getSettings');
        }).then(function (actual) {
            op.ensure();
            scopeChannelCache[n] = actual;
            setScopeChannelSwitch(n, actual.enabled);
            showScopeChannelCards();
            showScopeSelectedChannel();
            if (!actual.enabled)
                throw Error('CH' + n + ' turned OFF during scale update; confirm the physical display state.');
            if (Math.abs(Number(actual.scale) - wanted) > Math.max(1e-8, wanted * .04))
                throw Error('CH' + n + ' requested ' + wanted + ' V/div but the MHO954 reports ' + actual.scale +
                    ' V/div. The Panel has restored the physical readback.');
            return 'CH' + n + ' ' + text + ' verified';
        });
    });
}
/** Apply only the EDGE controls after independently confirming the physical
 * trigger mode. Never force a non-EDGE instrument to EDGE on connect/apply.
 */
function onScopeSetTrigger() {
    if (scopeSyncingControls) return;
    var source = sel('scopeTrigSource'), slope = sel('scopeTrigSlope'), level = Number(ui.get('scopeTrigLevel'));
    var slopeScpi = { Rising: 'POS', Falling: 'NEG', Either: 'RFAL' }[slope];
    if (!/^CH[1-4]$/.test(source) || !slopeScpi || !Number.isFinite(level) || Math.abs(level) > 20)
        return message('SCOPE', 'Select CH1–CH4, a valid edge and a finite trigger level within ±20 V');
    return launch('scope', 'APPLY EDGE TRIGGER source/slope/level', function (d, op) {
        return callP(d.scpi, 'query', [':TRIG:MODE?']).then(function (v) {
            op.ensure();
            if (String(v).trim().toUpperCase() !== 'EDGE')
                throw Error('Physical trigger mode is ' + v + '. Change the trigger type on the MHO954 first; no commands sent.');
            return callP(d.channel(Number(source.slice(2))), 'getSettings');
        }).then(function (c) {
            op.ensure();
            // The Rigol programming guide defines analogue EDGE trigger limits
            // relative to channel V/div and offset. Reject invalid levels before writes.
            var low = -4.5 * Number(c.scale) - Number(c.offset);
            var high = 4.5 * Number(c.scale) - Number(c.offset);
            if (level < low || level > high)
                throw Error('Level ' + level + ' V outside ' + source + ' physical trigger range ' +
                    low.toPrecision(4) + ' to ' + high.toPrecision(4) + ' V; no changes sent');
            return callP(d.scpi, 'write', [':TRIG:EDGE:SOUR ' + source.replace('CH', 'CHAN')]);
        }).then(function () { op.ensure(); return callP(d.scpi, 'query', [':TRIG:EDGE:SOUR?']); })
            .then(function (v) {
                op.ensure();
                if (normalizeTriggerSource(v) !== source) throw Error('Trigger source not confirmed: ' + v);
                return callP(d.scpi, 'write', [':TRIG:EDGE:SLOP ' + slopeScpi]);
            }).then(function () { op.ensure(); return callP(d.scpi, 'query', [':TRIG:EDGE:SLOP?']); })
            .then(function (v) {
                op.ensure();
                if (normalizeTriggerSlope(v) !== slope) throw Error('Trigger slope not confirmed: ' + v);
                return callP(d.scpi, 'write', [':TRIG:EDGE:LEV ' + level]);
            }).then(function () { op.ensure(); return callP(d.scpi, 'query', [':TRIG:EDGE:LEV?']); })
            .then(function (v) {
                op.ensure();
                if (!Number.isFinite(Number(v)) || Math.abs(Number(v) - level) > .025)
                    throw Error('Trigger level readback differs: ' + v);
                return scopeQueryTrigger(d, op);
            }).then(function () { return 'EDGE trigger verified: ' + source + ', ' + slope + ', ' + level + ' V'; });
    });
}
/** Roll is secondary acquisition behavior, not a trigger mode.
 * The UI updates from :TIM:ROLL? without emitting a selector action.
 */
function onScopeRollEdited() {
    if (scopeSyncingControls) return;
    var selected = sel('scopeRollSetting');
    if (selected !== 'Off' && selected !== 'Auto')
        return message('SCOPE', 'Invalid Auto Roll option ' + selected);
    var wanted = selected === 'Off' ? '0' : '1';
    return launch('scope', 'SET Auto Roll ' + selected, function (d, op) {
        return callP(d.scpi, 'write', [':TIM:ROLL ' + wanted])
            .then(function () { op.ensure(); return callP(d.scpi, 'query', [':TIM:ROLL?']); })
            .then(function (readback) {
                op.ensure();
                if (String(readback).trim() !== wanted)
                    throw Error('Auto Roll readback ' + readback + ' != requested ' + wanted);
                return scopeReadState(d, op);
            }).then(function () { return 'Auto Roll ' + selected + ' verified'; });
    });
}
function renderScreenFrame(f, channels, serial) {
    if (!f || !f.time || f.time.length !== 1000)
        throw Error('Scope did not return a complete 1000-point NORMAL screen record');
    var validated = [];
    channels.forEach(function (n) {
        var c = f['ch' + n];
        if (!c || !c.voltage || c.voltage.length !== 1000)
            throw Error('SCREEN CH' + n + ' returned incomplete waveform');
        validated.push({ n: n, ch: c, stats: summarize(c.voltage) });
    });
    plots.clear('scopeOverlay');
    var sum = [];
    validated.forEach(function (c) { chartWave(c.n, f.time, c.ch.voltage); sum.push('CH' + c.n + ' ' + c.stats.vpp.toFixed(3) + ' Vpp'); });
    var span = 1000 * (f.time[f.time.length - 1] - f.time[0]);
    put('scopeCapture', 'SCREEN #' + serial + ' · ' + channels.length + ' CH · 1000 pts/channel · ' + span.toFixed(3) + ' ms · ' + sum.join(' | '));
    var primary = validated[0];
    put('scopeMeasurements', 'CH' + primary.n + '  RMS ' + primary.stats.rms.toFixed(4) + ' V   |   Vpp ' + primary.stats.vpp.toFixed(4) + ' V');
    log('CAPTURE', 'SCREEN VERIFIED #' + serial + ' ' + JSON.stringify({ channels: channels, spanMs: span, measurements: sum }));
    return 'Display readout updated; physical scope setup unchanged';
}
function operatorScreenRead(d, op, channels, serial) {
    // The MHO954 verified driver requires stopped/frozen memory for a coherent frame.
    // Never secretly STOP a scope that the user deliberately set to RUN.
    return callP(d.acquisition, 'status').then(function (s) {
        op.ensure();
        if (s !== 'STOP')
            throw Error('Scope is ' + s + '. Press STOP or SINGLE + VIEW first; READ DISPLAY never changes acquisition state.');
        return callP(d.waveform, 'captureAll', [{ channels: channels, mode: 'NORMAL', format: scopeFormat(), start: 1, points: 1000,
                chunkPoints: 1000, freeze: false, onDiagnostic: function (e) {
                    if (e.phase === 'CAPTURE_VERIFIED' || e.phase === 'TRANSFER_FAILURE')
                        log('ACQDIAG', '#' + serial + ' SCREEN ' + JSON.stringify(e));
                } }]);
    }).then(function (f) { op.ensure(); var description = renderScreenFrame(f, channels, serial); return scopeReadState(d, op).then(function () { return description; }); })
        .catch(function (err) {
        // A 5-point NORM readback at 100ms/div was physically observed in v0.4.4.
        // Identify the unsupported sweep state without claiming an untested automatic fix.
        if (!/window|:WAV:STOP|preamble|waveform|points/i.test(textErr(err)))
            throw err;
        return scopeReadState(d, op).then(function (s) {
            throw Error('Screen readout unavailable (' + textErr(err) + '). ' +
                (s.scale >= .05 && s.roll !== '0' ? 'Auto Roll at slow sweep: select Auto Roll Off, then SINGLE + VIEW. ' : '') +
                'Instrument settings preserved; no partial waveform plotted.');
        }, function () { throw err; });
    });
}
function onScopeCh1() {
    return launch('scope', 'READ DISPLAY (no acquisition changes)', function (d, op) {
        var serial = ++scopeCaptureSerial;
        return scopeReadChannels(d, op).then(function (channels) { return operatorScreenRead(d, op, channels, serial); });
    }, { timeoutMs: 90000 });
}
function onScopeSingle() {


    return launch('scope', 'SINGLE + VIEW (current scope setup)', function (d, op) {
        var serial = ++scopeCaptureSerial, begin = Date.now(), sawActive = false, firstPoll = true, slowScale = .001;
        function poll() {
            op.ensure();
            var maxMs = Math.max(12000, Math.min(75000, slowScale * 12000 + 15000));
            if (Date.now() - begin > maxMs)
                throw Error('SINGLE acquisition did not complete within ' + maxMs + ' ms');
            return op.delay(200).then(function () { return callP(d.acquisition, 'status'); }).then(function (s) {
                op.ensure();
                if (s !== 'STOP')
                    sawActive = true;
                if (firstPoll && s === 'STOP')
                    log('ACQDIAG', 'SINGLE #' + serial + ' STOP on first poll: freshness not independently established');
                firstPoll = false;
                if (s === 'STOP')
                    return true;
                if (['RUN', 'WAIT', 'AUTO', 'TD'].indexOf(s) < 0)
                    throw Error('Unexpected trigger state ' + s);
                return poll();
            });
        }
        return scopeReadChannels(d, op).then(function (channels) {
            return callP(d.timebase, 'getScale').then(function (scale) {
                slowScale = Number(scale);
                op.ensure();
                put('scopeCapture', 'SINGLE armed · using existing ' + (slowScale * 1000).toPrecision(4) + ' ms/div setting');
                return callP(d.acquisition, 'single').then(function () { return poll(); }).then(function () {
                    return operatorScreenRead(d, op, channels, serial);
                }).then(function () { return 'SINGLE completed, ' + (sawActive ? 'hardware active state observed' : 'STOP first-poll; freshness not independently confirmed') + '; screen updated'; });
            });
        });
    }, { timeoutMs: 105000 });
}
function onScopeRun() {


    return launch('scope', 'RUN', function (d, op) {
        return callP(d.acquisition, 'run').then(function () { put('scopeCapture', 'RUN · acquiring on physical MHO954'); return scopeReadState(d, op); })
            .then(function () { return 'Scope RUN; use STOP before READ DISPLAY/FULL RECORD'; });
    });
}
function onScopeStop() {


    return launch('scope', 'STOP', function (d, op) {
        return callP(d.acquisition, 'stop').then(function () { return callP(d.acquisition, 'status'); })
            .then(function (s) {
            op.ensure();
            if (s !== 'STOP')
                throw Error('Scope did not confirm STOP: ' + s);
            return scopeReadState(d, op).then(function () { return 'Scope STOP verified; ready for READ DISPLAY or FULL RECORD'; });
        });
    });
}
function onScopeFull1(requestedChunkPoints) {
    // UI event invocations may provide an event object; accept only an explicit
    // validated integer supplied by the Binary Transfer Lab handler.
    var blockPoints = Number.isSafeInteger(requestedChunkPoints) ? requestedChunkPoints : FULL_RAW_CHUNK;
    if ([100000, 250000, 500000, 1000000].indexOf(blockPoints) < 0) {
        message('SCOPE', 'Invalid full RAW block size ' + blockPoints);
        return;
    }
    return launch('scope', 'DOWNLOAD COMPLETE STOPPED RECORD', function (d, op) {
        var serial = ++scopeCaptureSerial;
        return scopeReadChannels(d, op).then(function (channels) { return readFullRecord(d, op, channels, serial, blockPoints); });
    }, { timeoutMs: 300000 });
}
function readFullRecord(d, op, channels, serial, blockPoints) {
    blockPoints = blockPoints || FULL_RAW_CHUNK;
    return callP(d.acquisition, 'status').then(function (state) {
        op.ensure();
        if (state !== 'STOP')
            throw Error('FULL RAW requires an existing STOPPED acquisition. Press STOP or SINGLE + VIEW first.');
        return callP(d.acquisition, 'memoryDepth');
    }).then(function (depth) {
        op.ensure();
        if (!Number.isSafeInteger(depth) || depth < 1000)
            throw Error('Invalid instrument record depth ' + depth);
        if (depth > FULL_RAW_MAX_PER_CH)
            throw Error('Actual ' + depth + ' points/channel exceeds ' + FULL_RAW_MAX_PER_CH +
                ' point safety limit. No partial data downloaded; select a smaller instrument memory depth.');
        if (blockPoints > FULL_RAW_CHUNK && depth < blockPoints)
            throw Error('Selected experimental ' + blockPoints +
                '-point block is larger than the STOPPED record (' + depth + ' points). Acquire ' +
                'at least ' + blockPoints + ' points or select a smaller block; no transfer started.');
        return callP(d.acquisition, 'sampleRate').then(function (rate) {
            op.ensure();
            if (!Number.isFinite(rate) || rate <= 0)
                throw Error('Invalid instrument sample rate ' + rate);
            var record = makeFullRecord(channels, depth, rate, serial), format = record.format, total = depth * channels.length, done = 0, lastPercent = -1;
            var nativeMode = !!(d.transport && d.transport.nativeTransport);
            if (nativeMode && (typeof Binary === 'undefined' || !Binary))
                throw Error('Native Binary RC1.3 unavailable');
            var streamMetrics = null;
            log('CAPTURE', 'FULL RECORD #' + serial + ' [' + (nativeMode ? 'NATIVE' : 'LEGACY') + '] ' + depth + ' pts/channel × ' + channels.length + ' channels, ' + Math.ceil(depth / blockPoints) + ' requested blocks/channel (' + blockPoints + ' points/block); existing STOP record');
            function traceTiming(e) {
                if (['RECOVERY_BEGIN', 'RECOVERY_PASS', 'WINDOW_INVALID', 'BLOCK_DOWNGRADE', 'TRANSFER_FAILURE'].indexOf(e.phase) >= 0)
                    log('ACQDIAG', 'FULL #' + serial + ' ' + JSON.stringify(e));
                if (e.phase === 'CHUNK_TIMING' && blockPoints > 100000)
                    log('BLOCK', 'FULL #' + serial + ' CH' + e.channel + ' ' + e.start + '..' + (e.start + e.points - 1) +
                        ' pts=' + e.points + ' bytes=' + e.bytes + ' window=' + e.windowMs + ' preamble=' + e.preambleMs +
                        ' payload=' + e.dataMs + ' verify=' + e.verifyMs + ' callback=' + e.callbackMs + ' ms');
                if (e.phase === 'TIMING_SUMMARY')
                    log('PERF', 'FULL #' + serial + ' RAW ' + format + ' ' + e.pointsPerChannel + ' pts/CH ' + e.chunks + ' blocks ' + e.bytes + ' bytes: ' +
                        'total=' + e.totalMs + ' ms, preflight=' + e.preflightMs + ' ms, window=' + e.windowMs + ' ms, ' +
                        'preamble=' + e.preambleMs + ' ms, payload=' + e.dataMs + ' ms, verification=' + e.verifyMs + ' ms, ' +
                        'decode=' + e.decodeMs + ' ms, panel=' + e.callbackMs + ' ms, final=' + e.finalMs + ' ms' +
                        (e.recoveryUsed ? ' [RECOVERY USED]' : '') + (e.fallbackUsed ? ' [BLOCK SIZE FALLBACK]' : ''));
            }
            return callP(d.waveform, 'streamRaw', [{ channels: channels, format: format, start: 1, points: depth,
                    chunkPoints: blockPoints, freeze: false, nativeProcessing: nativeMode, onDiagnostic: traceTiming, onChunk: function (frame) {
                        op.ensure();
                        if (frame.sampleRate !== rate)
                            throw Error('Scope sample rate changed during RAW transfer');
                        var n = frame.channels[0];
                        if (nativeMode)
                            record.addNative(n, frame.firstIndex, frame);
                        else
                            record.add(n, frame.firstIndex, frame);
                        done += frame.points;
                        var percent = Math.floor(done * 100 / total);
                        if (percent >= lastPercent + 5 || percent === 100) {
                            lastPercent = percent;
                            put('scopeCapture', 'FULL RAW #' + serial + ' · ' + percent + '% · ' + done + '/' + total + ' samples');
                            log('CAPTURE', 'FULL RAW #' + serial + ' ' + percent + '% (' + done + '/' + total + ' points in ' + format + ')');
                        }
                    } }]).then(function (metrics) {
                op.ensure();
                if (metrics.bytes !== depth * channels.length * (format === 'WORD' ? 2 : 1))
                    throw Error('RAW byte-count summary disagrees with requested sample count');
                streamMetrics = metrics;
                return metrics;
            }).then(function () {
                op.ensure();
                record.complete();
                var summary = record.plot();
                lastFullRawRecord = record;
                if (streamMetrics) {
                    var timing = streamMetrics.timing;
                    log('PERF', 'FULL RAW complete ' + JSON.stringify({
                        mode: nativeMode ? 'native' : 'legacy',
                        pointsPerChannel: depth,
                        requestedBlock: streamMetrics.requestedChunkPoints,
                        effectiveBlock: streamMetrics.effectiveChunkPoints,
                        blocks: streamMetrics.chunks,
                        bytes: streamMetrics.bytes,
                        totalMs: timing.totalMs,
                        payloadMs: timing.dataMs,
                        decodeMs: timing.decodeMs,
                        callbackMs: timing.callbackMs,
                        recovered: !!streamMetrics.recoveryUsed,
                        fallback: !!streamMetrics.fallbackUsed
                    }));
                }
                var span = (depth - 1) / rate;
                put('scopeCapture', 'FULL RAW ' + format + ' #' + serial + ' · ' + depth + ' pts/CH · ' + (span * 1000).toFixed(3) + ' ms · ' + summary.join(' | '));
                log('CAPTURE', 'FULL RAW VERIFIED #' + serial + ' ' + depth + ' pts/channel x ' + channels.length + ' channels; no data truncation');
                return 'Complete stopped record retrieved and plotted; SAVE RAW available';
            });
        });
    });
}
/** Lossless RAW export: BYTE or WORD little-endian files under FileIO's 2 MiB limit.
 * A JSON manifest contains the exact Rigol preamble calibration/time axis.
 * User must explicitly request this; no file writes happen on capture/startup.
 */
function onScopeExportRaw() {
    var r = lastFullRawRecord;
    if (!r) {
        message('SCOPE', 'No RAW record retained. Download FULL RECORD first.');
        return;
    }
    try {
        if (typeof FileIO === 'undefined' || (typeof FileIO.writeBytes !== 'function' && typeof FileIO.writeBuffer !== 'function'))
            throw Error('WFScript FileIO is unavailable');
        var stem = 'MHO954_' + new Date(r.timestamp).toISOString().replace(/[^0-9TZ]/g, '_') + '_N' + r.serial;
        var manifest = { format: r.format === 'WORD' ? 'WFScript-MHO900-RAW-WORD-v1' : 'WFScript-MHO900-RAW-BYTE-v1', model: 'MHO954',
            description: 'Lossless transferred SCPI ' + r.format + ' sample codes. WORD uses little-endian unsigned decode pending physical validation.',
            wordByteOrder: r.format === 'WORD' ? 'little' : null, wordInterpretation: r.format === 'WORD' ? 'unsigned16' : null,
            sampleCountPerChannel: r.pointsPerChannel, firstIndex: r.firstIndex || 1, sampleRateSaPerSec: r.sampleRate,
            timestampUtc: new Date(r.timestamp).toISOString(), channels: [] };
        for (var k = 0; k < r.channels.length; k++) {
            var n = r.channels[k], ch = r.channelsData[n], width = r.format === 'WORD' ? 2 : 1;
            if (r.nativeProcessing) {
                if (typeof FileIO.writeBuffer !== 'function' || typeof Binary === 'undefined')
                    throw Error('RC1.3 native FileIO.writeBuffer/Binary required for native RAW export');
                var combined = Binary.concat(ch.chunks);
                if (!combined)
                    throw Error('Native RAW concatenation failed: ' + Binary.lastError);
                try {
                    if (combined.length !== ch.count * width)
                        throw Error('Native RAW byte count mismatch CH' + n);
                    var nativeName = stem + '_CH' + n + '.bin';
                    if (!FileIO.writeBuffer(FileIO.dataPath(nativeName), combined))
                        throw Error('Could not save ' + nativeName + ': ' + FileIO.lastError);
                    manifest.channels.push({ channel: n, file: nativeName, points: ch.count,
                        xIncrement: ch.preamble.xIncrement, xOrigin: ch.preamble.xOrigin, xReference: ch.preamble.xReference,
                        yIncrement: ch.preamble.yIncrement, yOrigin: ch.preamble.yOrigin, yReference: ch.preamble.yReference,
                        rmsV: ch.stats.rms, vppV: ch.stats.vpp, transport: 'native' });
                    log('EXPORT', 'NATIVE CH' + n + ' ' + ch.count + ' ' + r.format + ' samples (' + combined.length + ' bytes) -> ' + nativeName);
                }
                finally {
                    combined.release();
                }
                continue;
            }
            var arr = new Array(ch.count * width), i = 0;
            for (var j = 0; j < ch.chunks.length; j++) {
                var src = ch.chunks[j];
                for (var z = 0; z < src.length; z++) {
                    var sample = src[z];
                    if (width === 1)
                        arr[i++] = sample;
                    else {
                        arr[i++] = sample & 255;
                        arr[i++] = (sample >>> 8) & 255;
                    }
                }
            }
            if (i !== ch.count * width)
                throw Error('Export byte count mismatch CH' + n);
            var name = stem + '_CH' + n + '.bin';
            if (!FileIO.writeBytes(FileIO.dataPath(name), arr))
                throw Error('Could not save ' + name + ': ' + FileIO.lastError);
            manifest.channels.push({ channel: n, file: name, points: ch.count,
                xIncrement: ch.preamble.xIncrement, xOrigin: ch.preamble.xOrigin, xReference: ch.preamble.xReference,
                yIncrement: ch.preamble.yIncrement, yOrigin: ch.preamble.yOrigin, yReference: ch.preamble.yReference,
                rmsV: ch.stats.rms, vppV: ch.stats.vpp });
            log('EXPORT', 'CH' + n + ' ' + ch.count + ' ' + r.format + ' samples (' + arr.length + ' bytes) -> ' + name);
        }
        var manifestName = stem + '.json';
        if (!FileIO.writeJson(FileIO.dataPath(manifestName), manifest))
            throw Error('Could not save ' + manifestName + ': ' + FileIO.lastError);
        message('SCOPE', 'SAVED RAW ' + manifestName + ' + ' + r.channels.length +
            ' channel binary file(s) in ' + FileIO.dataDirectory());
    }
    catch (e) {
        failure('SCOPE', 'SAVE FULL RAW', e);
    }
}
// =============================================================================
// DP932A — Three output channels, limits, measurements and trend history
// =============================================================================
function onPsuEdited() { psuSettingsRevision++; for (var i = 1; i <= 3; i++)
    preparedPsu[i] = false; put('psuReadback', 'Settings edited: CONFIGURE again before output ON.'); }
/** Read all four setpoints from a channel's labeled Numeric Steppers.
 * Keep the same safety validation and hardware setup schema as v0.6.0.
 * No channel is configured or enabled just because an operator edits a value.
 */
function supplyRows() {
    var result = {};
    for (var n = 1; n <= 3; n++) {
        var voltage = Number(ui.get('psu' + n + 'V'));
        var current = Number(ui.get('psu' + n + 'A'));
        var ovp = Number(ui.get('psu' + n + 'OVP'));
        var ocp = Number(ui.get('psu' + n + 'OCP'));
        if (![voltage, current, ovp, ocp].every(Number.isFinite))
            throw Error('CH' + n + ': enter numeric voltage, current, OVP and OCP settings');
        if (voltage < 0 || current <= 0 || ovp <= voltage || ocp <= current)
            throw Error('CH' + n + ': require V >= 0, current > 0, OVP > V and OCP > current');
        result[n] = { voltage: voltage, current: current, ovp: ovp, ocp: ocp,
                      enableOVP: true, enableOCP: true };
    }
    return result;
}
function configurePsu(n) {
    return launch('psu', 'configure CH' + n + ' with protections, output OFF', function (d, op) {
        var o = supplyRows()[n], editRevision = psuSettingsRevision;
        if (!o)
            throw new Error('No setpoint row for CH' + n);
        preparedPsu[n] = false;
        log('PSU', 'CH' + n + ' SETUP ' + JSON.stringify(o) + '; requires output OFF');
        return callP(d.channel(n), 'setup', [o]).then(function () {
            op.ensure();
            if (editRevision !== psuSettingsRevision)
                throw new Error('PSU controls changed during CONFIGURE; configure again before enabling');
            preparedPsu[n] = true;
            return 'CH' + n + ' configured ' + o.voltage + ' V / ' + o.current + ' A, OVP ' + o.ovp + ' V, OCP ' + o.ocp + ' A; remains OFF';
        });
    });
}
function onPsuConfig1() { configurePsu(1); }
function onPsuConfig2() { configurePsu(2); }
function onPsuConfig3() { configurePsu(3); }
function outputPsu(n, on) {
    return launch('psu', 'CH' + n + ' output ' + (on ? 'ON' : 'OFF'), function (d, op) {
        if (on && !preparedPsu[n])
            throw new Error('CH' + n + ' requires CONFIGURE while output OFF before ON');
        if (on)
            log('SAFETY', 'Operator explicitly requested DP932A CH' + n + ' OUTPUT ON');
        var c = d.channel(n);
        return callP(c, 'output', [on]).then(function () { op.ensure(); return callP(c, 'getOutput'); })
            .then(function (reported) {
            op.ensure();
            if (reported !== on)
                throw new Error('CH' + n + ' output readback mismatch; requested ' + on + ' received ' + reported);
            put('psuReadback', 'CH' + n + ' ' + (reported ? 'ON' : 'OFF') + ' (verified)');
            put('psuLive' + n, 'CH' + n + ' OUTPUT ' + (reported ? 'ON' : 'OFF') + ' verified. READ CHANNELS for measured V / A.');
            return 'CH' + n + ' OUTPUT ' + (reported ? 'ON' : 'OFF') + ' confirmed';
        });
    });
}
function onPsuOn1() { outputPsu(1, true); }
function onPsuOff1() { outputPsu(1, false); }
function onPsuOn2() { outputPsu(2, true); }
function onPsuOff2() { outputPsu(2, false); }
function onPsuOn3() { outputPsu(3, true); }
function onPsuOff3() { outputPsu(3, false); }
function addHistory(series, value) {
    series.push({ t: (Date.now() - traceEpoch) / 1000, v: Number(value) });
    if (series.length > 120)
        series.shift();
}
function plotHistory(plotId, name, history, color) {
    var x = [], y = [];
    for (var i = 0; i < history.length; i++) {
        x.push(history[i].t);
        y.push(history[i].v);
    }
    plots.setSeries(plotId, name, x, y, { color: color, width: 2.1 });
}
function updatePowerCharts() {
    plots.clear('powerVoltage');
    plots.clear('powerCurrent');
    for (var n = 1; n <= 3; n++) {
        if (histories.psuV[n].length)
            plotHistory('powerVoltage', 'PSU CH' + n + ' V', histories.psuV[n], COLORS[n]);
        if (histories.psuI[n].length)
            plotHistory('powerCurrent', 'PSU CH' + n + ' A', histories.psuI[n], COLORS[n]);
    }
    if (histories.meter.length)
        plotHistory('meterTrend', 'DM858 V', histories.meter, COLORS.meter);
}
function readSupplyAll() {
    return launch('psu', 'read all channel outputs, modes, live V/I/P', function (d, op) {
        var lines = [], recorded = [];
        return sequenceP(op, [1, 2, 3], function (n) {
            var c = d.channel(n), row = { n: n };
            return callP(c, 'getOutput').then(function (on) { op.ensure(); row.on = on; return callP(c, 'getMode'); })
                .then(function (mode) { op.ensure(); row.mode = mode; return callP(c, 'measure'); })
                .then(function (r) {
                op.ensure();
                row.r = r;
                recorded.push(row);
                var line = 'CH' + n + ' ' + (row.on ? 'ON' : 'OFF') + ' ' + r.voltage.toFixed(4) + ' V ' + r.current.toFixed(4) +
                    ' A ' + r.power.toFixed(3) + ' W ' + row.mode;
                lines.push(line);
                log('PSU', 'READ ' + line);
            });
        }).then(function () {
            op.ensure();
            // Publish only after all 3 channels have completed, no partial snapshots on failure.
            recorded.forEach(function (row) {
                addHistory(histories.psuV[row.n], row.r.voltage);
                addHistory(histories.psuI[row.n], row.r.current);
            });
            recorded.forEach(function (row) {
                var r = row.r;
                ui.set('psuVRead' + row.n, r.voltage);
                ui.set('psuIRead' + row.n, r.current);
                put('psuLive' + row.n,
                    'CH' + row.n + '  ' + (row.on ? 'ON' : 'OFF') +
                    '  |  ' + r.power.toFixed(3) + ' W  |  ' + row.mode);
            });
            put('psuReadback', lines.join(' | '));
            updatePowerCharts();
            return 'Read all three PSU channels';
        });
    });
}
function onPsuReadAll() { readSupplyAll(); }
function onPsuAllOff() {
    return launch('psu', 'ALL OUTPUTS OFF with per-channel readback', function (d, op) {
        log('SAFETY', 'Operator requested DP932A all OFF; will verify CH1-CH3');
        return callP(d.output, 'allOff').then(function () {
            op.ensure();
            return sequenceP(op, [1, 2, 3], function (n) {
                return callP(d.channel(n), 'getOutput').then(function (on) {
                    op.ensure();
                    if (on)
                        throw new Error('CH' + n + ' remains ON after ALL OFF');
                });
            });
        }).then(function () {
            op.ensure();
            for (var k = 1; k <= 3; k++) {
                preparedPsu[k] = false;
                put('psuLive' + k, 'CH' + k + ' OUTPUT OFF verified. READ CHANNELS for measured V / A.');
            }
            return 'PSU CH1-CH3 all OFF (verified)';
        });
    });
}
function onPsuErrors() { run('psu', 'SCPI error queue', function (d, done) { d.status.errors(function (e, items) { if (!e)
    log('PSU', 'SCPI ERRORS ' + JSON.stringify(items)); done(e, e ? null : items.length ? JSON.stringify(items) : 'No supply SCPI errors'); }); }); }
// =============================================================================
// DM858 — On-demand measurements and opt-in one-second monitor
// =============================================================================
function readMeter(auto) {
    if (busy.meter) {
        meterSkipped++;
        return Promise.resolve(null);
    }
    return launch('meter', 'read ' + sel('meterMode'), function (d, op) {
        var kind = sel('meterMode'), meth = kind === 'AC Voltage' ? 'acVoltage' : 'dcVoltage';
        return callP(d.measure, meth, [{ range: 10 }]).then(function (r) {
            op.ensure();
            if (r.overload) {
                put('meterValue', 'OVERLOAD / OPEN INPUT');
                log('METER', meth + ' overload/open');
                return 'Meter overload/open input';
            }
            var x = Number(r.value);
            if (!Number.isFinite(x))
                throw new Error('Meter returned nonfinite numeric reading');
            ui.set('meterLarge', x);
            put('meterValue', x.toPrecision(9) + ' ' + r.unit + ' (' + kind + ')');
            addHistory(histories.meter, x);
            updatePowerCharts();
            meterMonitorReads++;
            if (!auto || meterMonitorReads % 10 === 1)
                log('METER', kind + ' value=' + x + ' ' + r.unit + ' sample=' + meterMonitorReads + ' coalesced=' + meterSkipped);
            return 'Meter ' + kind + ' reading ' + x + ' ' + r.unit;
        });
    }, auto ? { quiet: true, timeoutMs: 30000 } : { timeoutMs: 30000 });
}
function onMeterRead() { readMeter(false); }
function onMeterMonitorStart() {
    if (!deviceReady('meter'))
        return;
    if (meterMonitorTimer !== null)
        return;
    meterMonitorTimer = setInterval(function () { if (!busy.meter)
        readMeter(true);
    else
        meterSkipped++; }, 1000);
    message('METER', 'Monitoring ON at 1 second interval; overlapping reads are skipped, not queued.');
    readMeter(true);
}
function onMeterMonitorStop() { stopMeterMonitoring(); message('METER', 'Monitoring OFF. Existing output states unchanged.'); }
function onMeterConfig() {
    run('meter', 'configure DCV 10 V / 5 PLC', function (d, done) { d.configure.dcVoltage({ range: 10, nplc: 5 }, function (e) { done(e, e ? null : 'Meter configured DCV 10 V, NPLC 5'); }); });
}
function onMeterErrors() { run('meter', 'SCPI error queue', function (d, done) { d.status.errors(function (e, items) { if (!e)
    log('METER', 'SCPI ERRORS ' + JSON.stringify(items)); done(e, e ? null : items.length ? JSON.stringify(items) : 'No meter SCPI errors'); }); }); }
function onClearTrends() {
    histories = { psuV: { 1: [], 2: [], 3: [] }, psuI: { 1: [], 2: [], 3: [] }, meter: [] };
    plots.clear('powerVoltage');
    plots.clear('powerCurrent');
    plots.clear('meterTrend');
    message('METER', 'Timestamped trends cleared; hardware unchanged');
}
// =============================================================================
// LIFECYCLE — Cancel pending requests, close sockets and stop timers.
// Closing a session NEVER implies that external power outputs are OFF.
// =============================================================================
function onShutdown() {
    runtimeStopping = true;
    stopMeterMonitoring();
    ['scope', 'gen', 'meter', 'psu'].forEach(function (kind) {
        cancelOperation(kind, 'Panel shutdown', false);
        var d = dev[kind];
        dev[kind] = null;
        busy[kind] = false;
        if (d)
            try {
                d.close();
            }
            catch (e) {
                log('SHUTDOWN', kind + ' close: ' + textErr(e));
            }
    });

    scopeCaptureSerial++;
    log('SHUTDOWN', 'All operations invalidated and four TCP sessions closed. STOP is NOT an electrical interlock. External outputs remain in their physical states.');
}
// OPERATOR ACQUISITION CONTROLS — all hardware changes require explicit APPLY on Page 01.
var ACQ_DEPTH_CHOICE = { 'Keep current': null, 'AUTO': 'AUTO', '1 kpts': '1K', '10 kpts': '10K', '100 kpts': '100K',
    '1 Mpts': '1M', '10 Mpts': '10M', '25 Mpts': '25M', '50 Mpts': '50M', '100 Mpts': '100M', '250 Mpts': '250M', '500 Mpts': '500M' };
function onScopeApplyAcquisition() {
    return launch('scope', 'APPLY ACQUISITION MODE AND MEMORY', function (d, op) {
        var chosen = sel('scopeAcqMode'), requested = ACQ_DEPTH_CHOICE[sel('scopeMemoryChoice')];
        if (requested === undefined)
            throw Error('Unknown memory depth');
        var type = chosen === 'HRES 14-bit' || chosen === 'HRES 16-bit' ? 'HRES' : 'NORMAL';
        var bits = chosen === 'HRES 16-bit' ? 16 : 14;
        var params = { type: type };
        if (type === 'HRES')
            params.bits = bits;
        if (requested !== null)
            params.memoryDepth = requested;
        // This is deliberately a mutating operation, never part of mere SYNC/zoom.


        return callP(d.acquisition, 'configure', [params]).then(function () { op.ensure(); return scopeReadState(d, op); })
            .then(function (actual) {
            if (actual.type !== (type === 'HRES' ? 'HRES' : 'NORM'))
                throw Error('Instrument acquisition mode is ' + actual.type + ' rather than ' + type);
            if (type === 'HRES' && actual.bits !== bits)
                throw Error('High-resolution readback ' + actual.bits + ' does not equal ' + bits);
            var expected = ({ '1K': 1000, '10K': 10000, '100K': 100000, '1M': 1000000, '10M': 10000000, '25M': 25000000,
                '50M': 50000000, '100M': 100000000, '250M': 250000000, '500M': 500000000 })[requested];
            if (expected && actual.depth !== expected)
                throw Error('Requested memory ' + requested + ' but scope selected ' + actual.depth + ' points. Check enabled channels, installed memory option and acquisition mode.');
            put('scopeCapture', 'Acquisition ' + actual.type + (type === 'HRES' ? ' ' + bits + '-bit' : ' 12-bit') + ' / ' + actual.depth + ' points applied. Start a NEW SINGLE capture.');
            return 'Acquisition setup confirmed. New SINGLE capture required; ' + formatRate(actual.rate) + ' actual sample rate.';
        });
    });
}
function onScopeReadAcquisition() { return onScopeSync(); }
// =============================================================================
// STARTUP — Import drivers and initialize local UI only.
// =============================================================================
// QJSEngine compatibility: callbacks and Promise.then(); no async/await required.
try {
    SCPI_CORE = modules.load('WFScpiCore.mjs');
    if (!SCPI_CORE || SCPI_CORE.VERSION !== '0.3.2' || typeof SCPI_CORE.promiseDriver !== 'function')
        throw Error('Incompatible shared WFScpiCore.mjs');
    log('SCPI', 'Shared core ' + SCPI_CORE.VERSION + ' loaded; four instrument drivers use relative ES imports.');
}
catch (error) {
    log('SCPI', 'ERROR loading SCPI core: ' + textErr(error));
}
['scope', 'gen', 'meter', 'psu'].forEach(decodeModule);
try {
    setupPlots();
}
catch (error) {
    log('INIT', 'Plot initialization: ' + textErr(error));
}
put('genReadback', 'Not connected. APPLY requires output OFF.');
put('scopeTriggerInfo', 'TRIGGER · physical mode unknown until CONNECT');
syncScopeChannelSwitches();
showScopeChannelCards();
put('scopeMeasurements', 'No measurement yet.');
put('scopeCapture', 'No current acquisition · SINGLE + VIEW or STOP then FULL RECORD.');
put('scopeReadback', 'Connect to read MHO954 settings.');
put('scopeLiveRate', 'No sample-rate readback yet.');
put('psuReadback', 'Connect to read all three supply channels.');
for (var channelNumber = 1; channelNumber <= 3; channelNumber++)
    put('psuLive' + channelNumber, 'CH' + channelNumber + ' · Connect and press READ CHANNELS.');
put('meterValue', 'Connect and press READ MEASUREMENT.');
put('genStatus', 'Disconnected · outputs remain under instrument control.');
put('scopeStatus', 'Disconnected · no commands sent.');
put('psuStatus', 'Disconnected · outputs remain under instrument control.');
put('meterStatus', 'Disconnected · monitoring OFF.');
updateBinaryState();
log('INIT', 'Rigol Operator Bench v0.6.7 · physical CH1–CH4 display switches · readback-verified channel capture selection · five unchanged SCPI drivers · native/legacy RAW · safe startup.');
log('SAFETY', 'WFScript PANEL SIMULATION does not inhibit external TCP instruments. Disconnect/STOP is NOT OUTPUT OFF.');


// === Rigol v0.6.6: operator controls + shared instrument access ===
// The test procedures are in RigolTestSteps.mjs, NOT in this Panel script.
// RigolTestHost reuses the operator pages' existing SCPI sessions and runP
// serialization. It does not allocate instruments, sockets, or a test runner.
var rigolTestRunning = false;
var RigolTestHost = {
    run: function(which,label,task,timeout) {
        return runP(which,'TEST '+label,task,{testOwned:true,quiet:true,timeoutMs:timeout});
    },
    connected: function(which) {return !!(dev[which] && dev[which].connected);},
    operationActive: function(which) {return !!(operations[which] && operations[which].active);},
    label: function(which) {return DEVICE[which];},
    endpoint: function(which) {var e=endpoint(which);return e.host+':'+e.port;},
    stopping: function() {return runtimeStopping;},
    meterMonitoring: function() {return meterMonitorTimer !== null;},
    wiringConfirmed: function() {var a=ui.get('rigolTestWiring');return a===true||a===1||a==='1';},
    setValue: function(id,value) {ui.set(id,value);},
    display: put,
    log: function(msg) {log('TEST',msg);},
    renderScopeFrame: function(frame,serial) {renderScreenFrame(frame,[1],serial);},
    markUnprepared: function(which) {
        if(which==='psu')preparedPsu[1]=false;
        if(which==='gen')preparedGen[1]=false;
    }
};
// modules.load() shares the native test library already imported by TestRunner.
var rigolTestModule = modules.load('RigolTestSteps.mjs');
function onRigolTestRun() {
    if(rigolTestRunning) {put('rigolTestSafety','Test already running');return;}
    try {rigolTestModule.checkReady();}
    catch(e) {put('rigolTestSafety','Cannot start: '+textErr(e));return;}
    rigolTestRunning=true;
    put('rigolTestSafety','RUNNING — controlled outputs may be live. DO NOT disconnect cables.');
    if(!TestRunner.run()) {
        rigolTestRunning=false;
        put('rigolTestSafety','RUN rejected: '+TestRunner.lastError);
    }
}
function onRigolTestStop() {
    if(!TestRunner.terminate())put('rigolTestSafety','STOP rejected: '+TestRunner.lastError);
    else put('rigolTestSafety','STOP requested — cleanup underway. Verify physical outputs OFF.');
}
function onRigolTestReset() {
    if(rigolTestRunning) {put('rigolTestSafety','Wait for cleanup to finish before RESET.');return;}
    if(!TestRunner.reset())put('rigolTestSafety','RESET rejected: '+TestRunner.lastError);
    else {put('rigolTestSafety','READY — check wiring and outputs before next RUN');rigolTestModule.clearLastWave();}
}
function onRigolTestExportJson() {
    var path=TestRunner.exportReport('json');
    put('rigolTestReport',path?'JSON report: '+path:'Export error: '+TestRunner.lastError);
}
function onRigolTestExportCsv() {
    var path=TestRunner.exportReport('csv');
    put('rigolTestReport',path?'CSV report: '+path:'Export error: '+TestRunner.lastError);
}
TestRunner.onFinished(function(summary) {
    rigolTestRunning=false;
    var actual=rigolTestModule.finish();
    var off=(actual.psuOwned?'PSU CH1 NOT VERIFIED OFF':'PSU CH1 released/untouched')+
        ' · '+(actual.genOwned?'GEN CH1 NOT VERIFIED OFF':'GEN CH1 released/untouched');
    var archive=TestRunner.archiveInfo();
    put('rigolTestSafety','FINISHED '+String(summary.state).toUpperCase()+' — '+off+'. CHECK HARDWARE.');
    put('rigolTestReport', 'Native report '+String(summary.passed)+' PASS / '+String(summary.failed)+' FAIL · archive '+String(archive.status)+
        (archive.path?' · '+archive.path:''));
    log('TEST','FINISHED '+String(summary.state)+' '+off+' archive '+String(archive.status));
});
put('rigolTestSafety','READY · Connect four instruments and confirm wiring; never run unattended.');
put('rigolTestLatest','No CH1 scope capture from a test yet.');
put('rigolTestReport','Reports auto-archive as JSON after cleanup. JSON/CSV buttons export to Documents/WFScript Reports.');
