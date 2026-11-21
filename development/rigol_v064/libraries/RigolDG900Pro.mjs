/**
 * RigolDG900Pro.mjs v0.2.0 — QJSEngine-compatible shared-core module.
 * Source: development/RigolDG900Pro.modular.mjs and WFScpiCore.mjs.
 * No nested module loading; no object/array/function spread or optional chaining.
 * No connections, resets, or output changes during construction.
 */
/**
 * RigolDG900Pro.mjs — WFScript driver v0.2.0 for DG902 Pro / DG912 Pro / DG922 Pro.
 * Requires embedded WFScpiCore.mjs v0.3 and WFScript TCP API. Uses nonblocking callbacks,
 * never emits output-on on connect, and never resets the instrument automatically.
 * Source: Rigol DG800 Pro/DG900 Pro Programming Guide, April 2025 (SW 00.02.01).
 * Not affiliated with Rigol; remote TCP port is a configurable candidate, NOT
 * tested on physical DG902 Pro. Does NOT implement VXI-11, VISA or HiSLIP.
 * A successful socket write is NOT evidence that physical output is safe.
 */
import * as Core from './WFScpiCore.mjs';
export const VERSION = '0.2.0';
const MODELS = {
    'DG902 PRO': { sine: 70e6, square: 60e6, ramp: 3e6, pulse: 50e6, arb: 30e6, harmonic: 35e6 },
    'DG912 PRO': { sine: 150e6, square: 60e6, ramp: 5e6, pulse: 50e6, arb: 50e6, harmonic: 75e6 },
    'DG922 PRO': { sine: 200e6, square: 60e6, ramp: 5e6, pulse: 50e6, arb: 50e6, harmonic: 100e6 }
};
function fail(s) { throw new Error('DG900 Pro: ' + s); }
function callback(fn) { if (typeof fn !== 'function')
    fail('callback required'); return fn; }
function num(x, name, min = -Infinity, max = Infinity) { if (typeof x !== 'number' || !Number.isFinite(x) || x < min || x > max)
    fail(name + ' outside [' + min + ',' + max + ']'); return String(x); }
function int(x, name, min, max) { if (!Number.isSafeInteger(x) || x < min || x > max)
    fail(name + ' outside integer range [' + min + ',' + max + ']'); return String(x); }
function flag(x) { if (typeof x !== 'boolean')
    fail('expected boolean'); return x ? 'ON' : 'OFF'; }
function enumOf(x, values, name) { const v = String(x).toUpperCase(); if (values.indexOf(v) < 0)
    fail(name + ' must be ' + values.join('/')); return v; }
