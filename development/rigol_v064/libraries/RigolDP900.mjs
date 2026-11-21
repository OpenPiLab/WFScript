import * as Core from './WFScpiCore.mjs';
/**
 * RigolDP900.mjs — WFScript DP900-series power supply driver v0.2.0
 * Source: Rigol DP900 Series Programming Guide, July 2024, SW 00.01.02.
 * Written for WFScript/QJSEngine. No browser, Node, or external JS dependencies.
 * This is independently developed and is not affiliated with Rigol.
 *
 * Requires WFScript TCP.create({host,port}) socket.on('connect'|'data'|'error'|'close'),
 * socket.connect(), socket.write(string), socket.close().
 * Only raw SCPI/TCP is implemented. DP900 manual does NOT establish a raw SCPI port;
 * 5555 was physically verified with the DP932A firmware tested on 2026-11-15.
 * A LAN endpoint requiring VXI-11/LXI RPC or HiSLIP is NOT compatible with this
 * driver without a native transport adapter.
 *
 * Electrical safety: no auto-ON, no auto-reset, no auto-reenable of tripped output;
 * closing a connection does NOT turn outputs off. Must not be sole safety interlock.
 */
export const VERSION = '0.2.0';
const MODELS = {
    DP932A: { maxVoltage: [33.6, 33.6, 6.3], maxCurrent: [3.15, 3.15, 3.15], maxOvp: [35.2, 35.2, 6.6], maxOcp: [3.3, 3.3, 3.3] },
    DP932U: { maxVoltage: [33.6, 33.6, 6.3], maxCurrent: [3.15, 3.15, 3.15], maxOvp: [35.2, 35.2, 6.6], maxOcp: [3.3, 3.3, 3.3] },
    DP932E: { maxVoltage: [31.5, 31.5, 6.3], maxCurrent: [3.15, 3.15, 3.15], maxOvp: [33, 33, 6.6], maxOcp: [3.3, 3.3, 3.3] }
};
function error(message) { return new Error('DP900: ' + message); }
function fail(message) { throw error(message); }
function cbCheck(cb) { if (typeof cb !== 'function')
    fail('completion callback is required'); }
function num(value, label, min, max) {
    if (typeof value !== 'number' || !Number.isFinite(value) || value < min || value > max)
        fail(label + ' must be a finite number in [' + min + ', ' + max + ']');
    return String(value);
}
function integer(value, label, min, max) {
    if (!Number.isSafeInteger(value) || value < min || value > max)
        fail(label + ' must be an integer in [' + min + ', ' + max + ']');
    return String(value);
}
function boolean(value) { if (typeof value !== 'boolean')
    fail('boolean value required'); return value ? 'ON' : 'OFF'; }
