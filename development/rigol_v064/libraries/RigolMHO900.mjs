/** RigolMHO900.mjs — WFScript MHO900 Series scope driver v0.2.7 (verified RAW + experimental block-size trials).
 * Primary target: MHO954, four 12-bit analog channels (MHO900 family).
 * Grounding: Rigol MHO900 Programming Guide, February 2026, PGA461012-1110.
 * The attached MHO98 guide is NOT the target-series programming guide.
 * Imports one shared WFScpiCore.mjs v0.3.2 embedded alongside this driver.
 * Existing callback API retained; additive .promise facade uses QJSEngine Promise.then().
 * Always use actual scope channel impedances/probe attenuation and safety ratings.
 */
import * as Core from './WFScpiCore.mjs';
const ScpiSession = Core.ScpiSession;
const numeric = Core.numeric;
const logical = Core.logical;
const identity = Core.identity;
const errorReply = Core.errorReply;
const decode = Core.decode;
export const VERSION = '0.2.7';
function fail(s) { throw new Error('MHO900: ' + s); }
function cbcheck(cb) { if (typeof cb !== 'function')
    fail('callback required'); return cb; }
function number(v, name, min = -Infinity, max = Infinity) { if (typeof v !== 'number' || !Number.isFinite(v) || v < min || v > max)
    fail(name + ' must be a finite number from ' + min + ' to ' + max); return String(v); }
function integer(v, name, min, max) { if (!Number.isSafeInteger(v) || v < min || v > max)
    fail(name + ' must be integer ' + min + '..' + max); return String(v); }
function choice(v, allowed, name) { let t = String(v).toUpperCase(); if (!allowed.includes(t))
    fail(name + ' must be ' + allowed.join('|')); return t; }
function bool(v) { if (typeof v !== 'boolean')
    fail('expected boolean'); return v ? 'ON' : 'OFF'; }
function token(v) { if (typeof v !== 'string' || !v || /[\r\n\x00,]/.test(v))
    fail('invalid token'); return v; }