function token(x) { if (typeof x !== 'string' || !x.trim() || /[\r\n\x00-\x1f;,\"]/.test(x))
    fail('unsafe SCPI token/path'); return x.trim(); }
function csvNumbers(s, n) { const a = String(s).trim().replace(/^"|"$/g, '').split(','); if (n != null && a.length !== n)
    fail('expected ' + n + ' values, got ' + a.length); return a.map(Core.numeric); }
function parseWave(s) { const a = String(s).trim().replace(/^"|"$/g, '').split(','); if (a.length !== 5)
    fail('invalid waveform reply ' + s); const val = x => x.trim().toUpperCase() === 'DEF' ? null : Core.numeric(x); return { shape: a[0].trim().toUpperCase(), frequency: val(a[1]), amplitude: val(a[2]), offset: val(a[3]), phase: val(a[4]), amplitudeUnit: 'Vpp', raw: String(s).trim(), receivedAt: Date.now() }; }
function parseCounter(s) { const a = csvNumbers(s, 5); return { frequency: a[0], period: a[1], duty: a[2], positiveWidth: a[3], negativeWidth: a[4], receivedAt: Date.now(), units: { frequency: 'Hz', period: 's', duty: '%', positiveWidth: 's', negativeWidth: 's' } }; }
function parseStatistics(s) { const a = csvNumbers(s, 6); return { current: a[0], max: a[1], min: a[2], mean: a[3], stdDev: a[4], count: a[5] }; }
function parseOutputLoad(s) { const v = Core.numeric(s); return v > 1e30 ? 'highZ' : v; }
const SHAPES = { sine: 'SIN', square: 'SQU', ramp: 'RAMP', pulse: 'PULS', noise: 'NOIS', dc: 'DC', arb: 'ARB' };
const APPL = { sine: 'SIN', square: 'SQU', ramp: 'RAMP', pulse: 'PULS', noise: 'NOIS', dc: 'DC', arb: 'ARB' };
function waveformName(x) { const shape = String(x || '').toLowerCase(); if (!Object.prototype.hasOwnProperty.call(SHAPES, shape))
    fail('unknown waveform ' + x); return shape; }
class DGChannel {
    constructor(parent, n) {
        this.generator = parent;
        this.number = n;
        this.name = 'CH' + n;
        this._s = ':SOUR' + n;
        this._o = ':OUTP' + n;
        const scalar = (path, lo, hi) => ({ set: (v, cb) => this._set(path, num(v, path, lo, hi), cb), get: cb => this._get(path, Core.numeric, cb) });
        this.frequency = scalar('FREQ', 1e-6, this.generator._model().sine);
        this.amplitude = scalar('VOLT', 0.001, 20);
        this.offset = scalar('VOLT:OFFS', -10, 10);
        this.phase = scalar('PHAS', -360, 360);
        this.square = { duty: scalar('FUNC:SQU:DCYC', 0.01, 99.99) };
        this.pulse = { duty: scalar('FUNC:PULS:DCYC', 0.01, 99.99), width: scalar('FUNC:PULS:WIDT', 9e-9, 999900),
            leading: scalar('FUNC:PULS:TRAN:LEAD', 0, 1), trailing: scalar('FUNC:PULS:TRAN:TRAIL', 0, 1) };
        this.ramp = { symmetry: scalar('FUNC:RAMP:SYMM', 0.1, 99.9) };
        this.sweep = { configure: (o, cb) => this._sweep(o, cb), start: cb => this._set('SWE:STAT', 'ON', cb), stop: cb => this._set('SWE:STAT', 'OFF', cb),
            enabled: cb => this._get('SWE:STAT', Core.logical, cb), setSteps: (n, cb) => this._set('SWE:STEP', int(n, 'sweep steps', 2, 1024), cb),
            setTime: (seconds, cb) => this._set('SWE:TIME', num(seconds, 'sweep seconds', .001, 250000), cb) };
        this.burst = { configure: (o, cb) => this._burst(o, cb), start: cb => this._set('BURS:STAT', 'ON', cb), stop: cb => this._set('BURS:STAT', 'OFF', cb), enabled: cb => this._get('BURS:STAT', Core.logical, cb) };
        this.trigger = { setSource: (source, cb) => this.generator._write(':TRIG' + n + ':SOUR ' + enumOf(source, ['IMM', 'EXT', 'BUS', 'TIM'], 'trigger source'), cb),
            source: cb => this.generator._query(':TRIG' + n + ':SOUR?', s => s.trim().toUpperCase(), cb),
            fire: cb => this.generator._write(':TRIG' + n + ':IMM', cb),
            arm: cb => this.generator._write(':INIT' + n + ':IMM', cb),
            setContinuous: (yes, cb) => this.generator._write(':INIT' + n + ':CONT ' + flag(yes), cb),
            setCount: (n, cb) => this.generator._write(':TRIG' + this.number + ':COUN ' + int(n, 'trigger count', 1, 1000000), cb),
            setDelay: (s, cb) => this.generator._write(':TRIG' + this.number + ':DEL ' + num(s, 'trigger delay s', 0, 20), cb) };
        this.sync = { enable: cb => this.generator._write(this._o + ':SYNC ON', cb), disable: cb => this.generator._write(this._o + ':SYNC OFF', cb),
            state: cb => this.generator._query(this._o + ':SYNC?', Core.logical, cb),
            setPolarity: (p, cb) => this.generator._write(this._o + ':SYNC:POL ' + enumOf(p, ['NORM', 'INV'], 'sync polarity'), cb) };
        this.modulation = {
            am: (o, cb) => this._mod('AM', o, cb), fm: (o, cb) => this._mod('FM', o, cb), pm: (o, cb) => this._mod('PM', o, cb),
            ask: (o, cb) => this._mod('ASK', o, cb), fsk: (o, cb) => this._mod('FSK', o, cb), psk: (o, cb) => this._mod('PSK', o, cb),
            pwm: (o, cb) => this._mod('PWM', o, cb), sum: (o, cb) => this._mod('SUM', o, cb),
            disable: (mode, cb) => this._set(enumOf(mode, ['AM', 'FM', 'PM', 'ASK', 'FSK', 'PSK', 'PWM', 'SUM'], 'modulation') + ':STAT', 'OFF', cb)
        };
        this.arbitrary = { uploadCodes: (points, cb) => this._uploadCodes(points, cb),
            loadFile: (path, cb) => this.generator._write(':MMEM:LOAD:DATA ' + this.number + ',' + token(path), cb),
            selectBuiltin: (name, cb) => this._set('FUNC:ARB', token(name), cb) };
        this.harmonic = { setOrder: (order, cb) => this._set('HARM:ORD', int(order, 'harmonic order', 2, 16), cb),
            setAmplitude: (v, cb) => this._set('HARM:ORD:AMPL', num(v, 'harmonic amplitude', 0, 20), cb),
            setPhase: (v, cb) => this._set('HARM:ORD:PHAS', num(v, 'harmonic phase', -360, 360), cb),
            setType: (type, cb) => this._set('HARM:TYPE', enumOf(type, ['ORD', 'COMB'], 'harmonic type'), cb) };
        this.sequence = { enable: (on, cb) => this._set('FUNC:SEQ:STAT', flag(on), cb),
            clear: cb => this.generator._write(this._s + ':FUNC:SEQ:LIST:CLE', cb),
            apply: cb => this.generator._write(this._s + ':FUNC:SEQ:LIST:APPL', cb),
            loadArb: (path, cb) => this._set('FUNC:SEQ:ARB:LOAD', token(path), cb) };
    }
    _set(path, value, cb) { this.generator._write(this._s + ':' + path + ' ' + value, cb); }
    _get(path, parse, cb) { this.generator._query(this._s + ':' + path + '?', parse, cb); }
    setLoad(load, cb) { const value = typeof load === 'string' ? enumOf(load, ['HIGHZ', 'INFINITY', 'INF'], 'load') : int(load, 'load ohms', 1, 10000); this.generator._write(this._o + ':LOAD ' + (/^(HIGHZ|INFINITY|INF)$/.test(value) ? 'INF' : value), cb); }
    getLoad(cb) { this.generator._query(this._o + ':LOAD?', parseOutputLoad, cb); }
    getOutput(cb) { this.generator._query(this._o + '?', Core.logical, cb); }
    output(on, cb) {
        callback(cb);
        const want = Boolean(on);
        if (typeof on !== 'boolean')
            fail('output state must be boolean');
        this.generator._sequence([{ cmd: this._o + ' ' + flag(on) }, { cmd: '*OPC?', kind: 'query', parse: Core.numeric, assert: v => { if (v !== 1)
                    fail('output state change incomplete'); } },
            { cmd: this._o + '?', kind: 'query', parse: Core.logical, assert: v => { if (v !== want)
                    fail('output state readback mismatch'); } }], e => cb(e || null, !e));
    }
    enable(cb) { this.output(true, cb); }
    disable(cb) { this.output(false, cb); }
    setFrequency(hz, cb) { const max = this.generator._model().sine; this._set('FREQ', num(hz, 'frequency Hz', 1e-6, max), cb); }
    getFrequency(cb) { this._get('FREQ', Core.numeric, cb); }
    setAmplitude(vpp, cb) { this._set('VOLT', num(vpp, 'amplitude Vpp', 0.001, 20), cb); }
    getAmplitude(cb) { this._get('VOLT', Core.numeric, cb); }
    setOffset(vdc, cb) { this._set('VOLT:OFFS', num(vdc, 'offset Vdc', -10, 10), cb); }
    getOffset(cb) { this._get('VOLT:OFFS', Core.numeric, cb); }
    setPhase(deg, cb) { this._set('PHAS', num(deg, 'phase degrees', -360, 360), cb); }
    getPhase(cb) { this._get('PHAS', Core.numeric, cb); }
    getWaveform(cb) { this._get('APPL', parseWave, cb); }
    /** Preflight: REQUIRES the channel already OFF; never silently de-energizes it.
     * Load must be specified to avoid unexpectedly doubled open-circuit amplitude.
     * This configures but does not enable the connector output.
     */
    configure(o, cb) {
        callback(cb);
        if (!o || typeof o !== 'object')
            fail('configuration object required');
        const shape = waveformName(o.shape), f = shape === 'dc' || shape === 'noise' ? 'DEF' : num(o.frequency, 'frequency Hz', 1e-6, this.generator._model()[shape]);
        const amp = shape === 'dc' ? 'DEF' : num(o.amplitude, 'amplitude Vpp', .001, 20);
        const off = num(o.offset == null ? 0 : o.offset, 'offset V', -10, 10), phase = shape === 'dc' || shape === 'noise' ? 'DEF' : num(o.phase == null ? 0 : o.phase, 'phase deg', -360, 360);
        if (o.load === undefined)
            fail('configure requires explicit load (50 or "highZ")');
        const load = typeof o.load === 'string' ? enumOf(o.load, ['HIGHZ', 'INF', 'INFINITY'], 'load') : ' ' + int(o.load, 'load ohms', 1, 10000);
        const loadArg = /^(HIGHZ|INF|INFINITY)$/.test(load) ? 'INF' : load.trim();
        // Conservative amplitude cap based on model's frequency/selected load, with headroom for offset.
        const hz = shape === 'noise' || shape === 'dc' ? 1 : Number(f);
        const highZ = loadArg === 'INF', max = shape === 'noise' ? (highZ ? 4 : 2) : hz <= 50e6 ? (highZ ? 20 : 10) : hz <= 100e6 ? (highZ ? 10 : 5) : (highZ ? 4 : 2);
        if (shape !== 'dc' && Number(amp) + 2 * Math.abs(Number(off)) > max + 1e-9)
            fail('amplitude plus 2*abs(offset) exceeds ' + max + ' Vpp at selected load/frequency');
        if (shape === 'dc' && Math.abs(Number(off)) > max / 2)
            fail('DC offset exceeds selected-load limit');
        if (shape !== 'square' && o.duty !== undefined)
            fail('duty is only valid for square wave configuration');
        const duty = shape === 'square' ? Number(num(o.duty == null ? 50 : o.duty, 'square duty %', 0.01, 99.99)) : null;
        const steps = [{ cmd: this._o + '?', kind: 'query', parse: Core.logical, assert: on => { if (on)
                    fail(this.name + ' is ON; disable it before configuring'); } },
            { cmd: this._o + ':LOAD ' + loadArg }, { cmd: this._s + ':VOLT:UNIT VPP' },
            { cmd: this._s + ':APPL:' + APPL[shape] + ' ' + [f, amp, off, phase].join(',') },
            { cmd: '*OPC?', kind: 'query', parse: Core.numeric, assert: v => { if (v !== 1)
                    fail('configuration incomplete'); } }];
        let dutyAfterApplyIndex = -1, dutyFinalIndex = -1;
        if (shape === 'square') {
            // Rigol documents APPL:SQU as resetting duty to 50%, but verify rather
            // than assuming the instrument honored that behavior on this firmware.
            dutyAfterApplyIndex = steps.length;
            steps.push({ cmd: this._s + ':FUNC:SQU:DCYC?', kind: 'query', parse: Core.numeric });
            steps.push({ cmd: this._s + ':FUNC:SQU:DCYC ' + duty });
            steps.push({ cmd: '*OPC?', kind: 'query', parse: Core.numeric, assert: v => { if (v !== 1)
                    fail('square duty command incomplete'); } });
            dutyFinalIndex = steps.length;
            steps.push({ cmd: this._s + ':FUNC:SQU:DCYC?', kind: 'query', parse: Core.numeric,
                assert: value => { if (Math.abs(value - duty) > 0.11)
                    fail('square duty readback ' + value + '% does not match ' + duty + '%'); } });
        }
        steps.push({ cmd: this._s + ':APPL?', kind: 'query', parse: parseWave });
        this.generator._sequence(steps, (err, res) => {
            if (err)
                return cb(err);
            const actual = res[res.length - 1];
            if (shape === 'square') {
                actual.dutyAfterApply = res[dutyAfterApplyIndex];
                actual.duty = res[dutyFinalIndex];
            }
            cb(null, actual);
        });
    }
    sine(o, cb) { this.configure(Object.assign({}, o, { shape: 'sine' }), cb); }
    squareWave(o, cb) { this.configure(Object.assign({}, o, { shape: 'square' }), cb); }
    pulseWave(o, cb) { this.configure(Object.assign({}, o, { shape: 'pulse' }), cb); }
    rampWave(o, cb) { this.configure(Object.assign({}, o, { shape: 'ramp' }), cb); }
    dc(o, cb) { this.configure(Object.assign({}, o, { shape: 'dc' }), cb); }
    noise(o, cb) { this.configure(Object.assign({}, o, { shape: 'noise' }), cb); }
    _sweep(o, cb) {
        callback(cb);
        if (!o || typeof o !== 'object')
            fail('sweep options required');
        const start = Number(num(o.start, 'sweep start Hz', 1e-6, this.generator._model().sine)), stop = Number(num(o.stop, 'sweep stop Hz', 1e-6, this.generator._model().sine));
        if (stop <= start)
            fail('sweep stop must exceed start');
        const seconds = num(o.duration == null ? 1 : o.duration, 'sweep duration seconds', .001, 250000);
        const spacing = enumOf(o.spacing || 'LIN', ['LIN', 'LOG', 'STEP'], 'spacing');
        const src = enumOf(o.trigger || 'IMM', ['IMM', 'BUS', 'EXT', 'TIM'], 'trigger');
        const steps = [{ cmd: this._o + '?', kind: 'query', parse: Core.logical, assert: on => { if (on)
                    fail('turn channel OFF before sweep configure'); } },
            { cmd: this._s + ':FREQ:STAR ' + start }, { cmd: this._s + ':FREQ:STOP ' + stop },
            { cmd: this._s + ':SWE:SPAC ' + spacing }, { cmd: this._s + ':SWE:TIME ' + seconds },
            { cmd: ':TRIG' + this.number + ':SOUR ' + src }, { cmd: this._s + ':SWE:STAT ON' },
            { cmd: '*OPC?', kind: 'query', parse: Core.numeric, assert: v => { if (v !== 1)
                    fail('sweep not completed'); } }];
        if (spacing === 'STEP')
            steps.splice(5, 0, { cmd: this._s + ':SWE:STEP ' + int(o.steps || 10, 'sweep steps', 2, 1024) });
        this.generator._sequence(steps, e => cb(e || null, !e));
    }
    _burst(o, cb) {
        callback(cb);
        if (!o || typeof o !== 'object')
            fail('burst options required');
        const mode = enumOf(o.mode || 'TRIG', ['TRIG', 'GAT'], 'burst mode');
        const src = enumOf(o.trigger || 'BUS', ['IMM', 'BUS', 'EXT', 'TIM'], 'trigger source');
        const count = o.cycles === 'infinite' ? 'INF' : int(o.cycles == null ? 1 : o.cycles, 'burst cycles', 1, 1000000);
        const steps = [{ cmd: this._o + '?', kind: 'query', parse: Core.logical, assert: on => { if (on)
                    fail('turn channel OFF before burst configure'); } },
            { cmd: this._s + ':BURS:MODE ' + mode },
            { cmd: ':TRIG' + this.number + ':SOUR ' + src }, { cmd: this._s + ':BURS:STAT ON' },
            { cmd: '*OPC?', kind: 'query', parse: Core.numeric, assert: v => { if (v !== 1)
                    fail('burst configuration incomplete'); } }];
        if (mode === 'TRIG')
            steps.splice(2, 0, { cmd: this._s + ':BURS:NCYC ' + count });
        this.generator._sequence(steps, e => cb(e || null, !e));
    }
    _mod(mode, o, cb) {
        callback(cb);
        if (!o || typeof o !== 'object')
            fail('modulation options required');
        const numericSetting = { AM: 'DEP', FM: 'DEV', PM: 'DEV', ASK: 'AMPL', FSK: 'FREQ', PSK: 'PHAS', PWM: 'DEV', SUM: 'AMPL' };
        const key = { AM: 'depth', FM: 'deviation', PM: 'deviation', ASK: 'amplitude', FSK: 'hopFrequency', PSK: 'phase', PWM: 'deviation', SUM: 'amplitude' }[mode];
        if (o[key] === undefined)
            fail(mode + ' requires ' + key);
        const maxSetting = { AM: 120, FM: 1e9, PM: 360, ASK: 20, FSK: 200e6, PSK: 360, PWM: 1e6, SUM: 100 }[mode];
        const setting = num(o[key], key, mode === 'PM' || mode === 'PSK' ? -360 : 0, maxSetting);
        const steps = [{ cmd: this._o + '?', kind: 'query', parse: Core.logical, assert: on => { if (on)
                    fail('turn channel OFF before modulation configure'); } },
            { cmd: this._s + ':' + mode + ':' + numericSetting[mode] + ' ' + setting }];
        if (o.rate !== undefined) {
            const ratePath = mode === 'ASK' || mode === 'FSK' || mode === 'PSK' ? 'INT:RATE' : 'INT:FREQ';
            if (mode === 'PSK' || mode === 'FSK' || mode === 'ASK' || mode === 'AM' || mode === 'FM' || mode === 'PM' || mode === 'PWM' || mode === 'SUM')
                steps.push({ cmd: this._s + ':' + mode + ':' + ratePath + ' ' + num(o.rate, 'modulation rate Hz', .002, 1e6) });
        }
        if (o.source !== undefined && mode !== 'SUM')
            steps.push({ cmd: this._s + ':' + mode + ':SOUR ' + enumOf(o.source, ['INT', 'EXT'], 'modulation source') });
        steps.push({ cmd: this._s + ':' + mode + ':STAT ON' }, { cmd: '*OPC?', kind: 'query', parse: Core.numeric, assert: v => { if (v !== 1)
                fail('modulation command incomplete'); } });
        this.generator._sequence(steps, e => cb(e || null, !e));
    }
    /** ASCII DAC code transfers: 32..65536 pts per call; small atomic chunks. Not binary transfer. */
    _uploadCodes(points, cb) {
        callback(cb);
        if (!Array.isArray(points) || points.length < 32 || points.length > 65536)
            fail('uploadCodes needs 32..65536 integer samples');
        for (let i = 0; i < points.length; i++)
            int(points[i], 'DAC code', -32768, 32767);
        const chunk = 2048, steps = [];
        for (let i = 0; i < points.length; i += chunk) {
            const last = i + chunk >= points.length;
            const state = i === 0 ? (last ? 'END' : 'HEAD') : last ? 'END' : 'CONT';
            steps.push({ cmd: this._s + ':TRAC:DATA:DAC16 CODE,' + state + ',' + points.slice(i, i + chunk).join(',') });
        }
        steps.push({ cmd: '*OPC?', kind: 'query', parse: Core.numeric, assert: v => { if (v !== 1)
                fail('upload did not finish'); } });
        this.generator._sequence(steps, e => cb(e || null, !e));
    }
}
export class DG900Pro {
    constructor(host, options = {}) {
        this.host = host;
        this.port = options.port == null ? 5025 : options.port;
        this.expectedModel = options.expectedModel || null;
        this.transport = new Core.ScpiSession(host, this.port, options);
        this.info = null;
        this.ch1 = new DGChannel(this, 1);
        this.ch2 = new DGChannel(this, 2);
        this.channels = [this.ch1, this.ch2];
        this.scpi = { write: (cmd, cb) => this._write(cmd, cb), query: (cmd, cb) => this._query(cmd, s => s, cb), queryBlock: (cmd, cb) => this._queryBlock(cmd, s => s, cb), sequence: (steps, cb) => this._sequence(steps, cb) };
        this.promise = Core.promiseDriver(this);
        this.output = { allOff: cb => this._sequence([{ cmd: ':OUTP1 OFF' }, { cmd: ':OUTP2 OFF' },
                { cmd: '*OPC?', kind: 'query', parse: Core.numeric, assert: x => { if (x !== 1)
                        fail('all-off incomplete'); } },
                { cmd: ':OUTP1?', kind: 'query', parse: Core.logical, assert: x => { if (x)
                        fail('CH1 still enabled'); } },
                { cmd: ':OUTP2?', kind: 'query', parse: Core.logical, assert: x => { if (x)
                        fail('CH2 still enabled'); } }], e => callback(cb)(e || null, !e)) };
        this.system = { identity: cb => this._query('*IDN?', Core.identity, cb), options: cb => this._query('*OPT?', s => String(s).trim().toUpperCase() === 'NONE' ? [] : s.split(','), cb),
            version: cb => this._query(':SYST:VERS?', s => s, cb), reset: cb => this._write('*RST', cb),
            operationComplete: cb => this._query('*OPC?', Core.logical, cb), selfTest: cb => this._query('*TST?', Core.numeric, cb),
            beep: cb => this._write(':SYST:BEEP', cb),
            clockSource: cb => this._query(':SYST:ROSC:SOUR?', s => s.toUpperCase(), cb),
            setClockSource: (x, cb) => this._write(':SYST:ROSC:SOUR ' + enumOf(x, ['INT', 'EXT'], 'clock source'), cb) };
        this.status = { error: cb => this._query(':SYST:ERR?', Core.errorReply, cb), errors: cb => this._readErrors(cb),
            standardEvent: cb => this._query('*ESR?', Core.numeric, cb), statusByte: cb => this._query('*STB?', Core.numeric, cb),
            clear: cb => this._write('*CLS', cb) };
        this.counter = { enable: (yes, cb) => this._write(':COUN:STAT ' + flag(yes), cb),
            run: cb => this._write(':COUN:RUN:STAT RUN', cb), stop: cb => this._write(':COUN:RUN:STAT STOP', cb),
            single: cb => this._write(':COUN:SING', cb), read: cb => this._query(':COUN:MEAS?', parseCounter, cb),
            stats: cb => this._query(':COUN:AVER:ALL?', parseStatistics, cb),
            clearStats: cb => this._write(':COUN:AVER:CLE', cb),
            enableStats: (on, cb) => this._write(':COUN:AVER:STAT ' + flag(on), cb),
            setGate: (s, cb) => this._write(':COUN:GATE ' + num(s, 'counter gate s', .001, 10000), cb),
            setCoupling: (v, cb) => this._write(':COUN:COUP ' + enumOf(v, ['AC', 'DC'], 'counter coupling'), cb),
            setImpedance: (v, cb) => this._write(':COUN:IMP ' + (v === 50 ? 'FIFT' : v === 1e6 ? 'OMEG' : enumOf(v, ['FIFT', 'OMEG'], 'counter impedance')), cb) };
        this.network = { getHost: cb => this._query(':SYST:COMM:LAN:HOST?', s => s.trim(), cb),
            getIP: cb => this._query(':SYST:COMM:LAN:IPAD?', s => s.trim(), cb),
            getMdnsEnabled: cb => this._query(':LXI:MDNS:STAT?', Core.logical, cb),
            setMdnsEnabled: (yes, cb) => this._write(':LXI:MDNS:STAT ' + flag(yes), cb),
            getMdnsService: cb => this._query(':LXI:MDNS:SNAM:DES?', s => s.trim(), cb) };
        this.screen = { capturePng: cb => this._sequence([{ cmd: ':HCOP:SDUM:DATA:FORM PNG' },
                { cmd: ':HCOP:SDUM:DATA?', kind: 'block' }], (e, a) => callback(cb)(e || null, e ? undefined : { mimeType: 'image/png', bytes: a[1] })) };
        this.storage = { listFiles: cb => this._query(':MMEM:CAT:ALL?', s => s, cb),
            loadState: (path, cb) => this._write(':MMEM:LOAD:STAT ' + token(path), cb),
            saveState: (path, cb) => this._write(':MMEM:STOR:STAT ' + token(path), cb),
            saveSlot: (n, cb) => this._write('*SAV ' + int(n, 'state slot', 0, 5), cb),
            recallSlot: (n, cb) => this._write('*RCL ' + int(n, 'state slot', 0, 5), cb) };
        this.display = { setBrightness: (p, cb) => this._write(':DISP:BRIG ' + int(p, 'brightness percent', 1, 100), cb),
            setText: (s, cb) => { if (typeof s !== 'string' || s.length > 40 || /[\r\n"]/.test(s))
                fail('display message must be <=40 characters'); this._write(':DISP:TEXT "' + s + '"', cb); },
            clearText: cb => this._write(':DISP:TEXT:CLE', cb) };
    }
    _model() { const m = (this.info && this.info.model) || this.expectedModel || 'DG902 PRO'; return MODELS[m]; }
    connect(cb) {
        callback(cb);
        if (this.connected)
            return cb(null, this.info);
        this.transport.connect(err => {
            if (err)
                return cb(err);
            this.transport.query('*IDN?', (e, raw) => {
                if (e) {
                    this.close();
                    return cb(e);
                }
                try {
                    const info = Core.identity(raw);
                    const model = info.model.replace(/\s+/g, ' ').replace(/(DG9\d\d)PRO$/, '$1 PRO');
                    if (!MODELS[model] || (this.expectedModel && model !== this.expectedModel))
                        fail('unexpected model ' + info.model + ' (expected ' + (this.expectedModel || 'DG900 Pro') + ')');
                    info.model = model;
                    this.info = info;
                    cb(null, info);
                }
                catch (ex) {
                    this.close();
                    cb(ex);
                }
            });
        });
    }
    get connected() { return Boolean(this.info && this.transport.connected); }
    close() { this.info = null; this.transport.close(); }
    _write(cmd, cb) { callback(cb); if (!this.connected)
        return cb(new Error('DG900 Pro: not connected')); this.transport.write(cmd, cb); }
    _query(cmd, parser, cb) {
        callback(cb);
        if (!this.connected)
            return cb(new Error('DG900 Pro: not connected'));
        this.transport.query(cmd, (err, data) => { if (err)
            return cb(err); try {
            cb(null, parser(data));
        }
        catch (e) {
            cb(e);
        } });
    }
    _queryBlock(cmd, parser, cb) {
        callback(cb);
        if (!this.connected)
            return cb(new Error('DG900 Pro: not connected'));
        this.transport.block(cmd, (err, data) => { if (err)
            return cb(err); try {
            cb(null, parser(data));
        }
        catch (e) {
            cb(e);
        } });
    }
    _sequence(steps, cb) { callback(cb); if (!this.connected)
        return cb(new Error('DG900 Pro: not connected')); this.transport.sequence(steps, cb); }
    _readErrors(cb) {
        callback(cb);
        const got = [];
        const one = () => this.status.error((e, v) => {
            if (e)
                return cb(e);
            if (v.code === 0)
                return cb(null, got);
            got.push(v);
            if (got.length >= 20)
                return cb(new Error('DG900 Pro: instrument error queue exceeded 20 entries'));
            one();
        });
        one();
    }
    channel(n) { return n === 1 ? this.ch1 : n === 2 ? this.ch2 : fail('channel must be 1 or 2'); }
    /** Intentional output shutoff. Does not guarantee OFF if network fails. */
    shutdown(cb) { callback(cb); this.output.allOff(err => { this.close(); cb(err || null, !err); }); }
}
export class DG902Pro extends DG900Pro {
    constructor(host, opts = {}) { super(host, Object.assign({}, opts, { expectedModel: 'DG902 PRO' })); }
}
/** Factory is called inside its own module, bypassing JS-engine cross-module `new` assumptions. */
export function createDG902Pro(host, options = {}) {
    try {
        return new DG902Pro(host, options);
    }
    catch (err) {
        throw new Error('DG902Pro factory construction failed: ' + String(err && (err.stack || err.message) || err));
    }
}
/** No network or hardware I/O. Reports construction stages for the Windows Qt engine. */
export function diagnoseStartup() {
    const result = { version: VERSION, ok: false, stages: [] };
    function attempt(stage, fn) {
        try {
            fn();
            result.stages.push(stage + ': OK');
            return true;
        }
        catch (e) {
            result.stages.push(stage + ': ERROR: ' + String(e && (e.stack || e.message) || e));
            return false;
        }
    }
    if (!attempt('Core.ScpiSession', () => {
        const x = new Core.ScpiSession('127.0.0.1', 5555, {});
        if (x.host !== '127.0.0.1')
            throw new Error('incorrect session host');
    }))
        return result;
    if (!attempt('DG900Pro', () => {
        const x = new DG900Pro('127.0.0.1', { port: 5555 });
        if (x.channels.length !== 2)
            throw new Error('incorrect channel count');
    }))
        return result;
    if (!attempt('DG902Pro', () => {
        const x = new DG902Pro('127.0.0.1', { port: 5555 });
        if (x.expectedModel !== 'DG902 PRO')
            throw new Error('incorrect expected model');
    }))
        return result;
    result.ok = true;
    return result;
}
export default { DG900Pro, DG902Pro, createDG902Pro, diagnoseStartup, VERSION };