function pick(value, allowed, label) {
    const s = String(value).toUpperCase();
    if (allowed.indexOf(s) < 0)
        fail(label + ' must be one of ' + allowed.join(', '));
    return s;
}
function chName(n) { integer(n, 'channel', 1, 3); return 'CH' + n; }
function safeToken(s, label) {
    if (typeof s !== 'string' || !s.trim() || /[\r\n\x00-\x1f]/.test(s))
        fail(label + ' is invalid');
    return s.trim();
}
function filename(s) {
    const val = safeToken(s, 'file name');
    if (/["',;]/.test(val))
        fail('file name contains unsupported SCPI separators');
    return val;
}
function ipAddress(s) {
    if (!/^\d{1,3}(?:\.\d{1,3}){3}$/.test(s) || s.split('.').some(v => Number(v) > 255))
        fail('IPv4 address expected');
    return s;
}
function parseNumber(s) {
    const t = String(s).trim();
    if (!t || !/^[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?$/.test(t))
        fail('invalid numeric response: ' + t.slice(0, 90));
    const n = Number(t);
    if (!Number.isFinite(n))
        fail('non-finite response: ' + t);
    return n;
}
function parseBool(s) { const t = String(s).trim().toUpperCase(); if (['1', '+1', 'ON', 'YES'].indexOf(t) >= 0)
    return true; if (['0', '+0', 'OFF', 'NO'].indexOf(t) >= 0)
    return false; fail('invalid boolean response: ' + t); }
function bytesOf(data) {
    if (typeof data === 'string')
        return Array.from(data, x => x.charCodeAt(0) & 255);
    if (Array.isArray(data))
        return data.map(x => Number(x) & 255);
    if (data && typeof data.length === 'number')
        return Array.from(data, x => Number(x) & 255);
    fail('TCP data event must contain bytes');
}
function decode(bytes) { let s = ''; for (let i = 0; i < bytes.length; i += 2048)
    s += String.fromCharCode.apply(null, bytes.slice(i, i + 2048)); return s; }
function infoOf(s) {
    const parts = String(s).trim().split(',');
    if (parts.length < 4)
        fail('invalid *IDN? result: ' + String(s).slice(0, 100));
    return { manufacturer: parts[0].trim(), model: parts[1].trim().toUpperCase(), serial: parts[2].trim(), firmware: parts.slice(3).join(',').trim(), raw: String(s).trim() };
}
function errorOf(s) {
    const m = /^\s*([+-]?\d+)\s*,\s*(.*)\s*$/.exec(String(s));
    if (!m)
        fail('invalid error response ' + s);
    return { code: Number(m[1]), message: m[2].replace(/^"|"$/g, '') };
}
function lists(s) { return String(s).trim().split(',').map(x => x.trim()).filter(x => x !== ''); }
function parseMeasure(raw, n) {
    const items = lists(raw);
    if (items.length !== 3)
        fail('expected three readings (V,A,W); received ' + raw);
    return { channel: n, voltage: parseNumber(items[0]), current: parseNumber(items[1]), power: parseNumber(items[2]), unitVoltage: 'V', unitCurrent: 'A', unitPower: 'W', receivedAt: Date.now(), raw: String(raw).trim() };
}
function parseApply(raw, n) {
    const items = lists(raw);
    if (items.length < 2)
        fail('invalid :APPL? response ' + raw);
    const i = items.length - 2;
    return { channel: n, voltage: parseNumber(items[i]), current: parseNumber(items[i + 1]), raw: String(raw).trim() };
}
function parseTimerGroups(raw) {
    const text = String(raw).trim();
    if (!text)
        return [];
    return text.split(';').filter(Boolean).map(t => {
        const v = lists(t);
        if (v.length !== 4)
            fail('invalid timer group: ' + t);
        return { index: parseNumber(v[0]), voltage: parseNumber(v[1]), current: parseNumber(v[2]), duration: parseNumber(v[3]) };
    });
}
// A transport shared by every channel of a single instrument. Serializes SCPI queries.
const ScpiTransport = Core.ScpiSession;
class DPChannel {
    constructor(parent, n) { this.psu = parent; this.number = n; this.name = chName(n); }
    get limits() { const model = this.psu._model(); return { voltage: model.maxVoltage[this.number - 1], current: model.maxCurrent[this.number - 1], ovp: model.maxOvp[this.number - 1], ocp: model.maxOcp[this.number - 1] }; }
    // Output setpoints do not themselves enable the output.
    setVoltage(v, cb) { this.psu._write(':SOUR' + this.number + ':VOLT ' + num(v, 'voltage V', 0, this.limits.voltage), cb); }
    getVoltage(cb) { this.psu._query(':SOUR' + this.number + ':VOLT?', parseNumber, cb); }
    setCurrent(a, cb) { this.psu._write(':SOUR' + this.number + ':CURR ' + num(a, 'current A', 0, this.limits.current), cb); }
    getCurrent(cb) { this.psu._query(':SOUR' + this.number + ':CURR?', parseNumber, cb); }
    setVoltageStep(v, cb) { this.psu._write(':SOUR' + this.number + ':VOLT:STEP ' + num(v, 'voltage step V', 0, this.limits.voltage), cb); }
    getVoltageStep(cb) { this.psu._query(':SOUR' + this.number + ':VOLT:STEP?', parseNumber, cb); }
    setCurrentStep(a, cb) { this.psu._write(':SOUR' + this.number + ':CURR:STEP ' + num(a, 'current step A', 0, this.limits.current), cb); }
    getCurrentStep(cb) { this.psu._query(':SOUR' + this.number + ':CURR:STEP?', parseNumber, cb); }
    stepVoltage(direction, cb) { this.psu._write(':SOUR' + this.number + ':VOLT ' + pick(direction, ['UP', 'DOWN'], 'direction'), cb); }
    stepCurrent(direction, cb) { this.psu._write(':SOUR' + this.number + ':CURR ' + pick(direction, ['UP', 'DOWN'], 'direction'), cb); }
    set(v, a, cb) {
        this.psu._write(':APPL ' + this.name + ',' + num(v, 'voltage V', 0, this.limits.voltage) + ',' + num(a, 'current A', 0, this.limits.current), cb);
    }
    getSettings(cb) { this.psu._query(':APPL? ' + this.name, raw => parseApply(raw, this.number), cb); }
    enable(cb) { this.output(true, cb); }
    disable(cb) { this.output(false, cb); }
    output(on, cb) { this.psu._write(':OUTP ' + this.name + ',' + boolean(on), cb); }
    getOutput(cb) { this.psu._query(':OUTP? ' + this.name, parseBool, cb); }
    getMode(cb) { this.psu._query(':OUTP:MODE? ' + this.name, s => String(s).trim().toUpperCase(), cb); }
    measure(cb) { this.psu._query(':MEAS:ALL? ' + this.name, s => parseMeasure(s, this.number), cb); }
    measureVoltage(cb) { this.psu._query(':MEAS:VOLT? ' + this.name, parseNumber, cb); }
    measureCurrent(cb) { this.psu._query(':MEAS:CURR? ' + this.name, parseNumber, cb); }
    measurePower(cb) { this.psu._query(':MEAS:POWE? ' + this.name, parseNumber, cb); }
    setOVP(v, cb) { this.psu._write(':OUTP:OVP:VAL ' + this.name + ',' + num(v, 'OVP V', 0.001, this.limits.ovp), cb); }
    getOVP(cb) { this.psu._query(':OUTP:OVP:VAL? ' + this.name, parseNumber, cb); }
    enableOVP(on, cb) { this.psu._write(':OUTP:OVP ' + this.name + ',' + boolean(on), cb); }
    getOVPEnabled(cb) { this.psu._query(':OUTP:OVP? ' + this.name, parseBool, cb); }
    ovpTripped(cb) { this.psu._query(':OUTP:OVP:QUES? ' + this.name, parseBool, cb); }
    clearOVP(cb) { this.psu._write(':OUTP:OVP:CLE ' + this.name, cb); }
    setOCP(a, cb) { this.psu._write(':OUTP:OCP:VAL ' + this.name + ',' + num(a, 'OCP A', 0.001, this.limits.ocp), cb); }
    getOCP(cb) { this.psu._query(':OUTP:OCP:VAL? ' + this.name, parseNumber, cb); }
    enableOCP(on, cb) { this.psu._write(':OUTP:OCP ' + this.name + ',' + boolean(on), cb); }
    getOCPEnabled(cb) { this.psu._query(':OUTP:OCP? ' + this.name, parseBool, cb); }
    ocpTripped(cb) { this.psu._query(':OUTP:OCP:QUES? ' + this.name, parseBool, cb); }
    clearOCP(cb) { this.psu._write(':OUTP:OCP:CLE ' + this.name, cb); }
    setOCPDelay(ms, cb) { this.psu._write(':OUTP:OCP:DEL ' + this.name + ',' + num(ms, 'OCP delay ms', 0, 1000), cb); }
    getOCPDelay(cb) { this.psu._query(':OUTP:OCP:DEL? ' + this.name, s => parseNumber(String(s).replace(/ms$/i, '')), cb); }
    // Does not automatically clear a protection trip or turn the output back on.
    setup(options, cb) {
        cbCheck(cb);
        options = options || {};
        if (typeof options !== 'object')
            fail('setup expects an object');
        if (options.voltage === undefined || options.current === undefined)
            fail('setup requires voltage and current');
        const v = num(options.voltage, 'voltage V', 0, this.limits.voltage);
        const a = num(options.current, 'current A', 0, this.limits.current);
        const steps = [];
        if (options.ovp !== undefined)
            steps.push(':OUTP:OVP:VAL ' + this.name + ',' + num(options.ovp, 'OVP V', 0.001, this.limits.ovp));
        if (options.ocp !== undefined)
            steps.push(':OUTP:OCP:VAL ' + this.name + ',' + num(options.ocp, 'OCP A', 0.001, this.limits.ocp));
        if (options.enableOVP !== undefined)
            steps.push(':OUTP:OVP ' + this.name + ',' + boolean(options.enableOVP));
        if (options.enableOCP !== undefined)
            steps.push(':OUTP:OCP ' + this.name + ',' + boolean(options.enableOCP));
        steps.unshift(':APPL ' + this.name + ',' + v + ',' + a);
        // Safety check: setup is for outputs already OFF. For energized outputs
        // use deliberate single-value methods. Recheck by querying :OUTP?.
        this.getOutput((err, on) => {
            if (err)
                return cb(err);
            if (on)
                return cb(error(this.name + ' is ON; disable output before setup()'));
            this.psu._commands(steps, err2 => {
                if (err2)
                    return cb(err2);
                // Synchronize instrument execution before reporting setup complete.
                this.psu.system.operationComplete((err3, ok) => {
                    if (err3)
                        return cb(err3);
                    if (!ok)
                        return cb(error('*OPC? did not report completion'));
                    cb(null, true);
                });
            });
        });
    }
    // Read back actual state; no cached output state is trusted.
    snapshot(cb) {
        cbCheck(cb);
        this.getSettings((e, settings) => {
            if (e)
                return cb(e);
            this.getOutput((e2, on) => {
                if (e2)
                    return cb(e2);
                this.getMode((e3, mode) => {
                    if (e3)
                        return cb(e3);
                    this.measure((e4, reading) => {
                        if (e4)
                            return cb(e4);
                        cb(null, { channel: this.number, enabled: on, mode, setpoint: settings, reading });
                    });
                });
            });
        });
    }
}
/** DP932A/DP932U/DP932E over WFScript TCP; channels are psu.ch1, ch2, ch3. */
export class DP900 {
    constructor(host, options = {}) {
        if (typeof host !== 'string' || !host.trim())
            fail('IP address or hostname required');
        if (!options || typeof options !== 'object')
            fail('options must be an object');
        this.host = host.trim();
        this.port = options.port == null ? 5555 : Number(options.port);
        integer(this.port, 'TCP port', 1, 65535);
        this.expectedModel = options.expectedModel || null;
        if (this.expectedModel && !MODELS[this.expectedModel])
            fail('unsupported expectedModel');
        this.transport = new ScpiTransport(this.host, this.port, options);
        this.info = null;
        this.ch1 = new DPChannel(this, 1);
        this.ch2 = new DPChannel(this, 2);
        this.ch3 = new DPChannel(this, 3);
        this.channels = [this.ch1, this.ch2, this.ch3];
        this.scpi = { write: (cmd, cb) => this._write(cmd, cb), query: (cmd, cb) => this._query(cmd, s => s, cb), queryBlock: (cmd, cb) => this._queryBlock(cmd, cb) };
        this.promise = Core.promiseDriver(this);
        this.output = { allOff: cb => this._write(':OUTP ALL,OFF', cb), allOn: cb => this._write(':OUTP ALL,ON', cb),
            setPair: (mode, cb) => this._write(':OUTP:PAIR ' + pick(mode, ['OFF', 'SERIES', 'PARALLEL'], 'pairing'), cb),
            getPair: cb => this._query(':OUTP:PAIR?', s => s.toUpperCase(), cb),
            setTrack: (on, cb) => this._write(':OUTP:TRAC ' + boolean(on), cb),
            getTrack: cb => this._query(':OUTP:TRAC?', parseBool, cb),
            setSync: (on, cb) => this._write(':SYST:SYNC ' + boolean(on), cb),
            getSync: cb => this._query(':SYST:SYNC?', parseBool, cb),
            setOffMode: (mode, cb) => this._write(':OUTP:OFFM ' + pick(mode, ['0V', 'IMMEOFF', 'DELAYOFF'], 'off mode'), cb),
            getOffMode: cb => this._query(':OUTP:OFFM?', s => s.toUpperCase(), cb) };
        this.system = { identity: cb => this._query('*IDN?', infoOf, cb),
            options: cb => this._query('*OPT?', s => (!s || s.toUpperCase() === 'NONE') ? [] : lists(s), cb),
            clearStatus: cb => this._write('*CLS', cb),
            operationComplete: cb => this._query('*OPC?', parseBool, cb),
            reset: cb => this._write('*RST', cb),
            selfTest: cb => this._query('*TST?', parseNumber, cb),
            scpiVersion: cb => this._query(':SYST:VERS?', s => s, cb),
            beep: cb => this._write(':SYST:BEEP:IMM', cb),
            setBeeper: (on, cb) => this._write(':SYST:BEEP ' + boolean(on), cb),
            getBeeper: cb => this._query(':SYST:BEEP?', parseBool, cb),
            setBrightness: (val, cb) => this._write(':SYST:BRIG ' + integer(val, 'brightness', 1, 100), cb),
            getBrightness: cb => this._query(':SYST:BRIG?', parseNumber, cb),
            local: cb => this._write(':SYST:LOC', cb), remote: cb => this._write(':SYST:REM', cb),
            setPowerOn: (mode, cb) => this._write(':SYST:POWE ' + pick(mode, ['DEF', 'LAST'], 'power-on mode'), cb),
            getPowerOn: cb => this._query(':SYST:POWE?', s => s.toUpperCase(), cb),
            setTouchLock: (on, cb) => this._write(':SYST:TLOC ' + boolean(on), cb),
            getTouchLock: cb => this._query(':SYST:TLOC?', parseBool, cb),
            setScreenSaver: (on, cb) => this._write(':SYST:SAV ' + boolean(on), cb),
            getScreenSaver: cb => this._query(':SYST:SAV?', parseBool, cb),
            setLanguage: (lang, cb) => this._write(':SYST:LANG:TYPE ' + pick(lang, ['EN', 'CH', 'DE', 'ES', 'FR'], 'language'), cb),
            getLanguage: cb => this._query(':SYST:LANG:TYPE?', s => s, cb),
            setRemoteLock: (on, cb) => this._write(':SYST:KLOC:STAT ' + boolean(on), cb),
            getRemoteLock: cb => this._query(':SYST:KLOC:STAT?', parseBool, cb) };
        this.status = { error: cb => this._query(':SYST:ERR?', errorOf, cb), errors: cb => this._readErrors(cb),
            statusByte: cb => this._query('*STB?', parseNumber, cb), standardEvent: cb => this._query('*ESR?', parseNumber, cb),
            setStandardEventMask: (bits, cb) => this._write('*ESE ' + integer(bits, 'event mask', 0, 255), cb),
            getStandardEventMask: cb => this._query('*ESE?', parseNumber, cb),
            setStatusByteMask: (bits, cb) => this._write('*SRE ' + integer(bits, 'status mask', 0, 255), cb),
            getStatusByteMask: cb => this._query('*SRE?', parseNumber, cb),
            operationCondition: cb => this._query(':STAT:OPER:COND?', parseNumber, cb),
            operationEvent: cb => this._query(':STAT:OPER:EVEN?', parseNumber, cb),
            setOperationMask: (bits, cb) => this._write(':STAT:OPER:ENAB ' + integer(bits, 'operation mask', 0, 65535), cb),
            getOperationMask: cb => this._query(':STAT:OPER:ENAB?', parseNumber, cb),
            questionableEvent: cb => this._query(':STAT:QUES:EVEN?', parseNumber, cb),
            setQuestionableMask: (bits, cb) => this._write(':STAT:QUES:ENAB ' + integer(bits, 'questionable mask', 0, 65535), cb),
            getQuestionableMask: cb => this._query(':STAT:QUES:ENAB?', parseNumber, cb),
            channelCondition: (channel, cb) => this._query(':STAT:QUES:INST:ISUM' + integer(channel, 'channel', 1, 3) + ':COND?', parseNumber, cb),
            channelEvent: (channel, cb) => this._query(':STAT:QUES:INST:ISUM' + integer(channel, 'channel', 1, 3) + ':EVEN?', parseNumber, cb),
            clearRegisters: cb => this._write(':STAT:PRES', cb) };
        this.measure = { all: (source, cb) => this._measureAll(source, cb),
            voltage: (source, cb) => this._measureScalar('VOLT', source, cb),
            current: (source, cb) => this._measureScalar('CURR', source, cb),
            power: (source, cb) => this._measureScalar('POWE', source, cb) };
        this.analyzer = {
            setType: (mode, cb) => this._write(':ANAL:TYPE ' + pick(mode, ['COM', 'CURR'], 'analyzer type'), cb),
            getType: cb => this._query(':ANAL:TYPE?', s => s.toUpperCase(), cb),
            start: cb => this._write(':ANAL:STAT ON', cb), stop: cb => this._write(':ANAL:STAT OFF', cb),
            running: cb => this._query(':ANAL:STAT?', parseBool, cb),
            setCommonItems: (items, cb) => this._setAnalyzerItems(items, 'common', cb),
            getCommonItems: cb => this._query(':ANAL:COMM:MEAS:TYPE?', lists, cb),
            setPulseChannels: (channels, cb) => this._setAnalyzerItems(channels, 'current', cb),
            getPulseChannels: cb => this._query(':ANAL:CURR:MEAS:TYPE?', lists, cb),
            setPulseThreshold: (channel, side, on, value, cb) => this._pulseThreshold(channel, side, on, value, cb),
            getPulseThreshold: (channel, side, cb) => this._query(':ANAL:CURR:THRE? ' + pick(channel, ['CH1', 'CH2'], 'channel') + ',' + pick(side, ['UP', 'LOW'], 'side'), s => { const t = lists(s); return { enabled: parseBool(t[0]), value: parseNumber(t[1]) }; }, cb),
            setSaveRoute: (path, cb) => this._write(':ANAL:SAVE:ROUT ' + filename(path), cb),
            getSaveRoute: cb => this._query(':ANAL:SAVE:ROUT?', s => s, cb),
            setLogging: (on, cb) => this._write(':ANAL:SAVE:STAT ' + boolean(on), cb),
            getLogging: cb => this._query(':ANAL:SAVE:STAT?', parseBool, cb)
        };
        this.timer = {
            selectChannel: (n, cb) => this._write(':TIME:CHAN ' + chName(n), cb),
            getChannel: cb => this._query(':TIME:CHAN?', s => s, cb),
            setCycles: (count, cb) => this._write(':TIME:CYCLE ' + (count === 'infinite' ? 'I' : 'N,' + integer(count, 'cycles', 1, 99999)), cb),
            getCycles: cb => this._query(':TIME:CYCLE?', s => s, cb),
            setEndState: (value, cb) => this._write(':TIME:ENDS ' + pick(value, ['OFF', 'LAST'], 'end state'), cb),
            getEndState: cb => this._query(':TIME:ENDS?', s => s, cb),
            groupCount: cb => this._query(':TIME:GROUP:NUM?', parseNumber, cb),
            selectGroup: (index, cb) => this._write(':TIME:GROUP:INDE ' + integer(index, 'group', 1, 512), cb),
            getGroupIndex: cb => this._query(':TIME:GROUP:INDE?', parseNumber, cb),
            setGroup: (index, v, a, seconds, cb) => this._setTimerGroup(index, v, a, seconds, cb),
            getGroups: (count, cb) => this._queryBlock(':TIME:GROUP:PARA? ' + integer(count, 'group count', 1, 512), bytes => parseTimerGroups(decode(bytes)), cb),
            deleteGroups: (count, cb) => this._write(':TIME:GROUP:DEL ' + integer(count, 'group count', 1, 512), cb),
            setRunMode: (mode, cb) => this._write(':TIME:RUN ' + pick(mode, ['CONTINUE', 'SINGLE'], 'timer run mode'), cb),
            getRunMode: cb => this._query(':TIME:RUN?', s => s, cb),
            start: cb => this._write(':TIME ON', cb), stop: cb => this._write(':TIME OFF', cb),
            running: cb => this._query(':TIME?', parseBool, cb),
            setTrigger: (mode, cb) => this._write(':TIME:TRIG ' + pick(mode, ['MAN', 'MANUAL', 'BUS'], 'timer trigger'), cb),
            getTrigger: cb => this._query(':TIME:TRIG?', s => s, cb),
            fire: cb => this._write('*TRG', cb),
            template: {
                select: (type, cb) => this._write(':TIME:TEMP:SEL ' + pick(type, ['SINE', 'PULSE', 'RAMP', 'UP', 'DN', 'UPDN', 'RISE', 'FALL'], 'template'), cb),
                selected: cb => this._query(':TIME:TEMP:SEL?', s => s, cb),
                setPeriod: (value, cb) => this._write(':TIME:TEMP:PERI ' + num(value, 'period seconds', this.info && this.info.model === 'DP932U' ? 0.1 : 1, 3600), cb),
                getPeriod: cb => this._query(':TIME:TEMP:PERI?', parseNumber, cb),
                setPoints: (value, cb) => this._write(':TIME:TEMP:POINT ' + integer(value, 'points', 1, 512), cb),
                getPoints: cb => this._query(':TIME:TEMP:POINT?', parseNumber, cb),
                setInterval: (value, cb) => this._write(':TIME:TEMP:INTE ' + num(value, 'interval seconds', 0.1, 3600), cb),
                getInterval: cb => this._query(':TIME:TEMP:INTE?', parseNumber, cb),
                setMinimum: (value, cb) => this._write(':TIME:TEMP:MINV ' + num(value, 'minimum', 0, 99), cb),
                getMinimum: cb => this._query(':TIME:TEMP:MINV?', parseNumber, cb),
                setMaximum: (value, cb) => this._write(':TIME:TEMP:MAXV ' + num(value, 'maximum', 0, 99), cb),
                getMaximum: cb => this._query(':TIME:TEMP:MAXV?', parseNumber, cb),
                setSymmetry: (value, cb) => this._write(':TIME:TEMP:SYMM ' + integer(value, 'symmetry', 0, 100), cb),
                setStairSteps: (value, cb) => this._write(':TIME:TEMP:STAI ' + integer(value, 'stair steps', 3, 99999), cb),
                setRiseRate: (value, cb) => this._write(':TIME:TEMP:RISER ' + integer(value, 'rise index', 0, 10), cb),
                setFallRate: (value, cb) => this._write(':TIME:TEMP:FALLR ' + integer(value, 'fall index', 0, 10), cb),
                invert: (on, cb) => this._write(':TIME:TEMP:INVE ' + boolean(on), cb),
                construct: cb => this._write(':TIME:TEMP:CONST', cb)
            }
        };
        this.files = {
            catalog: cb => this._query(':MEM:CAT?', s => s.toUpperCase() === 'NONE' ? [] : lists(s), cb),
            currentDirectory: cb => this._query(':MEM:CDIR?', s => s, cb),
            setDirectory: (dir, cb) => this._write(':MEM:CDIR ' + filename(dir), cb),
            externalDisks: cb => this._query(':MEM:DISK?', s => s.toUpperCase() === 'NONE' ? [] : lists(s), cb),
            exists: (name, cb) => this._query(':MEM:VAL? ' + filename(name), parseBool, cb),
            delete: (name, cb) => this._write(':MEM:DEL ' + filename(name), cb),
            createDirectory: (dir, cb) => this._write(':MEM:MDIR ' + filename(dir), cb),
            save: (name, cb) => this._write(':MEM:STOR ' + filename(name), cb),
            load: (name, cb) => this._write(':MEM:LOAD ' + filename(name), cb),
            lock: (name, on, cb) => this._write(':MEM:LOCK ' + filename(name) + ',' + boolean(on), cb),
            isLocked: (name, cb) => this._query(':MEM:LOCK? ' + filename(name), parseBool, cb),
            saveSlot: (n, cb) => this._write('*SAV ' + integer(n, 'slot', 0, 9), cb),
            recallSlot: (n, cb) => this._write('*RCL ' + integer(n, 'slot', 0, 9), cb)
        };
        this.network = {
            getIP: cb => this._query(':SYST:COMM:LAN:IPAD?', s => s, cb),
            setIP: (addr, cb) => this._write(':SYST:COMM:LAN:IPAD ' + ipAddress(addr), cb),
            getGateway: cb => this._query(':SYST:COMM:LAN:GATE?', s => s, cb),
            setGateway: (addr, cb) => this._write(':SYST:COMM:LAN:GATE ' + ipAddress(addr), cb),
            getMask: cb => this._query(':SYST:COMM:LAN:SMAS?', s => s, cb),
            setMask: (addr, cb) => this._write(':SYST:COMM:LAN:SMAS ' + ipAddress(addr), cb),
            getMAC: cb => this._query(':SYST:COMM:LAN:MAC?', s => s, cb),
            setDHCP: (on, cb) => this._write(':SYST:COMM:LAN:DHCP ' + boolean(on), cb),
            getDHCP: cb => this._query(':SYST:COMM:LAN:DHCP?', parseBool, cb),
            setAutoIP: (on, cb) => this._write(':SYST:COMM:LAN:AUTO ' + boolean(on), cb),
            getAutoIP: cb => this._query(':SYST:COMM:LAN:AUTO?', parseBool, cb),
            setManualIP: (on, cb) => this._write(':SYST:COMM:LAN:MAN ' + boolean(on), cb),
            getManualIP: cb => this._query(':SYST:COMM:LAN:MAN?', parseBool, cb),
            apply: cb => this._write(':SYST:COMM:LAN:APPL', cb)
        };
        this.screen = { captureBmp: cb => this._query(':SYST:PRIN?', s => {
                const hex = String(s).trim().replace(/^0x/i, '').replace(/\s+/g, '');
                if (!hex || hex.length % 2 || /[^0-9a-f]/i.test(hex))
                    fail('unexpected screenshot format (expected hexadecimal BMP)');
                const bytes = [];
                for (let i = 0; i < hex.length; i += 2)
                    bytes.push(parseInt(hex.slice(i, i + 2), 16));
                if (bytes[0] !== 0x42 || bytes[1] !== 0x4D)
                    fail('screenshot response is not a BMP');
                return { mimeType: 'image/bmp', bytes };
            }, cb) };
        this.digitalIO = {
            setInputEnabled: (pin, on, cb) => this._digitalOnly(':TRIG:IN ' + this._pin(pin) + ',' + boolean(on), cb),
            setInputResponse: (pin, value, cb) => this._digitalOnly(':TRIG:IN:RESP ' + this._pin(pin) + ',' + pick(value, ['ON', 'OFF', 'ALTER'], 'response'), cb),
            setInputType: (pin, type, cb) => this._digitalOnly(':TRIG:IN:TYPE ' + this._pin(pin) + ',' + pick(type, ['RISE', 'FALL', 'HIGH', 'LOW'], 'input type'), cb),
            setOutputEnabled: (pin, on, cb) => this._digitalOnly(':TRIG:OUT ' + this._pin(pin) + ',' + boolean(on), cb),
            setOutputSource: (pin, ch, cb) => this._digitalOnly(':TRIG:OUT:SOUR ' + this._pin(pin) + ',' + chName(ch), cb),
            setOutputCondition: (pin, condition, cb) => this._digitalOnly(':TRIG:OUT:COND ' + this._pin(pin) + ',' + pick(condition, ['OUTON', 'OUTOFF', 'AUTO'], 'condition'), cb),
            setOutputPolarity: (pin, polarity, cb) => this._digitalOnly(':TRIG:OUT:POL ' + this._pin(pin) + ',' + pick(polarity, ['POS', 'NEG', 'POSITIVE', 'NEGATIVE'], 'polarity'), cb)
        };
    }
    channel(index) { return this.channels[Number(integer(index, 'channel', 1, 3)) - 1]; }
    _pin(pin) { return pick(pin, ['D1', 'D2', 'D3', 'D4'], 'digital pin'); }
    _digitalOnly(cmd, cb) { if (this.info && this.info.model !== 'DP932U')
        fail('Digital I/O option is documented only for DP932U'); this._write(cmd, cb); }
    _model() { return MODELS[this.info ? this.info.model : (this.expectedModel || 'DP932A')]; }
    connect(cb) {
        cbCheck(cb);
        if (this.connected && this.info) {
            cb(null, this.info);
            return;
        }
        this.transport.connect(err => {
            if (err)
                return cb(err);
            this._query('*IDN?', infoOf, (e, info) => {
                if (e) {
                    this.transport.close();
                    return cb(e);
                }
                if (!MODELS[info.model] || (this.expectedModel && info.model !== this.expectedModel)) {
                    this.transport.close();
                    return cb(error('unexpected model ' + info.model + '; expected ' + (this.expectedModel || 'DP932A/DP932E/DP932U')));
                }
                this.info = info;
                cb(null, info);
            });
        });
    }
    get connected() { return Boolean(this.transport.connected && this.info); }
    close() { this.info = null; this.transport.close(); }
    _write(s, cb) { this.transport.command(s, 'write', cb); }
    _query(s, parser, cb) {
        cbCheck(cb);
        this.transport.command(s, 'query', (err, r) => {
            if (err)
                return cb(err);
            let parsed;
            try {
                parsed = parser(r);
            }
            catch (e) {
                cb(e);
                return;
            }
            cb(null, parsed);
        });
    }
    _queryBlock(s, parser, cb) {
        cbCheck(cb);
        this.transport.command(s, 'block', (err, r) => {
            if (err)
                return cb(err);
            let parsed;
            try {
                parsed = parser(r);
            }
            catch (e) {
                cb(e);
                return;
            }
            cb(null, parsed);
        });
    }
    _commands(cmds, cb) {
        cbCheck(cb);
        let i = 0;
        const next = err => { if (err)
            return cb(err); if (i >= cmds.length)
            return cb(null, true); this._write(cmds[i++], next); };
        next(null);
    }
    _readErrors(cb) {
        cbCheck(cb);
        const errors = [];
        const next = () => this.status.error((err, e) => {
            if (err)
                return cb(err);
            if (e.code === 0 || errors.length >= 19)
                return cb(null, errors);
            errors.push(e);
            next();
        });
        next();
    }
    _measureScalar(mode, source, cb) {
        if (typeof source === 'function') {
            cb = source;
            source = null;
        }
        const name = source == null ? '' : this._sourceName(source, true);
        this._query(':MEAS:' + mode + '?' + (name ? ' ' + name : ''), s => name === 'ALL' ? lists(s).map(parseNumber) : parseNumber(s), cb);
    }
    _measureAll(source, cb) {
        if (typeof source === 'function') {
            cb = source;
            source = null;
        }
        const name = source == null ? '' : this._sourceName(source, false);
        this._query(':MEAS:ALL?' + (name ? ' ' + name : ''), s => parseMeasure(s, typeof source === 'number' ? source : name || null), cb);
    }
    _sourceName(source, all) {
        if (typeof source === 'number')
            return chName(source);
        return pick(source, all ? ['CH1', 'CH2', 'CH3', 'ALL', 'SERIES', 'PARALLEL'] : ['CH1', 'CH2', 'CH3', 'SERIES', 'PARALLEL'], 'measurement source');
    }
    _setAnalyzerItems(items, type, cb) {
        if (!Array.isArray(items) || !items.length || items.length > 3)
            fail('analyzer items must be an array of 1..3 values');
        const allowed = type === 'common' ? ['CH1_V', 'CH1_C', 'CH1_P', 'CH2_V', 'CH2_C', 'CH2_P', 'CH3_V', 'CH3_C', 'CH3_P'] : ['CH1', 'CH2'];
        if (type === 'current' && items.length > 2)
            fail('pulse current analysis uses only CH1 and CH2');
        this._write((type === 'common' ? ':ANAL:COMM:MEAS:TYPE ' : ':ANAL:CURR:MEAS:TYPE ') + items.map(v => pick(v, allowed, 'analyzer item')).join(','), cb);
    }
    _pulseThreshold(channel, side, on, value, cb) {
        this._write(':ANAL:CURR:THRE ' + pick(channel, ['CH1', 'CH2'], 'pulse channel') + ',' + pick(side, ['UP', 'LOW'], 'threshold direction') + ',' + boolean(on) + ',' + num(value, 'threshold A', 0, 3.15), cb);
    }
    _setTimerGroup(index, voltage, current, seconds, cb) {
        const v = num(voltage, 'timer voltage V', 0, 33.6), a = num(current, 'timer current A', 0, 3.15), t = num(seconds, 'timer duration s', 0.001, 3600);
        // Timer channel selection is global; user must explicitly select in panel/script.
        this._commands([':TIME:GROUP:INDE ' + integer(index, 'group index', 1, 512), ':TIME:GROUP:PARA ' + v + ',' + a + ',' + t], cb);
    }
    // Explicit action, not called by close(). Confirmation is the caller's job.
    shutdown(cb) { cbCheck(cb); this.output.allOff(err => { this.close(); cb(err || null, !err); }); }
}
export class DP932A extends DP900 {
    constructor(host, options = {}) { super(host, Object.assign({}, options, { expectedModel: 'DP932A' })); }
}
export default { DP900, DP932A, VERSION };