function safeText(v) { if (typeof v !== 'string' || /[\r\n\x00"]/.test(v))
    fail('invalid text'); return '"' + v + '"'; }
function channelNumber(v) { if (![1, 2, 3, 4].includes(v))
    fail('channel must be 1,2,3,4'); return v; }
function chan(v) { return 'CHANnel' + channelNumber(v); }
function parseBool(v) { return logical(v); }
function countValue(v) { let x = numeric(v); if (!Number.isSafeInteger(x) || x < 0)
    fail('invalid count reply'); return x; }
function metricName(v) { return choice(v, ['VMAX', 'VMIN', 'VPP', 'VTOP', 'VBASE', 'VAMP', 'VAVG', 'VRMS', 'OVERSHOOT', 'PRESHOOT', 'PERIOD', 'FREQUENCY', 'RTIME', 'FTIME', 'PWIDTH', 'NWIDTH', 'PDUTY', 'NDUTY', 'PVRMS', 'ACRMS', 'TVMAX', 'TVMIN', 'MAREA', 'MPAREA', 'PSLEWRATE', 'NSLEWRATE', 'PPULSES', 'NPULSES', 'PEDGES', 'NEDGES'], 'measurement metric'); }
function measurementUnit(t) { if (['FREQUENCY'].includes(t))
    return 'Hz'; if (['PERIOD', 'RTIME', 'FTIME', 'PWIDTH', 'NWIDTH', 'TVMAX', 'TVMIN'].includes(t))
    return 's'; if (['PDUTY', 'NDUTY', 'OVERSHOOT', 'PRESHOOT'].includes(t))
    return '%'; return 'V'; }
function parseMeasurement(v, metric, channel) { let value = numeric(v); return { value: Math.abs(value) >= 9e36 ? null : value, unit: measurementUnit(metric), metric, channel, valid: Math.abs(value) < 9e36, raw: String(v).trim(), timestamp: Date.now() }; }
function parsePreamble(v) {
    const a = String(v).trim().split(',');
    if (a.length !== 10)
        fail('invalid waveform preamble: 10 fields required, got ' + a.length);
    const z = a.map(numeric);
    if (![0, 1, 2].includes(z[0]) || ![0, 1, 2].includes(z[1]) || z[2] < 0 || z[4] <= 0 || z[7] === 0)
        fail('unexpected waveform preamble values');
    return { format: ['BYTE', 'WORD', 'ASCII'][z[0]], mode: ['NORMAL', 'MAX', 'RAW'][z[1]], points: z[2], averages: z[3], xIncrement: z[4], xOrigin: z[5], xReference: z[6], yIncrement: z[7], yOrigin: z[8], yReference: z[9], raw: String(v).trim() };
}
function normalizeWaveform(raw, preamble, firstIndex, format) {
    let rawSamples;
    if (format === 'BYTE')
        rawSamples = Array.from(raw);
    else if (format === 'ASCII')
        rawSamples = String(raw).trim().replace(/^,+|,+$/g, '').split(',').filter(Boolean).map(numeric);
    else if (format === 'WORD') {
        if (raw.length % 2 !== 0)
            fail('WORD binary payload must have even byte count');
        // Unsigned little-endian code interpretation. Byte order and vertical scaling
        // require physical MHO954 confirmation. Preserve original bytes in exports.
        rawSamples = new Array(raw.length / 2);
        for (let i = 0; i < rawSamples.length; i++)
            rawSamples[i] = raw[2 * i] + 256 * raw[2 * i + 1];
    }
    else
        fail('unsupported waveform transfer format ' + format);
    const voltages = format === 'ASCII' ? rawSamples.slice() : rawSamples.map(y => (y - preamble.yOrigin - preamble.yReference) * preamble.yIncrement);
    const time = rawSamples.map((_, i) => preamble.xOrigin + (firstIndex - 1 + i - preamble.xReference) * preamble.xIncrement);
    return { time, voltage: voltages, samples: rawSamples, points: rawSamples.length, firstIndex, format, mode: preamble.mode, dt: preamble.xIncrement, preamble, unit: 'V', timestamp: Date.now() };
}
function mergeResults(a, b) { return { time: a.time.concat(b.time), voltage: a.voltage.concat(b.voltage), samples: a.samples.concat(b.samples), points: a.points + b.points, firstIndex: a.firstIndex, format: a.format, mode: a.mode, dt: a.dt, preamble: a.preamble, unit: 'V', timestamp: b.timestamp }; }
function assembleWrite(cmd) { return { cmd }; }
function assembleQuery(cmd, parser) { return { cmd, kind: 'query', parse: parser || String }; }
function addOp(steps, prefix, key, value) {
    if (value === undefined)
        return;
    if (key === 'enabled')
        steps.push(assembleWrite(prefix + ':DISP ' + bool(value)));
    else if (key === 'scale')
        steps.push(assembleWrite(prefix + ':SCAL ' + number(value, 'scale', .0000001, 100000)));
    else if (key === 'offset')
        steps.push(assembleWrite(prefix + ':OFFS ' + number(value, 'offset', -1e6, 1e6)));
    else if (key === 'probe')
        steps.push(assembleWrite(prefix + ':PROB ' + number(value, 'probe', .001, 50000)));
    else if (key === 'coupling')
        steps.push(assembleWrite(prefix + ':COUP ' + choice(value, ['AC', 'DC', 'GND'], 'coupling')));
    else if (key === 'impedance')
        steps.push(assembleWrite(prefix + ':IMP ' + (value === 50 ? 'FIFT' : value === 1e6 ? 'OMEG' : choice(value, ['FIFT', 'OMEG'], 'impedance'))));
    else if (key === 'bandwidth')
        steps.push(assembleWrite(prefix + ':BWL ' + (value === false ? 'OFF' : choice(value, ['OFF', 'ON', '20M', '250M'], 'bandwidth'))));
    else
        fail('unsupported channel option: ' + key);
}
class ScopeChannel {
    constructor(scope, n) { this.scope = scope; this.number = n; this._prefix = ':CHAN' + n; }
    configure(settings, cb) {
        cbcheck(cb);
        if (!settings || typeof settings !== 'object' || Array.isArray(settings))
            fail('channel config object required');
        const steps = [];
        for (const key of ['impedance', 'probe', 'coupling', 'bandwidth', 'scale', 'offset', 'enabled'])
            if (Object.prototype.hasOwnProperty.call(settings, key))
                addOp(steps, this._prefix, key, settings[key]);
        if (!steps.length)
            fail('empty channel config');
        steps.push(assembleQuery(this._prefix + ':SCAL?', numeric));
        steps.push(assembleQuery(this._prefix + ':DISP?', parseBool));
        this.scope._sequence(steps, (e, r) => cb(e || null, e ? undefined : { channel: this.number, scale: r[r.length - 2], enabled: r[r.length - 1] }));
    }
    setScale(v, cb) { this.scope._write(this._prefix + ':SCAL ' + number(v, 'V/div', 1e-7, 1e5), cb); }
    getScale(cb) { this.scope._query(this._prefix + ':SCAL?', numeric, cb); }
    setOffset(v, cb) { this.scope._write(this._prefix + ':OFFS ' + number(v, 'offset', -1e6, 1e6), cb); }
    getOffset(cb) { this.scope._query(this._prefix + ':OFFS?', numeric, cb); }
    setCoupling(v, cb) { this.scope._write(this._prefix + ':COUP ' + choice(v, ['AC', 'DC', 'GND'], 'coupling'), cb); }
    getCoupling(cb) { this.scope._query(this._prefix + ':COUP?', s => String(s).trim(), cb); }
    setProbe(v, cb) { this.scope._write(this._prefix + ':PROB ' + number(v, 'probe', .001, 50000), cb); }
    getProbe(cb) { this.scope._query(this._prefix + ':PROB?', numeric, cb); }
    setImpedance(v, cb) { this.scope._write(this._prefix + ':IMP ' + (v === 50 ? 'FIFT' : v === 1e6 ? 'OMEG' : choice(v, ['FIFT', 'OMEG'], 'impedance')), cb); }
    getImpedance(cb) { this.scope._query(this._prefix + ':IMP?', s => String(s).trim(), cb); }
    getEnabled(cb) { this.scope._query(this._prefix + ':DISP?', parseBool, cb); }
    enable(cb) { this._setEnabled(true, cb); }
    disable(cb) { this._setEnabled(false, cb); }
    _setEnabled(wanted, cb) { cbcheck(cb); this.scope._sequence([assembleWrite(this._prefix + ':DISP ' + bool(wanted)), assembleQuery(this._prefix + ':DISP?', parseBool)], (e, r) => { if (e)
        return cb(e); if (r[1] !== wanted)
        return cb(new Error('MHO900: CH' + this.number + ' display state not confirmed')); cb(null, r[1]); }); }
    getSettings(cb) { cbcheck(cb); this.scope._sequence([assembleQuery(this._prefix + ':DISP?', parseBool), assembleQuery(this._prefix + ':SCAL?', numeric), assembleQuery(this._prefix + ':OFFS?', numeric), assembleQuery(this._prefix + ':COUP?', s => s.trim()), assembleQuery(this._prefix + ':PROB?', numeric), assembleQuery(this._prefix + ':IMP?', s => s.trim())], (e, r) => cb(e || null, e ? undefined : { channel: this.number, enabled: r[0], scale: r[1], offset: r[2], coupling: r[3], probe: r[4], impedance: r[5] })); }
    measure(metric, cb) { this.scope.measure.read(metric, this.number, cb); }
    capture(opts, cb) { this.scope.waveform.capture(this.number, opts, cb); }
}
export class MHO900 {
    constructor(host, opts = {}) {
        if (typeof host !== 'string' || !host.trim())
            fail('hostname or IP required');
        this.host = host.trim();
        this.port = opts.port == null ? 5555 : opts.port; // CANDIDATE: check actual SCPI Socket Port in scope Web Control.
        this.expectedModel = opts.expectedModel || null;
        this.transport = new ScpiSession(this.host, this.port, Object.assign({}, opts, { maxResponseBytes: opts.maxResponseBytes || 8 * 1024 * 1024 }));
        this.info = null;
        this.ch1 = new ScopeChannel(this, 1);
        this.ch2 = new ScopeChannel(this, 2);
        this.ch3 = new ScopeChannel(this, 3);
        this.ch4 = new ScopeChannel(this, 4);
        this.channels = [this.ch1, this.ch2, this.ch3, this.ch4];
        this.scpi = { write: (cmd, cb) => this._write(cmd, cb), query: (cmd, cb) => this._query(cmd, x => x, cb), block: (cmd, cb) => this._block(cmd, cb), sequence: (steps, cb) => this._sequence(steps, cb) };
        this.promise = Core.promiseDriver(this);
        this.acquisition = {
            run: cb => this._write(':RUN', cb), stop: cb => this._write(':STOP', cb), single: cb => this._write(':SING', cb), forceTrigger: cb => this._write(':TFOR', cb),
            autoset: cb => this._write(':AUT', cb), status: cb => this._query(':TRIG:STAT?', s => String(s).trim().toUpperCase(), cb),
            sampleRate: cb => this._query(':ACQ:SRAT?', numeric, cb), memoryDepth: cb => this._query(':ACQ:MDEP?', numeric, cb),
            type: cb => this._query(':ACQ:TYPE?', v => String(v).trim().toUpperCase(), cb),
            bits: cb => this._query(':ACQ:BITS?', countValue, cb),
            configure: (o, cb) => {
                cbcheck(cb);
                if (!o || typeof o !== 'object')
                    fail('acquisition settings required');
                const s = [];
                if (o.type !== undefined)
                    s.push(assembleWrite(':ACQ:TYPE ' + choice(o.type, ['NORM', 'NORMAL', 'PEAK', 'AVER', 'AVERAGES', 'HRES', 'HRESOLUTION'], 'acquisition mode')));
                if (o.bits !== undefined) {
                    let bits = Number(integer(o.bits, 'high-resolution bits', 14, 16));
                    if (bits !== 14 && bits !== 16)
                        fail('HRES bits must be 14 or 16');
                    if (!o.type || !String(o.type).toUpperCase().startsWith('HRES'))
                        fail('HRES bits require acquisition type HRES');
                    s.push(assembleWrite(':ACQ:BITS ' + bits));
                }
                if (o.averages !== undefined) {
                    let k = Number(integer(o.averages, 'averages', 2, 65536));
                    if ((k & (k - 1)) !== 0)
                        fail('averages must be a power of two');
                    s.push(assembleWrite(':ACQ:AVER ' + k));
                }
                if (o.memoryDepth !== undefined) {
                    let v = o.memoryDepth;
                    let formatted = typeof v === 'number' ? integer(v, 'memory depth', 1000, 500000000) : choice(v, ['AUTO', '1K', '10K', '100K', '1M', '10M', '25M', '50M', '100M', '125M', '200M', '250M', '500M'], 'memory depth');
                    s.push(assembleWrite(':ACQ:MDEP ' + formatted));
                }
                if (!s.length)
                    fail('empty acquisition config');
                s.push(assembleQuery(':ACQ:SRAT?', numeric));
                this._sequence(s, (e, r) => cb(e || null, e ? undefined : { sampleRate: r[r.length - 1] }));
            }
        };
        this.timebase = { configure: (o, cb) => {
                cbcheck(cb);
                if (!o || typeof o !== 'object')
                    fail('timebase object required');
                let s = [];
                if (o.mode !== undefined)
                    s.push(assembleWrite(':TIM:MODE ' + choice(o.mode, ['MAIN', 'XY', 'ROLL'], 'timebase mode')));
                if (o.scale !== undefined)
                    s.push(assembleWrite(':TIM:MAIN:SCAL ' + number(o.scale, 'seconds/div', 1e-12, 1000)));
                if (o.offset !== undefined)
                    s.push(assembleWrite(':TIM:MAIN:OFFS ' + number(o.offset, 'horizontal offset', -1e6, 1e6)));
                if (!s.length)
                    fail('empty timebase config');
                s.push(assembleQuery(':TIM:MAIN:SCAL?', numeric));
                this._sequence(s, (e, r) => cb(e || null, e ? undefined : { scale: r[r.length - 1] }));
            },
            getScale: cb => this._query(':TIM:MAIN:SCAL?', numeric, cb), getOffset: cb => this._query(':TIM:MAIN:OFFS?', numeric, cb) };
        this.trigger = { edge: (o, cb) => {
                cbcheck(cb);
                if (!o || typeof o !== 'object')
                    fail('edge settings required');
                let s = [assembleWrite(':TRIG:MODE EDGE')];
                if (o.channel !== undefined)
                    s.push(assembleWrite(':TRIG:EDGE:SOUR ' + chan(o.channel)));
                if (o.slope !== undefined)
                    s.push(assembleWrite(':TRIG:EDGE:SLOP ' + choice(o.slope, ['POS', 'POSITIVE', 'NEG', 'NEGATIVE', 'RFAL'], 'edge slope')));
                if (o.level !== undefined)
                    s.push(assembleWrite(':TRIG:EDGE:LEV ' + number(o.level, 'trigger level', -1e6, 1e6)));
                if (o.sweep !== undefined)
                    s.push(assembleWrite(':TRIG:SWE ' + choice(o.sweep, ['AUTO', 'NORM', 'NORMAL', 'SING', 'SINGLE'], 'trigger sweep')));
                s.push(assembleQuery(':TRIG:EDGE:LEV?', numeric));
                this._sequence(s, (e, r) => cb(e || null, e ? undefined : { level: r[r.length - 1] }));
            },
            getStatus: cb => this._query(':TRIG:STAT?', s => String(s).trim().toUpperCase(), cb),
            setSweep: (v, cb) => this._write(':TRIG:SWE ' + choice(v, ['AUTO', 'NORM', 'NORMAL', 'SING', 'SINGLE'], 'trigger sweep'), cb),
            force: cb => this._write(':TFOR', cb) };
        this.measure = { read: (metric, ch, cb) => { cbcheck(cb); const m = metricName(metric); this._query(':MEAS:ITEM? ' + m + ',' + chan(ch), v => parseMeasurement(v, m, ch), cb); },
            readNumber: (metric, ch, cb) => { cbcheck(cb); this.measure.read(metric, ch, (e, r) => cb(e || null, e ? undefined : r.value)); },
            readMany: (specs, cb) => { cbcheck(cb); if (!Array.isArray(specs) || !specs.length)
                fail('measure specs required'); const steps = specs.map(o => { let m = metricName(o.metric), n = channelNumber(o.channel); return assembleQuery(':MEAS:ITEM? ' + m + ',' + chan(n), v => parseMeasurement(v, m, n)); }); this._sequence(steps, (e, r) => cb(e || null, e ? undefined : r)); },
            statistics: (metric, ch, kind, cb) => { cbcheck(cb); let m = metricName(metric), t = choice(kind, ['CURR', 'CURRENT', 'MAX', 'MAXIMUM', 'MIN', 'MINIMUM', 'AVER', 'AVERAGES', 'DEV', 'DEVIATION', 'CNT'], 'statistic'); this._query(':MEAS:STAT:ITEM? ' + t + ',' + m + ',' + chan(ch), numeric, cb); },
            resetStatistics: cb => this._write(':MEAS:STAT:RES', cb) };
        this.waveform = { preamble: cb => this._query(':WAV:PRE?', parsePreamble, cb),
            capture: (n, opts, cb) => {
                if (typeof opts === 'function') {
                    cb = opts;
                    opts = {};
                }
                cbcheck(cb);
                opts = opts || {};
                const mode = choice(opts.mode || 'NORMAL', ['NORMAL', 'NORM', 'RAW', 'MAX', 'MAXIMUM'], 'waveform mode');
                const fmt = choice(opts.format || 'BYTE', ['BYTE', 'WORD', 'ASCII'], 'waveform format');
                const first = opts.start == null ? 1 : Number(integer(opts.start, 'start sample', 1, 500000000));
                let points = opts.points == null ? 1000 : Number(integer(opts.points, 'points', 1, 65536));
                if (['NORMAL', 'NORM'].includes(mode) && (first + points - 1 > 1000))
                    fail('normal waveform window limited to 1000 points');
                // RAW BYTE also uses the validated window state machine. This keeps
                // ScopeChannel.capture() and downloadRaw() from bypassing the fix.
                if (mode === 'RAW' && (fmt === 'BYTE' || fmt === 'WORD'))
                    return this._captureAllVerified({ channels: [n], mode: 'RAW', format: fmt, start: first, points: points, chunkPoints: Math.min(points, 8192), freeze: false }, (err, frame) => {
                        if (err)
                            return cb(err);
                        const ch = frame['ch' + n];
                        cb(null, {
                            time: ch.time, voltage: ch.voltage, samples: ch.samples, points: ch.points,
                            firstIndex: first, format: fmt, mode: 'RAW', dt: frame.dt, preamble: ch.preamble,
                            unit: 'V', timestamp: frame.timestamp, channel: n
                        });
                    });
                const s = [assembleWrite(':WAV:SOUR ' + chan(n)), assembleWrite(':WAV:MODE ' + mode), assembleWrite(':WAV:FORM ' + (fmt === 'ASCII' ? 'ASC' : fmt)), assembleWrite(':WAV:STAR ' + first), assembleWrite(':WAV:STOP ' + (first + points - 1)), assembleQuery(':WAV:PRE?', parsePreamble), { cmd: ':WAV:DATA?', kind: fmt === 'ASCII' ? 'query' : 'block' }];
                this._sequence(s, (e, r) => { if (e)
                    return cb(e); try {
                    const p = r[r.length - 2];
                    if (p.format !== fmt)
                        fail('instrument replied ' + p.format + ', expected ' + fmt);
                    const expectedMode = mode.startsWith('NORM') ? 'NORMAL' : mode.startsWith('MAX') ? 'MAX' : 'RAW';
                    if (p.mode !== expectedMode)
                        fail('instrument replied waveform mode ' + p.mode + ', expected ' + expectedMode);
                    let waveform = normalizeWaveform(r[r.length - 1], p, first, fmt);
                    if (waveform.points !== points)
                        fail('waveform sample count mismatch: expected ' + points + ', got ' + waveform.points);
                    waveform.channel = n;
                    cb(null, waveform);
                }
                catch (ex) {
                    cb(ex);
                } });
            },
            /** Incremental RAW waveform retrieval; each query is capped at <=65536 points. Scope must be stopped first.
             * One callback per chunk; optional onChunk(chunk) must not issue other SCPI commands on same scope.
             * Non-streaming core buffers one IEEE block at a time; no auto capture of 500 Mpoint records.
             */
            downloadRaw: (n, opts, cb) => {
                cbcheck(cb);
                opts = opts || {};
                const start = opts.start == null ? 1 : Number(integer(opts.start, 'start', 1, 500000000));
                const count = Number(integer(opts.points, 'points', 1, 200000));
                const maxChunk = opts.chunkPoints == null ? 8192 : Number(integer(opts.chunkPoints, 'chunkPoints', 1, 65536));
                let next = start;
                const end = start + count - 1;
                let saved = null, metadata = null;
                const step = () => {
                    if (next > end)
                        return cb(null, saved);
                    let pts = Math.min(maxChunk, end - next + 1);
                    this.waveform.capture(n, { mode: 'RAW', format: opts.format || 'BYTE', start: next, points: pts }, (e, r) => {
                        if (e)
                            return cb(e);
                        if (metadata && r.preamble.xIncrement !== metadata.xIncrement)
                            return cb(new Error('MHO900: time scale changed during RAW transfer'));
                        metadata = r.preamble;
                        if (typeof opts.onChunk === 'function')
                            try {
                                opts.onChunk(r);
                            }
                            catch (ex) {
                                return cb(ex);
                            }
                        if (opts.collect !== false)
                            saved = saved ? mergeResults(saved, r) : r;
                        next += pts;
                        step();
                    });
                };
                step();
            }, /** Verified RAW capture (v0.4.3), including bounded, conditional readout recovery.
             * Retrieves the SAME stopped hardware record from all channels.
             * This delegates to a staged, per-transaction driver state machine; it never arms
             * SINGLE, resets the scope, touches generator outputs, or silently restarts RUN.
             */
            captureAll: (opts, cb) => this._captureAllVerified(opts, cb),
            streamRaw: (opts, cb) => this._streamRawVerified(opts, cb)
        };
        this.screen = { capture: (cb) => { cbcheck(cb); this._sequence([assembleWrite(':SAVE:IMAG:FORM PNG'), { cmd: ':SAVE:IMAG:DATA?', kind: 'block' }], (e, r) => cb(e || null, e ? undefined : { mimeType: 'image/png', bytes: r[1], timestamp: Date.now() })); } };
        this.counter = { enable: (yes, cb) => this._write(':COUN:ENAB ' + bool(yes), cb), setSource: (n, cb) => this._write(':COUN:SOUR ' + chan(n), cb), frequency: cb => this._query(':COUN:CURR?', numeric, cb) };
        this.dvm = { enable: (yes, cb) => this._write(':DVM:ENAB ' + bool(yes), cb), setSource: (n, cb) => this._write(':DVM:SOUR ' + chan(n), cb), read: cb => this._query(':DVM:CURR?', numeric, cb) };
        this.mask = { enable: (yes, cb) => this._write(':MASK:ENAB ' + bool(yes), cb), run: cb => this._write(':MASK:OPER RUN', cb), stop: cb => this._write(':MASK:OPER STOP', cb), reset: cb => this._write(':MASK:RES', cb),
            results: cb => { cbcheck(cb); this._sequence([assembleQuery(':MASK:PASS?', countValue), assembleQuery(':MASK:FAIL?', countValue), assembleQuery(':MASK:TOT?', countValue)], (e, r) => cb(e || null, e ? undefined : { passed: r[0], failed: r[1], total: r[2] })); } };
        this.buses = [1, 2, 3, 4].map(n => ({ number: n, configure: (o, cb) => {
                cbcheck(cb);
                if (!o || typeof o !== 'object')
                    fail('bus config required');
                let p = ':BUS' + n, steps = [];
                if (o.protocol !== undefined)
                    steps.push(assembleWrite(p + ':MODE ' + choice(o.protocol, ['RS232', 'SPI', 'IIC', 'LIN', 'CAN', 'PAR', 'PARALLEL', 'IIS', 'FLEX', 'M1553'], 'bus protocol')));
                if (o.format !== undefined)
                    steps.push(assembleWrite(p + ':FORM ' + choice(o.format, ['HEX', 'ASC', 'ASCII', 'DEC', 'BIN'], 'bus format')));
                if (o.visible !== undefined)
                    steps.push(assembleWrite(p + ':DISP ' + bool(o.visible)));
                if (o.eventTable !== undefined)
                    steps.push(assembleWrite(p + ':EVEN ' + bool(o.eventTable)));
                if (!steps.length)
                    fail('empty bus config');
                steps.push(assembleQuery(p + ':MODE?', s => s.trim()));
                this._sequence(steps, (e, r) => cb(e || null, e ? undefined : { bus: n, protocol: r[r.length - 1] }));
            },
            setUart: (o, cb) => {
                cbcheck(cb);
                if (!o || typeof o !== 'object')
                    fail('UART options required');
                let p = ':BUS' + n, steps = [assembleWrite(p + ':MODE RS232')];
                if (o.tx !== undefined)
                    steps.push(assembleWrite(p + ':RS232:TX ' + (o.tx === 'OFF' ? 'OFF' : chan(o.tx))));
                if (o.rx !== undefined)
                    steps.push(assembleWrite(p + ':RS232:RX ' + (o.rx === 'OFF' ? 'OFF' : chan(o.rx))));
                if (o.baud !== undefined)
                    steps.push(assembleWrite(p + ':RS232:BAUD ' + integer(o.baud, 'baud', 1, 20000000)));
                if (o.parity !== undefined)
                    steps.push(assembleWrite(p + ':RS232:PAR ' + choice(o.parity, ['NONE', 'ODD', 'EVEN'], 'UART parity')));
                if (o.bits !== undefined)
                    steps.push(assembleWrite(p + ':RS232:DBIT ' + integer(o.bits, 'data bits', 5, 9)));
                steps.push(assembleQuery(p + ':RS232:BAUD?', numeric));
                this._sequence(steps, (e, r) => cb(e || null, e ? undefined : { baud: r[r.length - 1] }));
            },
            readEvents: (cb) => this._block(':BUS' + n + ':DATA?', cb) }));
        this.bus1 = this.buses[0];
        this.bus2 = this.buses[1];
        this.bus3 = this.buses[2];
        this.bus4 = this.buses[3];
        this.network = { getIP: cb => this._query(':LAN:IPAD?', s => s.trim(), cb), getHostname: cb => this._query(':LAN:HOST:NAME?', s => s.trim(), cb), getMdns: cb => this._query(':LAN:MDNS?', logical, cb), getSocketVisa: cb => this._query(':LAN:VISA? SOCK', s => s.trim(), cb) };
        this.status = { error: cb => this._query(':SYST:ERR?', errorReply, cb), errors: cb => this.errors(cb), opc: cb => this._query('*OPC?', s => numeric(s) === 1, cb), selfTest: cb => this._query('*TST?', numeric, cb), id: cb => this._query('*IDN?', identity, cb) };
        this.generator = {
            enable: (n, yes, cb) => this._write(':SOUR' + (n === 1 || n === 2 ? n : fail('generator channel must be 1 or 2')) + ':OUTP:STAT ' + bool(yes), cb),
            setFunction: (n, kind, cb) => this._write(':SOUR' + (n === 1 || n === 2 ? n : fail('generator channel must be 1 or 2')) + ':FUNC ' + choice(kind, ['SIN', 'SQU', 'RAMP', 'PULS', 'NOIS', 'DC', 'ARB'], 'generator function'), cb)
        };
    }
    /**
     * Verifies every RAW waveform command at the instrument (not just the TCP socket).
     * Evidence: MHO954 retained the previous 503193..505000 window after a 1M -> 10k
     * depth change, causing -200 and a repeat 1808-byte response. Normal screen mode
     * 512-byte read restored its RAW window in two physical hardware observations.
     *
     * Normal-mode recovery is intentionally limited to: (a) invalid RAW window
     * before transfer, or (b) one failed boundary transaction. Never loops.
     * Transport SCPI sequences are atomic within each phase, and Panel-level
     * operation ownership prevents another Panel action interleaving phases.
     */
    /**
     * Optimized stopped-record RAW stream, using ONE preflight for the requested
     * transfer instead of independently preparing each block.
     *
     * Guarantees: no implicit RUN/SINGLE; optional explicit freeze; bounded
     * windows and per-block START/STOP readbacks, instrument error queue checks,
     * exact IEEE block lengths, final STOP/depth/sample-rate readbacks, and one
     * conditional NORMAL 512-point stale-window recovery. Completed chunks are
     * emitted once and never retained by the driver.
     *
     * opts.onChunk(frame) is synchronous and must not issue SCPI commands.
     * It receives the same frame/chN structure used by captureAll, one block at
     * a time, so callers can consume samples without a whole-record copy.
     */
    _streamRawVerified(opts, cb) {
        cbcheck(cb);
        opts = opts || {};
        if (typeof opts !== 'object' || Array.isArray(opts))
            fail('streamRaw options object required');
        const channels = opts.channels == null ? [1] : opts.channels;
        if (!Array.isArray(channels) || !channels.length || channels.length > 4)
            fail('streamRaw requires 1..4 channels');
        channels.forEach(channelNumber);
        if (new Set(channels).size !== channels.length)
            fail('streamRaw duplicate channel');
        const format = choice(opts.format || 'BYTE', ['BYTE', 'WORD'], 'streamRaw format');
        const first = opts.start == null ? 1 : Number(integer(opts.start, 'start', 1, 500000000));
        const count = Number(integer(opts.points, 'points', 1, 1000000));
        const end = first + count - 1;
        if (end > 500000000)
            fail('streamRaw exceeds maximum record index');
        // Default remains the physically tested 100k window. Larger blocks (250k,
        // 500k, 1M) are opt-in experiments; Core native stream capacity is 8 MiB.
        // A 1M WORD reply is 2,000,000 bytes, safely below the existing cap.
        const chunkSize = opts.chunkPoints == null ? 100000 : Number(integer(opts.chunkPoints, 'chunkPoints', 1, 1000000));
        if (opts.freeze !== undefined && typeof opts.freeze !== 'boolean')
            fail('freeze must be boolean');
        if (typeof opts.onChunk !== 'function')
            fail('streamRaw requires onChunk(frame)');
        const useNative = opts.nativeProcessing === true;
        if (useNative && !this.transport.nativeTransport)
            return cb(new Error('MHO900: nativeProcessing requires Binary transport. Reconnect scope using native mode.'));
        const diag = typeof opts.onDiagnostic === 'function' ? opts.onDiagnostic : null;
        const self = this, started = Date.now();
        let depth = 0, rate = 0, recovered = false, finished = false, staleErrors = 0;
        let activeChunkSize = chunkSize, fallbackUsed = false;
        let channelIndex = 0, at = first, totalPoints = 0, totalBytes = 0, totalChunks = 0;
        let firstCalibration = null, previousEnd = null;
        let preflightMs = 0, windowMs = 0, preambleMs = 0, dataMs = 0, verifyMs = 0, decodeMs = 0, callbackMs = 0, finalMs = 0;
        function emit(e) { if (diag)
            try {
                diag(e);
            }
            catch (_ignore) { } }
        function werror(message) { const e = new Error('MHO900: ' + message); e.waveformWindow = true; throw e; }
        function statusAssert(cmd, where) {
            return { cmd: ':SYST:ERR?', kind: 'query', parse: errorReply, assert: r => {
                    if (r.code !== 0)
                        werror(where + ' ' + cmd + ' rejected: SCPI ' + r.code + ' ' + r.message);
                } };
        }
        function verifiedWrite(steps, cmd, where) { steps.push(assembleWrite(cmd)); steps.push(statusAssert(cmd, where)); }
        function readback(cmd, expected, where) {
            return { cmd, kind: 'query', parse: countValue, assert: x => {
                    if (x !== expected)
                        werror(where + ' ' + cmd + ' returned ' + x + ' instead of ' + expected);
                } };
        }
        function finish(err) {
            if (finished)
                return;
            finished = true;
            if (err) {
                emit({ phase: 'TRANSFER_FAILURE', error: String(err && err.message || err), completedPoints: totalPoints });
                return cb(err);
            }
            const result = { channels: channels.slice(), firstIndex: first, pointsPerChannel: count,
                sampleRate: rate, format: format, chunks: totalChunks, bytes: totalBytes, recoveryUsed: recovered,
                requestedChunkPoints: chunkSize, effectiveChunkPoints: activeChunkSize, fallbackUsed,
                receiveMode: useNative ? 'native' : 'legacy',
                timing: { totalMs: Date.now() - started, preflightMs, windowMs, preambleMs, dataMs, verifyMs, decodeMs, callbackMs, finalMs } };
            emit(Object.assign({ phase: 'TIMING_SUMMARY' }, result.timing, { pointsPerChannel: count, bytes: totalBytes, chunks: totalChunks, recoveryUsed: recovered,
                requestedChunkPoints: chunkSize, effectiveChunkPoints: activeChunkSize, fallbackUsed }));
            cb(null, result);
        }
        if (!this.connected)
            return finish(new Error('MHO900: not connected'));
        function drainStale(next) {
            self._query(':SYST:ERR?', errorReply, (err, r) => {
                if (err)
                    return finish(err);
                if (r.code === 0)
                    return next();
                emit({ phase: 'STALE_SCPI_ERROR', code: r.code, message: r.message });
                if (++staleErrors >= 16)
                    return finish(new Error('MHO900: too many prior SCPI errors'));
                drainStale(next);
            });
        }
        function begin() {
            const s = [];
            if (opts.freeze === true)
                s.push(assembleWrite(':STOP'));
            s.push({ cmd: ':TRIG:STAT?', kind: 'query', parse: v => String(v).trim().toUpperCase(), assert: v => {
                    if (v !== 'STOP')
                        fail('scope must be STOPPED for streamRaw; state ' + v);
                } });
            const di = s.length;
            s.push({ cmd: ':ACQ:MDEP?', kind: 'query', parse: countValue, assert: v => {
                    if (v < end)
                        fail('requested RAW index ' + end + ' exceeds actual memory depth ' + v);
                } });
            const ri = s.length;
            s.push(assembleQuery(':ACQ:SRAT?', numeric));
            channels.forEach(n => s.push({ cmd: ':CHAN' + n + ':DISP?', kind: 'query', parse: parseBool, assert: on => {
                    if (!on)
                        fail('CH' + n + ' disabled for RAW transfer');
                } }));
            verifiedWrite(s, ':WAV:SOUR ' + chan(channels[0]), 'stream preflight');
            verifiedWrite(s, ':WAV:MODE RAW', 'stream preflight');
            verifiedWrite(s, ':WAV:FORM ' + format, 'stream preflight');
            const si = s.length;
            s.push(assembleQuery(':WAV:STAR?', countValue));
            const ei = s.length;
            s.push(assembleQuery(':WAV:STOP?', countValue));
            self._sequence(s, (err, values) => {
                if (err)
                    return finish(err);
                depth = values[di];
                rate = values[ri];
                const oldStart = values[si], oldStop = values[ei];
                preflightMs = Date.now() - started;
                emit({ phase: 'STREAM_PREFLIGHT', elapsedMs: preflightMs, depth, sampleRate: rate, oldStart, oldStop, format });
                if (oldStart < 1 || oldStop < 1 || oldStart > oldStop || oldStop > depth) {
                    emit({ phase: 'WINDOW_INVALID', start: oldStart, stop: oldStop, depth, action: 'NORMAL_512_RECOVERY' });
                    return recover('invalid preflight window ' + oldStart + '..' + oldStop, () => nextChannel());
                }
                nextChannel();
            });
        }
        function recover(reason, next) {
            if (recovered)
                return finish(new Error('MHO900: RAW recovery already attempted; ' + reason));
            recovered = true;
            emit({ phase: 'RECOVERY_BEGIN', reason, procedure: 'verified NORMAL 512-point read' });
            const s = [];
            verifiedWrite(s, ':WAV:SOUR ' + chan(channels[channelIndex]), 'recovery');
            verifiedWrite(s, ':WAV:MODE NORM', 'recovery');
            verifiedWrite(s, ':WAV:FORM BYTE', 'recovery');
            verifiedWrite(s, ':WAV:STAR 1', 'recovery');
            verifiedWrite(s, ':WAV:STOP 512', 'recovery');
            s.push(readback(':WAV:STAR?', 1, 'recovery'));
            s.push(readback(':WAV:STOP?', 512, 'recovery'));
            s.push({ cmd: ':WAV:PRE?', kind: 'query', parse: parsePreamble, assert: p => {
                    if (p.mode !== 'NORMAL' || p.format !== 'BYTE' || p.points !== 512)
                        fail('invalid recovery preamble');
                } });
            s.push({ cmd: ':WAV:DATA?', kind: 'block', assert: b => { if (b.length !== 512)
                    fail('invalid NORMAL recovery byte count'); } });
            s.push(statusAssert(':WAV:DATA?', 'recovery'));
            verifiedWrite(s, ':WAV:MODE RAW', 'recovery');
            verifiedWrite(s, ':WAV:FORM ' + format, 'recovery');
            verifiedWrite(s, ':WAV:STAR 1', 'recovery');
            verifiedWrite(s, ':WAV:STOP 512', 'recovery');
            s.push(readback(':WAV:STAR?', 1, 'recovery'));
            s.push(readback(':WAV:STOP?', 512, 'recovery'));
            self._sequence(s, (err) => {
                if (err)
                    return finish(new Error('MHO900: verified RAW recovery failed: ' + err.message));
                emit({ phase: 'RECOVERY_PASS', start: 1, stop: 512, bytes: 512 });
                next();
            });
        }
        function nextChannel() {
            if (channelIndex >= channels.length)
                return finalize();
            const n = channels[channelIndex];
            at = first;
            previousEnd = null;
            firstCalibration = null;
            const s = [];
            verifiedWrite(s, ':WAV:SOUR ' + chan(n), 'stream CH' + n);
            // After NORMAL recovery the selected format is restored explicitly.
            self._sequence(s, (err) => { if (err)
                return finish(err); nextChunk(); });
        }
        function downgrade(reason) {
            // Only recoverable SCPI WINDOW errors get retried. Transport timeout,
            // malformed IEEE framing, or a socket abort never enter this function.
            // Completed chunks stay committed and are not replayed.
            if (!self.connected)
                return false;
            const target = activeChunkSize > 100000 ? 100000 : (activeChunkSize > 32768 ? 32768 : 0);
            if (!target)
                return false;
            const old = activeChunkSize;
            activeChunkSize = target;
            fallbackUsed = true;
            emit({ phase: 'BLOCK_DOWNGRADE', fromPoints: old, toPoints: target,
                channel: channels[channelIndex], nextIndex: at, reason });
            nextChunk();
            return true;
        }
        function nextChunk() {
            if (at > end) {
                channelIndex++;
                return nextChannel();
            }
            const n = channels[channelIndex], take = Math.min(activeChunkSize, end - at + 1), to = at + take - 1;
            const context = 'stream CH' + n + ' window ' + at + '..' + to;
            emit({ phase: 'WINDOW_ATTEMPT', channel: n, start: at, stop: to, points: take, requestedChunkPoints: chunkSize, activeChunkPoints: activeChunkSize });
            const stamp = { start: Date.now() }, steps = [];
            // Safe even after changing channel, memory depth, or ROI direction.
            steps.push(assembleWrite(':WAV:STAR 1'));
            steps.push(assembleWrite(':WAV:STOP ' + to));
            steps.push(assembleWrite(':WAV:STAR ' + at));
            steps.push(statusAssert('window settings', context));
            steps.push(readback(':WAV:STAR?', at, context));
            steps.push({ cmd: ':WAV:STOP?', kind: 'query', parse: v => { stamp.afterWindow = Date.now(); return countValue(v); }, assert: v => {
                    if (v !== to)
                        werror(context + ' :WAV:STOP? returned ' + v + ' instead of ' + to);
                } });
            const preIdx = steps.length;
            steps.push({ cmd: ':WAV:PRE?', kind: 'query', parse: v => {
                    stamp.afterPreamble = Date.now();
                    return parsePreamble(v);
                }, assert: p => {
                    if (p.mode !== 'RAW' || p.format !== format || p.points !== take)
                        werror(context + ' preamble mode=' + p.mode + ' format=' + p.format + ' points=' + p.points + ' expected RAW ' + format + ' ' + take);
                } });
            const dataIdx = steps.length;
            steps.push({ cmd: ':WAV:DATA?', kind: useNative ? 'nativeBlock' : 'block', parse: b => {
                    stamp.afterData = Date.now();
                    return b;
                }, assert: b => {
                    if (b.length !== take * (format === 'WORD' ? 2 : 1))
                        werror(context + ' returned ' + b.length + ' bytes expected ' + take * (format === 'WORD' ? 2 : 1));
                } });
            steps.push(statusAssert(':WAV:DATA?', context));
            self._sequence(steps, (err, reply) => {
                if (err) {
                    if (err.waveformWindow && !recovered)
                        return recover(err.message, () => {
                            // A larger-than-100k window was NOT yet proven safe on this hardware.
                            // Recover once to a canonical state, then retry only the uncommitted
                            // window using the hardware-proven 100k size. Never replay prior chunks.
                            if (activeChunkSize > 100000 && downgrade(err.message))
                                return;
                            nextChunk();
                        });
                    if (err.waveformWindow && downgrade(err.message))
                        return;
                    return finish(err);
                }
                try {
                    const afterChecks = Date.now(), p = reply[preIdx];
                    // Every chunk must share the same waveform calibration and contiguous time axis.
                    if (firstCalibration) {
                        if (Math.abs(p.xIncrement - firstCalibration.xIncrement) > Math.max(1e-15, p.xIncrement * 1e-6) ||
                            Math.abs(p.xOrigin - firstCalibration.xOrigin) > Math.max(1e-12, p.xIncrement * .01) ||
                            p.yIncrement !== firstCalibration.yIncrement || p.yOrigin !== firstCalibration.yOrigin || p.yReference !== firstCalibration.yReference)
                            fail(context + ' waveform calibration changed between chunks');
                    }
                    else
                        firstCalibration = p;
                    if (previousEnd !== null && previousEnd + 1 !== at)
                        fail(context + ' noncontiguous RAW indices');
                    const beforeDecode = Date.now();
                    const w = useNative ? { points: take, rawBuffer: reply[dataIdx], preamble: p } :
                        normalizeWaveform(reply[dataIdx], p, at, format);
                    const afterDecode = Date.now();
                    if (w.points !== take)
                        fail(context + ' decoded points differ from requested window');
                    const frame = { channels: [n], points: take, firstIndex: at, mode: 'RAW', format,
                        sampleRate: rate, dt: p.xIncrement, triggerStatus: 'STOP', timestamp: Date.now() };
                    frame['ch' + n] = useNative ?
                        { channel: n, rawBuffer: w.rawBuffer, points: take, preamble: p, unit: 'V', native: true } :
                        { channel: n, voltage: w.voltage, samples: w.samples, time: w.time, points: w.points, preamble: p, unit: 'V' };
                    const beforeCall = Date.now();
                    opts.onChunk(frame);
                    const afterCall = Date.now();
                    const times = { windowMs: (stamp.afterWindow || afterChecks) - stamp.start,
                        preambleMs: (stamp.afterPreamble || afterChecks) - (stamp.afterWindow || afterChecks),
                        dataMs: (stamp.afterData || afterChecks) - (stamp.afterPreamble || afterChecks),
                        verifyMs: afterChecks - (stamp.afterData || afterChecks),
                        decodeMs: afterDecode - beforeDecode, callbackMs: afterCall - beforeCall };
                    windowMs += times.windowMs;
                    preambleMs += times.preambleMs;
                    dataMs += times.dataMs;
                    verifyMs += times.verifyMs;
                    decodeMs += times.decodeMs;
                    callbackMs += times.callbackMs;
                    totalPoints += take;
                    totalBytes += reply[dataIdx].length;
                    totalChunks++;
                    previousEnd = to;
                    emit(Object.assign({ phase: 'CHUNK_TIMING', channel: n, start: at, points: take, bytes: reply[dataIdx].length }, times));
                    at = to + 1;
                    nextChunk();
                }
                catch (e) {
                    finish(e);
                }
            });
        }
        function finalize() {
            const before = Date.now(), s = [];
            s.push({ cmd: ':TRIG:STAT?', kind: 'query', parse: v => String(v).trim().toUpperCase(), assert: v => {
                    if (v !== 'STOP')
                        fail('scope no longer STOPPED after RAW stream');
                } });
            s.push({ cmd: ':ACQ:MDEP?', kind: 'query', parse: countValue, assert: v => {
                    if (v !== depth)
                        fail('memory depth changed during RAW stream');
                } });
            s.push({ cmd: ':ACQ:SRAT?', kind: 'query', parse: numeric, assert: v => {
                    if (Math.abs(v - rate) > Math.max(0.01, rate * 1e-8))
                        fail('sample rate changed during RAW stream');
                } });
            s.push(statusAssert('final status', 'stream'));
            self._sequence(s, (err) => {
                finalMs = Date.now() - before;
                if (err)
                    return finish(err);
                if (totalPoints !== count * channels.length)
                    return finish(new Error('MHO900: incomplete RAW stream sample count'));
                finish(null);
            });
        }
        drainStale(begin);
    }
    _captureAllVerified(opts, cb) {
        if (typeof opts === 'function') {
            cb = opts;
            opts = {};
        }
        cbcheck(cb);
        opts = opts || {};
        if (!opts || typeof opts !== 'object' || Array.isArray(opts))
            fail('captureAll options object required');
        const channels = opts.channels == null ? [1, 2, 3, 4] : opts.channels;
        if (!Array.isArray(channels) || channels.length < 1 || channels.length > 4)
            fail('captureAll channels must be nonempty');
        channels.forEach(channelNumber);
        if (new Set(channels).size !== channels.length)
            fail('captureAll channels must be unique');
        const mode = choice(opts.mode || 'RAW', ['RAW', 'NORMAL', 'NORM'], 'waveform mode');
        const format = choice(opts.format || 'BYTE', ['BYTE', 'WORD'], 'waveform binary format');
        const first = opts.start == null ? 1 : Number(integer(opts.start, 'start', 1, 500000000));
        const count = opts.points == null ? 1000 : Number(integer(opts.points, 'points', 1, 200000));
        const end = first + count - 1;
        if (mode !== 'RAW' && end > 1000)
            fail('NORMAL capture limited to 1000 points');
        if (end > 500000000)
            fail('captureAll window exceeds maximum memory');
        const chunkSize = opts.chunkPoints == null ? Math.min(count, 8192) : Number(integer(opts.chunkPoints, 'chunkPoints', 1, 65536));
        if (opts.freeze !== undefined && typeof opts.freeze !== 'boolean')
            fail('freeze must be boolean');
        const recordMode = mode === 'RAW' ? 'RAW' : 'NORMAL';
        const diagnostics = typeof opts.onDiagnostic === 'function' ? opts.onDiagnostic : null;
        function diag(event) { if (diagnostics)
            try {
                diagnostics(event);
            }
            catch (_ignore) { } }
        function werror(message) { const e = new Error('MHO900: ' + message); e.waveformWindow = true; throw e; }
        function statusAssert(cmd, context) {
            return { cmd: ':SYST:ERR?', kind: 'query', parse: errorReply, assert: r => {
                    if (r.code !== 0)
                        werror(context + ' ' + cmd + ' rejected: SCPI ' + r.code + ' ' + r.message);
                } };
        }
        function verifiedWrite(steps, cmd, context) { steps.push(assembleWrite(cmd)); steps.push(statusAssert(cmd, context)); }
        function exactReadback(cmd, expected, context) {
            return { cmd, kind: 'query', parse: countValue, assert: v => {
                    if (v !== expected)
                        werror(context + ' ' + cmd + ' returned ' + v + ' instead of ' + expected);
                } };
        }
        if (!this.connected) {
            cb(new Error('MHO900: not connected'));
            return;
        }
        const self = this;
        let depth = 0, rate = 0, recovered = false, finished = false, legacyErrors = 0;
        function finish(err, result) { if (finished)
            return; finished = true; cb(err || null, err ? undefined : result); }
        // Drain old errors *by querying*, not by destructive *CLS. Without this, a
        // previous -200 would be misattributed to the first verified write.
        function drainStale() {
            self._query(':SYST:ERR?', errorReply, (err, r) => {
                if (err)
                    return finish(err);
                if (r.code === 0)
                    return begin();
                diag({ phase: 'STALE_SCPI_ERROR', code: r.code, message: r.message });
                if (++legacyErrors >= 16)
                    return finish(new Error('MHO900: 16 pre-existing scope SCPI errors; drain remaining errors before capturing'));
                drainStale();
            });
        }
        function begin() {
            const s = [];
            if (opts.freeze !== false)
                s.push(assembleWrite(':STOP'));
            s.push({ cmd: ':TRIG:STAT?', kind: 'query', parse: v => String(v).trim().toUpperCase(), assert: v => {
                    if (v !== 'STOP')
                        fail('scope must be STOPPED for synchronized capture; state ' + v);
                } });
            const depthIdx = s.length;
            s.push({ cmd: ':ACQ:MDEP?', kind: 'query', parse: countValue, assert: v => { if (v < end)
                    fail('requested ' + end + ' samples but actual memory depth is ' + v); } });
            const rateIdx = s.length;
            s.push(assembleQuery(':ACQ:SRAT?', numeric));
            channels.forEach(n => s.push({ cmd: ':CHAN' + n + ':DISP?', kind: 'query', parse: parseBool, assert: on => {
                    if (!on)
                        fail('CH' + n + ' was disabled for this acquisition');
                } }));
            verifiedWrite(s, ':WAV:SOUR ' + chan(channels[0]), 'preflight');
            verifiedWrite(s, ':WAV:MODE ' + (recordMode === 'RAW' ? 'RAW' : 'NORM'), 'preflight');
            verifiedWrite(s, ':WAV:FORM ' + format, 'preflight');
            const si = s.length;
            s.push(assembleQuery(':WAV:STAR?', countValue));
            const ei = s.length;
            s.push(assembleQuery(':WAV:STOP?', countValue));
            self._sequence(s, (err, r) => {
                if (err)
                    return finish(err);
                depth = r[depthIdx];
                rate = r[rateIdx];
                const a = r[si], b = r[ei];
                diag({ phase: 'WINDOW_BEFORE', start: a, stop: b, depth: depth, requestedStart: first, requestedStop: end });
                const invalid = a < 1 || b < 1 || a > b || b > depth;
                if (invalid && recordMode === 'RAW') {
                    diag({ phase: 'WINDOW_INVALID', start: a, stop: b, depth: depth, action: 'NORMAL_512_RECOVERY' });
                    return recover('invalid RAW window ' + a + '..' + b + ' for depth ' + depth, () => transfer());
                }
                transfer();
            });
        }
        function recover(reason, then) {
            if (recovered)
                return finish(new Error('MHO900: RAW window recovery already attempted; ' + reason));
            recovered = true;
            diag({ phase: 'RECOVERY_BEGIN', reason: reason, procedure: 'NORMAL 512-byte verified read; return RAW' });
            const s = [];
            verifiedWrite(s, ':WAV:SOUR ' + chan(channels[0]), 'recovery');
            verifiedWrite(s, ':WAV:MODE NORM', 'recovery');
            verifiedWrite(s, ':WAV:FORM BYTE', 'recovery');
            verifiedWrite(s, ':WAV:STAR 1', 'recovery');
            verifiedWrite(s, ':WAV:STOP 512', 'recovery');
            s.push(exactReadback(':WAV:STAR?', 1, 'recovery'));
            s.push(exactReadback(':WAV:STOP?', 512, 'recovery'));
            s.push({ cmd: ':WAV:PRE?', kind: 'query', parse: parsePreamble, assert: p => {
                    if (p.mode !== 'NORMAL' || p.format !== 'BYTE' || p.points !== 512)
                        fail('NORMAL recovery preamble invalid mode=' + p.mode + ' points=' + p.points);
                } });
            s.push({ cmd: ':WAV:DATA?', kind: 'block', assert: b => {
                    if (b.length !== 512)
                        fail('NORMAL recovery transferred ' + b.length + ' bytes, expected 512');
                } });
            s.push(statusAssert(':WAV:DATA?', 'recovery'));
            verifiedWrite(s, ':WAV:MODE RAW', 'recovery');
            verifiedWrite(s, ':WAV:FORM BYTE', 'recovery');
            // Apply a safe, canonical RAW window before the first read. A mode switch
            // should not be assumed to clear stale indices; confirm it explicitly.
            verifiedWrite(s, ':WAV:STAR 1', 'recovery');
            verifiedWrite(s, ':WAV:STOP 512', 'recovery');
            s.push(exactReadback(':WAV:STAR?', 1, 'recovery'));
            s.push(exactReadback(':WAV:STOP?', 512, 'recovery'));
            self._sequence(s, (err) => {
                if (err)
                    return finish(new Error('MHO900: verified NORMAL-mode RAW-window recovery failed: ' + err.message));
                diag({ phase: 'RECOVERY_PASS', start: 1, stop: 512, bytes: 512 });
                then();
            });
        }
        function transfer() {
            const s = [], frames = [];
            for (const n of channels) {
                verifiedWrite(s, ':WAV:SOUR ' + chan(n), 'CH' + n);
                verifiedWrite(s, ':WAV:MODE ' + (recordMode === 'RAW' ? 'RAW' : 'NORM'), 'CH' + n);
                verifiedWrite(s, ':WAV:FORM ' + format, 'CH' + n);
                for (let at = first; at <= end; at += chunkSize) {
                    const take = Math.min(chunkSize, end - at + 1), to = at + take - 1, context = 'CH' + n + ' window ' + at + '..' + to;
                    // Safe order for both forward and backward window moves:
                    //   STAR 1 -> STOP newEnd -> STAR newStart.
                    // No intermediate start is above stop, even when switching channels
                    // after the preceding chunk ends at the far right of the record.
                    verifiedWrite(s, ':WAV:STAR 1', context);
                    verifiedWrite(s, ':WAV:STOP ' + to, context);
                    verifiedWrite(s, ':WAV:STAR ' + at, context);
                    s.push(exactReadback(':WAV:STAR?', at, context));
                    s.push(exactReadback(':WAV:STOP?', to, context));
                    const pIndex = s.length;
                    s.push({ cmd: ':WAV:PRE?', kind: 'query', parse: raw => {
                            const p = parsePreamble(raw);
                            diag({ phase: 'PREAMBLE', channel: n, start: at, requested: take, preamblePoints: p.points, preambleMode: p.mode, xIncrement: p.xIncrement });
                            return p;
                        }, assert: p => {
                            if (p.mode !== recordMode || p.format !== format || p.points !== take)
                                werror(context + ' :WAV:PRE? mode=' + p.mode + ' points=' + p.points + ' expected ' + recordMode + ' ' + take);
                        } });
                    const dataIndex = s.length;
                    s.push({ cmd: ':WAV:DATA?', kind: 'block', parse: raw => {
                            diag({ phase: 'DATA', channel: n, start: at, requested: take, receivedBytes: raw.length, empty: raw.length === 0 });
                            return raw;
                        }, assert: b => {
                            if (b.length !== take * (format === 'WORD' ? 2 : 1))
                                werror(context + ' :WAV:DATA? returned ' + b.length + ' bytes, expected ' + take * (format === 'WORD' ? 2 : 1));
                        } });
                    s.push(statusAssert(':WAV:DATA?', context));
                    frames.push({ channel: n, start: at, points: take, pIndex, dataIndex });
                }
            }
            const finalStatusIndex = s.length;
            s.push({ cmd: ':TRIG:STAT?', kind: 'query', parse: v => String(v).trim().toUpperCase(), assert: v => {
                    if (v !== 'STOP')
                        fail('acquisition resumed during waveform retrieval');
                } });
            self._sequence(s, (err, reply) => {
                if (err) {
                    diag({ phase: 'TRANSFER_FAILURE', error: err.message, recoverable: Boolean(err.waveformWindow) });
                    if (err.waveformWindow && !recovered && recordMode === 'RAW')
                        return recover('boundary/data verification failure ' + err.message, () => transfer());
                    return finish(err);
                }
                try {
                    const result = { channels: channels.slice(), points: count, firstIndex: first,
                        mode: recordMode, format: format, sampleRate: rate, triggerStatus: reply[finalStatusIndex],
                        acquisition: 'frozen-record', time: null, timestamp: Date.now(), recoveryUsed: recovered };
                    let baseline = null;
                    for (const f of frames) {
                        const p = reply[f.pIndex];
                        const w = normalizeWaveform(reply[f.dataIndex], p, f.start, format);
                        const key = 'ch' + f.channel;
                        if (!result[key])
                            result[key] = { channel: f.channel, voltage: [], samples: [], time: [], points: 0, preamble: p, unit: 'V' };
                        const ch = result[key];
                        if (ch.points && Math.abs(ch.time[ch.time.length - 1] + ch.preamble.xIncrement - w.time[0]) > Math.max(1e-15, 0.005 * Math.abs(ch.preamble.xIncrement)))
                            fail('CH' + f.channel + ' chunk time discontinuity');
                        ch.voltage = ch.voltage.concat(w.voltage);
                        ch.samples = ch.samples.concat(w.samples);
                        ch.time = ch.time.concat(w.time);
                        ch.points += w.points;
                    }
                    for (const n of channels) {
                        const w = result['ch' + n];
                        if (w.points !== count)
                            fail('CH' + n + ' incomplete data');
                        if (!baseline) {
                            baseline = w;
                            result.time = w.time;
                            result.dt = w.preamble.xIncrement;
                            continue;
                        }
                        const dt = baseline.preamble.xIncrement;
                        const tolerance = Math.max(1e-15, 0.005 * Math.abs(dt));
                        if (Math.abs(w.preamble.xIncrement - dt) > Math.max(1e-17, Math.abs(dt) * 1e-6) ||
                            Math.abs(w.time[0] - baseline.time[0]) > tolerance ||
                            Math.abs(w.time[w.points - 1] - baseline.time[baseline.points - 1]) > tolerance)
                            fail('CH' + n + ' time axis is not synchronized with CH' + channels[0]);
                    }
                    diag({ phase: 'CAPTURE_VERIFIED', channels: channels.slice(), points: count, format: format, recoveryUsed: recovered });
                    finish(null, result);
                }
                catch (e) {
                    finish(e);
                }
            });
        }
        drainStale();
    }
    connect(cb) { cbcheck(cb); if (this.connected)
        return cb(null, this.info); this.transport.connect(e => { if (e)
        return cb(e); this.transport.query('*IDN?', (err, raw) => { if (err) {
        this.close();
        return cb(err);
    } try {
        let info = identity(raw);
        let key = info.model.replace(/[\s_-]+/g, '').toUpperCase();
        if (!/^MHO9(34|54|84)$/.test(key))
            fail('unsupported or unexpected model ' + info.model + '; expected MHO900 family');
        if (this.expectedModel && key !== String(this.expectedModel).replace(/[\s_-]+/g, '').toUpperCase())
            fail('expected ' + this.expectedModel + ' but detected ' + info.model);
        this.info = info;
        cb(null, info);
    }
    catch (ex) {
        this.close();
        cb(ex);
    } }); }); }
    get connected() { return Boolean(this.transport.connected && this.info); }
    close() { this.info = null; this.transport.close(); }
    _write(cmd, cb) { cbcheck(cb); if (!this.connected)
        return cb(new Error('MHO900: not connected')); this.transport.write(cmd, cb); }
    _query(cmd, parser, cb) { cbcheck(cb); if (!this.connected)
        return cb(new Error('MHO900: not connected')); this.transport.query(cmd, (e, v) => { if (e)
        return cb(e); try {
        cb(null, parser(v));
    }
    catch (ex) {
        cb(ex);
    } }); }
    _block(cmd, cb) { cbcheck(cb); if (!this.connected)
        return cb(new Error('MHO900: not connected')); this.transport.block(cmd, cb); }
    _sequence(steps, cb) { cbcheck(cb); if (!this.connected)
        return cb(new Error('MHO900: not connected')); this.transport.sequence(steps, cb); }
    channel(n) { return this.channels[channelNumber(n) - 1]; }
    errors(cb) { cbcheck(cb); let errors = []; const one = () => this.status.error((e, r) => { if (e)
        return cb(e); if (r.code === 0)
        return cb(null, errors); errors.push(r); if (errors.length > 30)
        return cb(new Error('MHO900: error queue exceeded 30')); one(); }); one(); }
    /** Scope stop is a measurement state change, not shutdown of external generator/supply. */
    shutdown(cb) { cbcheck(cb); if (!this.connected) {
        this.close();
        return cb(null, true);
    } this.acquisition.stop(e => { this.close(); cb(e || null, !e); }); }
}
export class MHO954 extends MHO900 {
    constructor(host, opts = {}) { super(host, Object.assign({}, opts, { expectedModel: 'MHO954' })); }
}
export function createMHO954(host, opts) { return new MHO954(host, opts || {}); }
/** Does not access TCP or modify any scope settings. */
export function diagnoseStartup() {
    const stages = [];
    try {
        const session = new Core.ScpiSession('127.0.0.1', 5555, {});
        if (!session || session.connected)
            throw new Error('SCPI constructor unexpectedly connected');
        stages.push('ScpiSession: OK');
        const base = new MHO900('127.0.0.1', { port: 5555 });
        if (!base.ch1 || base.connected)
            throw new Error('MHO900 constructor incomplete');
        stages.push('MHO900: OK');
        const specific = new MHO954('127.0.0.1', { port: 5555 });
        if (!specific.ch4 || specific.connected)
            throw new Error('MHO954 constructor incomplete');
        stages.push('MHO954: OK');
        return { version: VERSION, ok: true, stages };
    }
    catch (err) {
        return { version: VERSION, ok: false, stages, error: String(err && err.stack || err) };
    }
}
export default { MHO900, MHO954, createMHO954, diagnoseStartup, VERSION };
