import * as Core from './WFScpiCore.mjs';
/**
 * RigolDM858.mjs v0.2.0 — WFScript / QJSEngine SCPI driver.
 * Copyright (c) 2026. Example code; not affiliated with Rigol Technologies.
 *
 * WFScript requirements: TCP.create({host,port}); socket.on(event,handler);
 * socket.connect(), socket.write(string), socket.close(); 'data' is a byte array.
 * modules.load('RigolDM858.mjs') imports this module into the Panel engine.
 *
 * One connection = one serialized SCPI command queue. This module deliberately
 * uses a shared WFScpiCore session and optional additive Promise facade.
 * Shared session provides bounded queues and JS watchdog request timeouts.
 * Never use this driver for safety-critical protection or unattended control.
 */
const MODES = {
    dcVoltage: { scpi: 'VOLT:DC', unit: 'V', range: [0.1, 1, 10, 100, 1000], sense: 'VOLT:DC', nplc: true },
    acVoltage: { scpi: 'VOLT:AC', unit: 'V', range: [0.1, 1, 10, 100, 750], sense: 'VOLT:AC' },
    dcCurrent: { scpi: 'CURR:DC', unit: 'A', range: [0.0001, 0.001, 0.01, 0.1, 1, 3, 10], sense: 'CURR:DC', nplc: true },
    acCurrent: { scpi: 'CURR:AC', unit: 'A', range: [0.0001, 0.001, 0.01, 0.1, 1, 3, 10], sense: 'CURR:AC' },
    resistance: { scpi: 'RES', unit: 'Ohm', range: [100, 1000, 10000, 100000, 1000000, 10000000, 50000000], sense: 'RES', nplc: true },
    resistance4Wire: { scpi: 'FRES', unit: 'Ohm', range: [100, 1000, 10000, 100000, 1000000, 10000000, 50000000], sense: 'FRES', nplc: true },
    capacitance: { scpi: 'CAP', unit: 'F', range: [1e-9, 1e-8, 1e-7, 1e-6, 1e-5, 1e-4, 1e-3, 1e-2], sense: 'CAP' },
    frequency: { scpi: 'FREQ', unit: 'Hz', sense: 'FREQ' },
    period: { scpi: 'PER', unit: 's', sense: 'PER' },
    continuity: { scpi: 'CONT', unit: 'Ohm' },
    diode: { scpi: 'DIOD', unit: 'V' },
    temperature: { scpi: 'TEMP', unit: 'C' }
};
const NPLC_VALUES = [0.4, 5, 20];
const TEMPERATURE_SENSORS = {
    thermocouple: 'TCOUPLE', rtd2: 'RTD', rtd4: 'FRTD',
    thermistor2: 'THERMISTOR', thermistor4: 'FTHERMISTOR'
};
const SENSOR_TYPE_VALUES = {
    thermocouple: ['B', 'E', 'J', 'K', 'N', 'R', 'S', 'T'],
    rtd2: [385, 389, 391, 392], rtd4: [385, 389, 391, 392],
    thermistor2: [2200, 3000, 5000, 10000, 30000],
    thermistor4: [2200, 3000, 5000, 10000, 30000]
};
function fail(message) { throw new Error('DM858: ' + message); }
function callbackOf(options, callback) {
    if (typeof options === 'function')
        return [{}, options];
    return [options == null ? {} : options, callback];
}
function callbackRequired(cb) { if (typeof cb !== 'function')
    fail('a callback function is required'); }
function positiveInteger(value, min, max, name) {
    if (!Number.isSafeInteger(value) || value < min || value > max)
        fail(name + ' must be an integer from ' + min + ' to ' + max);
    return String(value);
}
function numeric(value, name) {
    if (typeof value !== 'number' || !Number.isFinite(value))
        fail(name + ' must be a finite number');
    return String(value);
}
function choice(value, allowed, label) {
    const text = String(value).toUpperCase();
    if (allowed.indexOf(text) < 0)
        fail(label + ' must be ' + allowed.join(', '));
    return text;
}
function asBool(value) {
    if (typeof value !== 'boolean')
        fail('expected a boolean');
    return value ? 'ON' : 'OFF';
}
function quoted(value) {
    if (typeof value !== 'string' || !value || /["\r\n\x00-\x1f]/.test(value))
        fail('SCPI string must be nonempty and must not contain quotes/control characters');
    return '"' + value + '"';
}
function parseNumber(raw) {
    const text = String(raw).trim();
    if (!text)
        fail('instrument returned an empty reading');
    const value = Number(text);
    if (!Number.isFinite(value))
        fail('invalid numeric SCPI response: ' + text.slice(0, 80));
    return value;
}
function parseList(raw) {
    const text = String(raw).trim();
    if (!text)
        return [];
    return text.split(/[;,]/).map(s => parseNumber(s));
}
function makeReading(raw, mode, unit) {
    const value = parseNumber(raw);
    const overload = Math.abs(value) >= 9e37;
    return { value: overload ? null : value,
        unit: unit || (MODES[mode] ? MODES[mode].unit : ''),
        function: mode || 'unknown',
        overload: overload, timestamp: Date.now(), raw: String(raw).trim() };
}
function readOneOrMany(raw, mode, unit) {
    const values = parseList(raw);
    if (values.length === 0)
        return [];
    const all = String(raw).trim().split(/[;,]/).map(s => makeReading(s, mode, unit));
    return all.length === 1 ? all[0] : all;
}
function sameAllowedNumber(value, choices) {
    return choices.some(v => Math.abs(v - value) < Math.max(1, Math.abs(v)) * 1e-10);
}
function modeByName(name) {
    if (!Object.prototype.hasOwnProperty.call(MODES, name))
        fail('unknown measurement function: ' + name);
    return MODES[name];
}
function scpiRange(modeName, value, model) {
    const m = modeByName(modeName);
    if (!m.range)
        fail('range is not adjustable in ' + modeName);
    if (value == null || String(value).toUpperCase() === 'AUTO')
        return 'AUTO';
    if (['MIN', 'MAX', 'DEF'].indexOf(String(value).toUpperCase()) >= 0)
        return String(value).toUpperCase();
    numeric(value, 'range');
    if (!sameAllowedNumber(value, m.range))
        fail('invalid range for ' + modeName + ': ' + value);
    if (model === 'DM858E' && ((modeName === 'capacitance' && value > 0.001) ||
        ((modeName === 'acCurrent' || modeName === 'dcCurrent') && value === 10)))
        fail('range not supported by DM858E');
    if (model === 'DM858' && (modeName === 'acCurrent' || modeName === 'dcCurrent') && value === 3)
        fail('3 A range applies to DM858E; use 10 A range on DM858');
    return String(value);
}
function measurementArgs(modeName, options, model) {
    const m = modeByName(modeName);
    if (modeName === 'temperature') {
        const sensor = options.sensor || 'thermocouple';
        if (!TEMPERATURE_SENSORS[sensor])
            fail('unknown temperature sensor: ' + sensor);
        if (options.type == null)
            return ' ' + TEMPERATURE_SENSORS[sensor];
        const types = SENSOR_TYPE_VALUES[sensor];
        const valid = types.some(v => String(v).toUpperCase() === String(options.type).toUpperCase());
        if (!valid)
            fail('invalid temperature sensor type for ' + sensor);
        return ' ' + TEMPERATURE_SENSORS[sensor] + ',' + String(options.type).toUpperCase();
    }
    if (options.range != null && !m.range)
        fail('range is unsupported for ' + modeName);
    if (options.resolution != null && !m.range)
        fail('resolution is unsupported for ' + modeName);
    if (options.nplc != null && !m.nplc)
        fail('NPLC is unsupported for ' + modeName);
    if (!m.range)
        return '';
    if (options.range == null && options.resolution == null)
        return '';
    const range = scpiRange(modeName, options.range, model);
    if (options.resolution == null)
        return ' ' + range;
    if (range === 'AUTO')
        fail('specify a fixed range when specifying resolution');
    const res = numeric(options.resolution, 'resolution');
    return ' ' + range + ',' + res;
}
function unitFor(dm, name) {
    return name === 'temperature' ? dm._temperatureUnit : modeByName(name).unit;
}
function scpiError(error, prefix) {
    if (error instanceof Error)
        return error;
    const m = error && error.message !== undefined ? error.message : String(error);
    return new Error('DM858 ' + prefix + ': ' + m);
}
/** A physical DM858 or DM858E session, created by new Rigol.DM858(host). */
export class DM858 {
    constructor(host, options = {}) {
        if (typeof host !== 'string' || !host.trim())
            fail('host must be an IP address or mDNS hostname');
        if (typeof options !== 'object' || options == null)
            fail('options must be an object');
        this.host = host.trim();
        this.port = options.port == null ? 5025 : Number(options.port);
        positiveInteger(this.port, 1, 65535, 'TCP port');
        this.maxResponseBytes = options.maxResponseBytes == null ? 24 * 1024 * 1024 : options.maxResponseBytes;
        positiveInteger(this.maxResponseBytes, 1024, 128 * 1024 * 1024, 'maxResponseBytes');
        this.trace = options.trace || null;
        // All four instrument drivers now delegate framing, TCP events,
        // binary blocks, queueing, and timeouts to one WFScpiCore module.
        this.transport = new Core.ScpiSession(this.host, this.port, options);
        this.scpi = {
            write: (cmd, cb) => this._enqueue(cmd, 'write', cb),
            query: (cmd, cb) => this._enqueue(cmd, 'query', cb),
            queryBinary: (cmd, cb) => this._enqueue(cmd, 'binary', cb)
        };
        this.measure = {};
        this.configure = {};
        for (const name of Object.keys(MODES)) {
            this.measure[name] = (opts, cb) => this._measure(name, opts, cb);
            this.configure[name] = (opts, cb) => this._configure(name, opts, cb);
        }
        this.sense = {
            setNPLC: (name, value, cb) => this._setNPLC(name, value, cb),
            getNPLC: (name, cb) => this._getSense(name, 'NPLC', parseNumber, cb),
            setRange: (name, value, cb) => this._setRange(name, value, cb),
            getRange: (name, cb) => this._getSense(name, 'RANG', parseNumber, cb),
            setAutoRange: (name, enabled, cb) => this._setAutoRange(name, enabled, cb),
            getAutoRange: (name, cb) => this._getSense(name, 'RANG:AUTO', s => Number(s) === 1, cb),
            setRelative: (name, enabled, cb) => this._setRelative(name, enabled, cb),
            setRelativeValue: (name, value, cb) => this._setRelativeValue(name, value, cb),
            captureRelativeValue: (name, cb) => this._setRelativeAuto(name, cb)
        };
        this.acquisition = {
            read: cb => this._readAcquisition('READ?', cb),
            fetch: cb => this._readAcquisition('FETC?', cb),
            start: cb => this._enqueue('INIT', 'write', cb),
            abort: cb => this._enqueue('ABOR', 'write', cb),
            storedCount: cb => this._queryConverted('DATA:POIN?', parseNumber, cb),
            last: cb => this._lastReading(cb),
            remove: (count, cb) => this._removeReadings(count, cb),
            readBuffered: (count, cb) => this._bufferedReadings(count, cb),
            setMemoryThreshold: (count, cb) => this._setMemoryThreshold(count, cb),
            getMemoryThreshold: cb => this._queryConverted('DATA:POIN:EVEN:THR?', parseNumber, cb)
        };
        this.trigger = {
            setSource: (source, cb) => this._setTriggerSource(source, cb),
            getSource: cb => this._queryConverted('TRIG:SOUR?', s => ({ IMM: 'immediate', BUS: 'bus', EXT: 'external' })[s.trim()] || s.trim(), cb),
            setCount: (count, cb) => this._enqueue('TRIG:COUN ' + positiveInteger(count, 1, 1000, 'trigger count'), 'write', cb),
            getCount: cb => this._queryConverted('TRIG:COUN?', parseNumber, cb),
            setSamples: (count, cb) => this._enqueue('SAMP:COUN ' + positiveInteger(count, 1, 2000, 'sample count'), 'write', cb),
            getSamples: cb => this._queryConverted('SAMP:COUN?', parseNumber, cb),
            fire: cb => this._enqueue('*TRG', 'write', cb),
            setOutputSlope: (direction, cb) => this._enqueue('OUTP:TRIG:SLOP ' + choice(direction, ['POS', 'NEG', 'POSITIVE', 'NEGATIVE'], 'slope'), 'write', cb)
        };
        this.math = {
            enableStatistics: (enabled, cb) => this._enqueue('CALC:AVER ' + asBool(enabled), 'write', cb),
            clearStatistics: cb => this._enqueue('CALC:AVER:CLE', 'write', cb),
            statistics: cb => this._statistics(cb),
            statisticsCount: cb => this._queryConverted('CALC:AVER:COUN?', parseNumber, cb),
            setLimits: (lower, upper, cb) => this._setLimits(lower, upper, cb),
            enableLimits: (enabled, cb) => this._enqueue('CALC:LIM ' + asBool(enabled), 'write', cb),
            clearLimits: cb => this._enqueue('CALC:LIM:CLE', 'write', cb),
            setDbm: (ohms, cb) => this._setScaleDbm(ohms, cb),
            setDb: (referenceDbm, cb) => this._setScaleDb(referenceDbm, cb),
            enableScale: (enabled, cb) => this._enqueue('CALC:SCAL ' + asBool(enabled), 'write', cb),
            clearAll: cb => this._enqueue('CALC:CLE', 'write', cb)
        };
        this.system = {
            identity: cb => this._queryConverted('*IDN?', parseIdentity, cb),
            reset: cb => this._enqueue('*RST', 'write', cb),
            clearStatus: cb => this._enqueue('*CLS', 'write', cb),
            operationComplete: cb => this._queryConverted('*OPC?', s => Number(s) === 1, cb),
            beep: cb => this._enqueue('SYST:BEEP', 'write', cb),
            setBeeper: (enabled, cb) => this._enqueue('SYST:BEEP:STAT ' + asBool(enabled), 'write', cb),
            getBeeper: cb => this._queryConverted('SYST:BEEP:STAT?', s => Number(s) === 1, cb),
            setTemperatureUnit: (unit, cb) => this._setTemperatureUnit(unit, cb),
            getTemperatureUnit: cb => this._queryConverted('UNIT:TEMP?', s => s.trim(), cb),
            scpiVersion: cb => this._enqueue('SYST:VERS?', 'query', cb)
        };
        this.status = {
            error: cb => this._queryConverted('SYST:ERR?', parseError, cb),
            errors: cb => this._allErrors(cb),
            statusByte: cb => this._queryConverted('*STB?', parseNumber, cb),
            standardEvent: cb => this._queryConverted('*ESR?', parseNumber, cb),
            operationCondition: cb => this._queryConverted('STAT:OPER:COND?', parseNumber, cb),
            questionableCondition: cb => this._queryConverted('STAT:QUES:COND?', parseNumber, cb)
        };
        this.screen = {
            capture: (opts, cb) => this._captureScreen(opts, cb)
        };
        this.network = {
            setMdns: (enabled, cb) => this._enqueue('LXI:MDNS:ENAB ' + asBool(enabled), 'write', cb),
            getMdns: cb => this._queryConverted('LXI:MDNS:ENAB?', s => Number(s) === 1, cb),
            hostname: cb => this._queryConverted('LXI:MDNS:HNAM?', dequote, cb),
            serviceName: cb => this._queryConverted('LXI:MDNS:SNAM?', dequote, cb),
            setServiceName: (name, cb) => this._setServiceName(name, cb),
            restart: cb => this._enqueue('LXI:REST', 'write', cb),
            controlPort: cb => this._queryConverted('SYST:COMM:LAN:CONT?', parseNumber, cb)
        };
        this.files = {
            catalog: (path, cb) => this._fileCatalog(path, cb),
            getDirectory: cb => this._queryConverted('MMEM:CDIR?', dequote, cb),
            setDirectory: (path, cb) => this._enqueue('MMEM:CDIR ' + quoted(path), 'write', cb),
            copy: (src, dst, cb) => this._enqueue('MMEM:COPY ' + quoted(src) + ',' + quoted(dst), 'write', cb),
            move: (src, dst, cb) => this._enqueue('MMEM:MOVE ' + quoted(src) + ',' + quoted(dst), 'write', cb),
            remove: (path, cb) => this._enqueue('MMEM:DEL ' + quoted(path), 'write', cb),
            createDirectory: (path, cb) => this._enqueue('MMEM:MDIR ' + quoted(path), 'write', cb),
            removeDirectory: (path, cb) => this._enqueue('MMEM:RDIR ' + quoted(path), 'write', cb),
            saveState: (path, cb) => this._enqueue('MMEM:STOR:STAT ' + quoted(path), 'write', cb),
            loadState: (path, cb) => this._enqueue('MMEM:LOAD:STAT ' + quoted(path), 'write', cb),
            saveReadings: (path, cb) => this._enqueue('MMEM:STOR:DATA RDG_STORE,' + quoted(path), 'write', cb),
            savePreferences: (path, cb) => this._enqueue('MMEM:STOR:PREF ' + quoted(path), 'write', cb),
            loadPreferences: (path, cb) => this._enqueue('MMEM:LOAD:PREF ' + quoted(path), 'write', cb)
        };
        this.promise = Core.promiseDriver(this);
    }
    connect(callback) {
        callbackRequired(callback);
        if (this.connected && this.info) {
            callback(null, this.info);
            return;
        }
        this.info = null;
        this.transport.connect(err => {
            if (err)
                return callback(scpiError(err, 'connect'));
            this.transport.query('*IDN?', (err2, raw) => {
                if (err2) {
                    this.transport.close();
                    return callback(scpiError(err2, 'identity'));
                }
                try {
                    const info = parseIdentity(raw);
                    if (!/^DM858E?$/i.test(info.model))
                        fail('unexpected *IDN? model: ' + info.model + ' (expected DM858 or DM858E)');
                    this.info = info;
                    callback(null, info);
                }
                catch (e) {
                    this.transport.close();
                    callback(e);
                }
            });
        });
    }
    get connected() { return Boolean(this.transport.connected && this.info); }
    close() { this.info = null; this.transport.close(); }
    _enqueue(command, kind, callback) {
        callbackRequired(callback);
        this.transport.command(command, kind === 'binary' ? 'block' : kind, callback);
    }
    _queryConverted(cmd, parser, cb) {
        this._enqueue(cmd, 'query', (err, text) => {
            if (err)
                return cb(err);
            let converted;
            try {
                converted = parser(text);
            }
            catch (e) {
                return cb(scpiError(e, 'parse'));
            }
            cb(null, converted);
        });
    }
    _measure(name, opts, cb) {
        [opts, cb] = callbackOf(opts, cb);
        callbackRequired(cb);
        if (opts.nplc !== undefined)
            fail('one-shot MEAS resets settings; use configure.' + name + '({nplc:...}) then acquisition.read()');
        const args = measurementArgs(name, opts, this.info && this.info.model);
        const cmd = 'MEAS:' + MODES[name].scpi + '?' + args;
        this._queryConverted(cmd, raw => makeReading(raw, name, unitFor(this, name)), cb);
        this._lastMode = name;
    }
    _configure(name, opts, cb) {
        [opts, cb] = callbackOf(opts, cb);
        callbackRequired(cb);
        const m = modeByName(name);
        const args = measurementArgs(name, opts, this.info && this.info.model);
        this._enqueue('CONF:' + m.scpi + args, 'write', err => {
            if (err)
                return cb(err);
            this._lastMode = name;
            this._lastRange = opts.range == null ? 'AUTO' : opts.range;
            if (opts.nplc === undefined)
                return cb(null, { function: name, range: this._lastRange });
            this._setNPLC(name, opts.nplc, err2 => cb(err2 || null, err2 ? undefined : { function: name, range: this._lastRange, nplc: opts.nplc }));
        });
    }
    _getSense(name, tail, parser, cb) {
        callbackRequired(cb);
        const mode = modeByName(name);
        if (!mode.sense)
            fail('SENSe parameters not available for ' + name);
        this._queryConverted('SENS:' + mode.sense + ':' + tail + '?', parser, cb);
    }
    _setNPLC(name, value, cb) {
        callbackRequired(cb);
        const m = modeByName(name);
        if (!m.nplc)
            fail('NPLC not available for ' + name);
        if (!sameAllowedNumber(value, NPLC_VALUES))
            fail('NPLC must be 0.4, 5, or 20');
        this._enqueue('SENS:' + m.sense + ':NPLC ' + numeric(value, 'NPLC'), 'write', cb);
    }
    _setRange(name, value, cb) {
        callbackRequired(cb);
        const m = modeByName(name);
        if (!m.range)
            fail('range unavailable for ' + name);
        const val = scpiRange(name, value, this.info && this.info.model);
        this._enqueue('SENS:' + m.sense + ':RANG ' + val, 'write', cb);
    }
    _setAutoRange(name, enabled, cb) {
        callbackRequired(cb);
        const m = modeByName(name);
        if (!m.range)
            fail('autoranging unavailable for ' + name);
        this._enqueue('SENS:' + m.sense + ':RANG:AUTO ' + asBool(enabled), 'write', cb);
    }
    _setRelative(name, enabled, cb) {
        callbackRequired(cb);
        const m = modeByName(name);
        if (!m.sense)
            fail('relative unsupported for ' + name);
        this._enqueue('SENS:' + m.sense + ':NULL ' + asBool(enabled), 'write', cb);
    }
    _setRelativeValue(name, value, cb) {
        callbackRequired(cb);
        const m = modeByName(name);
        if (!m.sense)
            fail('relative unsupported for ' + name);
        this._enqueue('SENS:' + m.sense + ':NULL:VAL ' + numeric(value, 'relative value'), 'write', cb);
    }
    _setRelativeAuto(name, cb) {
        callbackRequired(cb);
        const m = modeByName(name);
        if (!m.sense)
            fail('relative unsupported for ' + name);
        this._enqueue('SENS:' + m.sense + ':NULL:VAL:AUTO', 'write', cb);
    }
    _readAcquisition(cmd, cb) {
        callbackRequired(cb);
        this._queryConverted(cmd, raw => readOneOrMany(raw, this._lastMode, unitForKnown(this)), cb);
    }
    _lastReading(cb) {
        callbackRequired(cb);
        this._queryConverted('DATA:LAST?', raw => {
            const s = String(raw).trim();
            const m = /^([+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[Ee][+-]?\d+)?)(?:\s+(.+))?$/.exec(s);
            if (!m)
                fail('unrecognized DATA:LAST? response: ' + s);
            const token = (m[2] || '').trim();
            const known = { VDC: ['dcVoltage', 'V'], VAC: ['acVoltage', 'V'], IDC: ['dcCurrent', 'A'], IAC: ['acCurrent', 'A'],
                RES: ['resistance', 'Ohm'], FRES: ['resistance4Wire', 'Ohm'], CAP: ['capacitance', 'F'],
                FREQ: ['frequency', 'Hz'], PER: ['period', 's'], TEMP: ['temperature', this._temperatureUnit] };
            const mode = known[token] || [this._lastMode, unitForKnown(this)];
            const reading = makeReading(m[1], mode[0], mode[1]);
            reading.instrumentFunction = token || null;
            return reading;
        }, cb);
    }
    _bufferedReadings(count, cb) {
        if (typeof count === 'function') {
            cb = count;
            count = null;
        }
        callbackRequired(cb);
        let command = 'R?';
        if (count != null)
            command += ' ' + positiveInteger(count, 1, this.info && this.info.model === 'DM858E' ? 20000 : 500000, 'reading count');
        this._queryConverted(command, raw => listReadings(raw, this), cb);
    }
    _removeReadings(count, cb) {
        callbackRequired(cb);
        const limit = this.info && this.info.model === 'DM858E' ? 20000 : 500000;
        this._queryConverted('DATA:REM? ' + positiveInteger(count, 1, limit, 'reading count'), raw => listReadings(raw, this), cb);
    }
    _setMemoryThreshold(count, cb) {
        callbackRequired(cb);
        const limit = this.info && this.info.model === 'DM858E' ? 20000 : 500000;
        this._enqueue('DATA:POIN:EVEN:THR ' + positiveInteger(count, 1, limit, 'memory threshold'), 'write', cb);
    }
    _setTriggerSource(source, cb) {
        callbackRequired(cb);
        const value = { immediate: 'IMM', bus: 'BUS', external: 'EXT' }[String(source).toLowerCase()];
        if (!value)
            fail('trigger source must be immediate, bus, or external');
        this._enqueue('TRIG:SOUR ' + value, 'write', cb);
    }
    _statistics(cb) {
        callbackRequired(cb);
        this._queryConverted('CALC:AVER:ALL?', raw => {
            const a = parseList(raw);
            if (a.length !== 4)
                fail('CALC:AVER:ALL? expected four numbers');
            return { mean: a[0], stdDev: a[1], min: a[2], max: a[3] };
        }, cb);
    }
    _setLimits(lower, upper, cb) {
        callbackRequired(cb);
        numeric(lower, 'lower limit');
        numeric(upper, 'upper limit');
        if (lower >= upper)
            fail('lower limit must be less than upper limit');
        // Query existing limits and choose an intermediate-valid order.
        // Setting a positive lower limit before raising the default 0 upper
        // limit would otherwise be rejected by the instrument.
        this._queryConverted('CALC:LIM:LOW?', parseNumber, (err, oldLower) => {
            if (err)
                return cb(err);
            this._queryConverted('CALC:LIM:UPP?', parseNumber, (err2, oldUpper) => {
                if (err2)
                    return cb(err2);
                const first = lower <= oldUpper
                    ? ['CALC:LIM:LOW ' + lower, 'CALC:LIM:UPP ' + upper]
                    : ['CALC:LIM:UPP ' + upper, 'CALC:LIM:LOW ' + lower];
                this._enqueue(first[0], 'write', err3 => {
                    if (err3)
                        return cb(err3);
                    this._enqueue(first[1], 'write', cb);
                });
            });
        });
    }
    _setScaleDbm(ohms, cb) {
        callbackRequired(cb);
        if (typeof ohms !== 'number' || ohms < 2 || ohms > 8000)
            fail('dBm reference resistance must be 2–8000 Ohm');
        this._enqueue('CALC:SCAL:DBM:REF ' + numeric(ohms, 'reference resistance'), 'write', err => {
            if (err)
                return cb(err);
            this._enqueue('CALC:SCAL:FUNC DBM', 'write', cb);
        });
    }
    _setScaleDb(ref, cb) {
        callbackRequired(cb);
        numeric(ref, 'dB reference');
        if (ref < -120 || ref > 120)
            fail('dB reference must be -120 to 120 dBm');
        this._enqueue('CALC:SCAL:DB:REF ' + ref, 'write', err => {
            if (err)
                return cb(err);
            this._enqueue('CALC:SCAL:FUNC DB', 'write', cb);
        });
    }
    _setTemperatureUnit(unit, cb) {
        callbackRequired(cb);
        const u = choice(unit, ['C', 'F', 'K'], 'temperature unit');
        this._enqueue('UNIT:TEMP ' + u, 'write', err => {
            if (!err)
                this._temperatureUnit = u;
            cb(err || null, !err ? u : undefined);
        });
    }
    _setServiceName(name, cb) {
        callbackRequired(cb);
        if (typeof name !== 'string' || name.length > 30)
            fail('mDNS service name max length is 30 characters');
        this._enqueue('LXI:MDNS:SNAM:DES ' + quoted(name), 'write', cb);
    }
    _captureScreen(opts, cb) {
        [opts, cb] = callbackOf(opts, cb);
        callbackRequired(cb);
        const format = choice(opts.format || 'PNG', ['PNG', 'BMP'], 'image format');
        this._enqueue('HCOP:SDUM:DATA:FORM ' + format, 'write', err => {
            if (err)
                return cb(err);
            this._enqueue('HCOP:SDUM:DATA?', 'binary', (err2, bytes) => {
                if (err2)
                    return cb(err2);
                cb(null, { format: format, mimeType: format === 'PNG' ? 'image/png' : 'image/bmp', bytes: bytes });
            });
        });
    }
    _allErrors(cb) {
        callbackRequired(cb);
        const errors = [];
        const next = () => {
            this.status.error((err, item) => {
                if (err)
                    return cb(err);
                if (item.code === 0)
                    return cb(null, errors);
                errors.push(item);
                if (errors.length === 16)
                    return cb(null, errors); // limit drain
                next();
            });
        };
        next();
    }
    _fileCatalog(path, cb) {
        if (typeof path === 'function') {
            cb = path;
            path = null;
        }
        callbackRequired(cb);
        this._enqueue('MMEM:CAT:ALL?' + (path == null ? '' : ' ' + quoted(path)), 'query', cb);
    }
}
function parseIdentity(raw) {
    const parts = String(raw).trim().split(',');
    if (parts.length < 4)
        fail('invalid *IDN? response: ' + String(raw).slice(0, 100));
    return { manufacturer: parts[0].trim(), model: parts[1].trim(), serial: parts[2].trim(),
        firmware: parts.slice(3).join(',').trim(), raw: String(raw).trim() };
}
function parseError(raw) {
    const m = /^\s*([+-]?\d+)\s*,\s*(.*)\s*$/.exec(String(raw));
    if (!m)
        fail('unrecognized SYST:ERR? response: ' + raw);
    return { code: Number(m[1]), message: dequote(m[2]) };
}
function dequote(s) {
    const text = String(s).trim();
    return text.length >= 2 && text[0] === '"' && text[text.length - 1] === '"' ? text.slice(1, -1) : text;
}
function unitForKnown(dm) {
    const name = dm._lastMode;
    if (!Object.prototype.hasOwnProperty.call(MODES, name))
        return '';
    return name === 'temperature' ? dm._temperatureUnit : MODES[name].unit;
}
function listReadings(raw, dm) {
    const s = String(raw).trim();
    if (!s)
        return [];
    return s.split(/[;,]/).map(piece => makeReading(piece, dm._lastMode, unitForKnown(dm)));
}
export const VERSION = '0.2.0';
export default { DM858, VERSION };
