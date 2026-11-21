/**
 * WFScpiCore.mjs v0.3.2 — shared raw-TCP SCPI session for WFScript/QJSEngine.
 *
 * One module is shared by RigolMHO900, RigolDG900Pro, RigolDM858 and RigolDP900.
 * Physical transport: WFScript TCP.create({host,port}). Not VISA, VXI-11 or HiSLIP.
 * All SCPI I/O is callback-first. Promise adapters ADD an ergonomic interface.
 * Do not use async/await, Promise.finally or object spread: unsupported by Qt 6.9.2 QV4.
 *
 * Safety: connect does NOT send *RST, *CLS, outputs ON or OFF. TCP write acceptance
 * is not proof of instrument execution. Socket close does not de-energize hardware.
 */
export const VERSION = '0.3.2';
export function asBytes(data) {
    if (typeof data === 'string')
        return Array.from(data, x => x.charCodeAt(0) & 255);
    if (Array.isArray(data))
        return data.map(x => Number(x) & 255);
    if (data && typeof data.length === 'number')
        return Array.from(data, x => Number(x) & 255);
    throw new Error('SCPI: TCP data must be a byte sequence');
}
export function decode(bytes) {
    let result = '';
    for (let i = 0; i < bytes.length; i += 2048)
        result += String.fromCharCode.apply(null, bytes.slice(i, i + 2048));
    return result;
}
export function numeric(raw) {
    const text = String(raw).trim();
    if (!/^[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?$/.test(text))
        throw new Error('SCPI: invalid numeric reply: ' + text.slice(0, 100));
    const value = Number(text);
    if (!Number.isFinite(value))
        throw new Error('SCPI: non-finite numeric reply');
    return value;
}
export function logical(raw) {
    const text = String(raw).trim().toUpperCase();
    if (['1', '+1', 'ON', 'YES'].indexOf(text) >= 0)
        return true;
    if (['0', '+0', 'OFF', 'NO'].indexOf(text) >= 0)
        return false;
    throw new Error('SCPI: invalid boolean reply: ' + text);
}
export function identity(raw) {
    const a = String(raw).trim().split(',');
    if (a.length < 4)
        throw new Error('SCPI: invalid *IDN? reply');
    return { manufacturer: a[0].trim(), model: a[1].trim().toUpperCase(),
        serial: a[2].trim(), firmware: a.slice(3).join(',').trim(), raw: String(raw).trim() };
}
export function errorReply(raw) {
    const m = /^\s*([+-]?\d+)\s*,\s*(.*)$/.exec(String(raw));
    if (!m)
        throw new Error('SCPI: invalid error reply: ' + String(raw).slice(0, 100));
    return { code: Number(m[1]), message: m[2].trim().replace(/^"|"$/g, '') };
}
export function validCommand(cmd) {
    if (typeof cmd !== 'string' || !cmd.trim() || /[\r\n\x00]/.test(cmd))
        throw new Error('SCPI: invalid command');
    return cmd;
}
function asError(err) {
    return err instanceof Error ? err : new Error('SCPI: ' + String(err && err.message || err));
}
/** A QJSEngine-friendly Promise wrapper: no async/await syntax required. */
export function fromCallback(start) {
    return new Promise(function (resolve, reject) {
        let complete = false;
        function done(err, result) {
            if (complete)
                return;
            complete = true;
            if (err)
                reject(asError(err));
            else
                resolve(result);
        }
        try {
            start(done);
        }
        catch (err) {
            done(err);
        }
    });
}
export function invoke(target, method, args) {
    if (!target || typeof target[method] !== 'function')
        return Promise.reject(new Error('SCPI Promise: method ' + String(method) + ' not available'));
    const params = Array.isArray(args) ? args.slice() : [];
    return fromCallback(function (done) {
        params.push(done);
        target[method].apply(target, params);
    });
}
/**
 * Additive high-level Promise API for any callback-based instrument driver.
 *    driver.promise.query('*IDN?').then(...).catch(...)
 *    driver.promise.call(driver.channel(1), 'getOutput').then(...)
 * Promise resolution means the driver callback completed; write() is local
 * TCP acceptance only unless the invoked driver method verifies the result.
 */
export function promiseDriver(driver) {
    const scpi = driver.scpi;
    const binary = scpi.block ? 'block' : (scpi.queryBlock ? 'queryBlock' : 'queryBinary');
    return {
        connect: function () { return invoke(driver, 'connect'); },
        write: function (cmd) { return invoke(scpi, 'write', [cmd]); },
        query: function (cmd) { return invoke(scpi, 'query', [cmd]); },
        block: function (cmd) { return invoke(scpi, binary, [cmd]); },
        sequence: function (steps) {
            return scpi.sequence ? invoke(scpi, 'sequence', [steps])
                : Promise.reject(new Error('SCPI Promise: sequence unavailable on this driver'));
        },
        call: function (target, method, args) { return invoke(target, method, args); }
    };
}
export class ScpiSession {
    constructor(host, port, opts) {
        opts = opts || {};
        if (typeof host !== 'string' || !host.trim())
            throw new Error('SCPI: host required');
        if (!Number.isInteger(port) || port < 1 || port > 65535)
            throw new Error('SCPI: TCP port must be 1..65535');
        this.host = host.trim();
        this.port = port;
        this.trace = opts.trace || null;
        this.maxResponseBytes = opts.maxResponseBytes == null ? 16 * 1024 * 1024 : opts.maxResponseBytes;
        this.maxQueue = opts.maxQueue == null ? 256 : opts.maxQueue;
        this.requestTimeoutMs = opts.requestTimeoutMs == null ? 15000 : opts.requestTimeoutMs;
        this.connectTimeoutMs = opts.connectTimeoutMs == null ? 8000 : opts.connectTimeoutMs;
        if (!Number.isSafeInteger(this.maxResponseBytes) || this.maxResponseBytes < 1024 || this.maxResponseBytes > 128 * 1024 * 1024)
            throw new Error('SCPI: maxResponseBytes must be 1024..134217728');
        if (!Number.isSafeInteger(this.maxQueue) || this.maxQueue < 1 || this.maxQueue > 4096)
            throw new Error('SCPI: maxQueue must be 1..4096');
        for (const value of [this.requestTimeoutMs, this.connectTimeoutMs])
            if (!Number.isSafeInteger(value) || value < 100 || value > 600000)
                throw new Error('SCPI: timeout must be 100..600000 ms');
        this.nativeTransport = opts.nativeTransport === true;
        // Native transport is opt-in per SCPI session. The legacy path remains byte-for-byte compatible.
        this.nativeRx = null;
        this._nativeCrDelimiter = null;
        this.connected = false;
        this.opening = null;
        this.sock = null;
        this.active = null;
        this.queue = [];
        this.rx = [];
        this.closed = true;
        this._transaction = null;
        this._generation = 0;
        this._connectTimer = null;
        this._responseTimer = null;
        this._lastError = null;
        this.promise = {
            connect: () => fromCallback(cb => this.connect(cb)),
            write: cmd => fromCallback(cb => this.write(cmd, cb)),
            query: cmd => fromCallback(cb => this.query(cmd, cb)),
            block: cmd => fromCallback(cb => this.block(cmd, cb)),
            sequence: steps => fromCallback(cb => this.sequence(steps, cb))
        };
    }
    get pending() { return this.queue.length + (this.active ? 1 : 0); }
    get state() {
        return { connected: this.connected, pending: this.pending, queued: this.queue.length,
            active: this.active ? this.active.cmd : null, host: this.host, port: this.port,
            requestTimeoutMs: this.requestTimeoutMs, connectTimeoutMs: this.connectTimeoutMs,
            lastError: this._lastError ? String(this._lastError.message) : null };
    }
    _trace(direction, message) {
        if (typeof this.trace === 'function') {
            try {
                this.trace(direction, message);
            }
            catch (e) {
                if (typeof print === 'function')
                    print('[SCPI TRACE HANDLER ERROR] ' + String(e));
            }
        }
    }
    _notify(cb, err, data) {
        if (typeof cb !== 'function')
            return;
        try {
            cb(err || null, data);
        }
        catch (e) {
            const detail = String(e && (e.stack || e.message) || e);
            this._trace('CALLBACK_ERROR', detail);
            if (typeof print === 'function')
                print('[SCPI CALLBACK ERROR] ' + detail);
        }
    }
    _clearConnectTimer() { if (this._connectTimer !== null) {
        clearTimeout(this._connectTimer);
        this._connectTimer = null;
    } }
    _clearResponseTimer() { if (this._responseTimer !== null) {
        clearTimeout(this._responseTimer);
        this._responseTimer = null;
    } }
    connect(cb) {
        if (typeof cb !== 'function')
            throw new Error('SCPI: connect callback required');
        if (this.connected) {
            this._notify(cb, null, true);
            return;
        }
        if (this.sock || this.opening) {
            this._notify(cb, new Error('SCPI: connection already in progress'));
            return;
        }
        if (typeof TCP === 'undefined' || !TCP || typeof TCP.create !== 'function') {
            this._notify(cb, new Error('SCPI: WFScript TCP.create unavailable'));
            return;
        }
        if (this.nativeTransport) {
            if (typeof Binary === 'undefined' || !Binary || typeof Binary.stream !== 'function') {
                this._notify(cb, new Error('SCPI native receive requires WFScript Binary RC1.3'));
                return;
            }
            if (this.maxResponseBytes > 8 * 1024 * 1024) {
                this._notify(cb, new Error('SCPI native stream is limited to 8 MiB'));
                return;
            }
            this.nativeRx = Binary.stream(this.maxResponseBytes);
            this._nativeCrDelimiter = Binary.fromHex('0d');
            if (!this.nativeRx || !this._nativeCrDelimiter) {
                this._notify(cb, new Error('SCPI native stream allocation failed: ' + Binary.lastError));
                return;
            }
        }
        this.closed = false;
        this.opening = cb;
        this.rx = [];
        const generation = ++this._generation;
        try {
            const socket = TCP.create({ host: this.host, port: this.port });
            this.sock = socket;
            socket.on('connect', () => {
                if (socket !== this.sock || generation !== this._generation)
                    return;
                this._clearConnectTimer();
                this.connected = true;
                const ready = this.opening;
                this.opening = null;
                this._notify(ready, null, true);
                this._pump();
            });
            if (this.nativeTransport) {
                socket.on('binary', chunk => {
                    if (socket !== this.sock || generation !== this._generation) {
                        if (chunk && chunk.release)
                            chunk.release();
                        return;
                    }
                    try {
                        this._receiveNative(chunk);
                    }
                    catch (e) {
                        this.abort(e);
                    }
                    finally {
                        if (chunk && chunk.release)
                            chunk.release();
                    }
                });
            }
            else {
                socket.on('data', data => {
                    if (socket !== this.sock || generation !== this._generation)
                        return;
                    try {
                        this._receive(asBytes(data));
                    }
                    catch (e) {
                        this.abort(e);
                    }
                });
            }
            socket.on('error', err => { if (socket === this.sock)
                this.abort(err); });
            socket.on('close', () => {
                if (socket === this.sock && !this.closed)
                    this.abort(new Error('SCPI: socket disconnected'));
            });
            this._connectTimer = setTimeout(() => {
                if (this.sock === socket && !this.connected)
                    this.abort(new Error('SCPI: connect timed out after ' + this.connectTimeoutMs + ' ms'));
            }, this.connectTimeoutMs);
            socket.connect();
        }
        catch (e) {
            this.abort(e);
        }
    }
    close() { this.abort(new Error('SCPI: session closed')); }
    abort(reason) {
        const issue = asError(reason || 'SCPI: session aborted');
        this._lastError = issue;
        const old = this.sock;
        ++this._generation;
        this.sock = null;
        this.connected = false;
        this.closed = true;
        this._clearConnectTimer();
        this._clearResponseTimer();
        this.rx = [];
        if (this.nativeRx)
            this.nativeRx.clear();
        this.nativeRx = null;
        if (this._nativeCrDelimiter)
            this._nativeCrDelimiter.release();
        this._nativeCrDelimiter = null;
        const callbacks = [];
        if (this.opening)
            callbacks.push(this.opening);
        this.opening = null;
        if (this._transaction)
            callbacks.push(this._transaction.cb);
        this._transaction = null;
        if (this.active && !this.active.isTransaction)
            callbacks.push(this.active.cb);
        this.active = null;
        for (const item of this.queue)
            callbacks.push(item.cb);
        this.queue = [];
        if (old)
            try {
                old.close();
            }
            catch (_) { }
        this._trace('ABORT', issue.message);
        callbacks.forEach(cb => this._notify(cb, issue));
    }
    write(cmd, cb) { this.command(cmd, 'write', cb); }
    query(cmd, cb) { this.command(cmd, 'query', cb); }
    block(cmd, cb) { this.command(cmd, 'block', cb); }
    nativeBlock(cmd, cb) { this.command(cmd, 'nativeBlock', cb); }
    command(cmd, kind, cb) {
        validCommand(cmd);
        if (['write', 'query', 'block', 'nativeBlock'].indexOf(kind) < 0)
            throw new Error('SCPI: invalid command kind');
        if (typeof cb !== 'function')
            throw new Error('SCPI: callback required');
        if (kind === 'nativeBlock' && !this.nativeTransport) {
            this._notify(cb, new Error('SCPI: nativeBlock requires native transport; disconnect and reconnect'));
            return;
        }
        if (!this.connected) {
            this._notify(cb, new Error('SCPI: not connected'));
            return;
        }
        if (this.queue.length >= this.maxQueue) {
            this._notify(cb, new Error('SCPI: command queue limit reached'));
            return;
        }
        this.queue.push({ cmd: cmd, kind: kind, cb: cb });
        this._pump();
    }
    /** Atomic command group; no unrelated operation interleaves between steps. */
    sequence(steps, cb) {
        if (typeof cb !== 'function')
            throw new Error('SCPI: callback required');
        if (!Array.isArray(steps) || !steps.length || steps.length > 512)
            throw new Error('SCPI: sequence must contain 1..512 steps');
        const parsed = steps.map(x => {
            if (!x || typeof x !== 'object')
                throw new Error('SCPI: invalid sequence step');
            validCommand(x.cmd);
            const kind = x.kind || 'write';
            if (['write', 'query', 'block', 'nativeBlock'].indexOf(kind) < 0)
                throw new Error('SCPI: invalid sequence kind');
            return { cmd: x.cmd, kind: kind, parse: x.parse, assert: x.assert };
        });
        if (!this.connected) {
            this._notify(cb, new Error('SCPI: not connected'));
            return;
        }
        if (this.queue.length >= this.maxQueue) {
            this._notify(cb, new Error('SCPI: command queue limit reached'));
            return;
        }
        this.queue.push({ isTransaction: true, steps: parsed, cb: cb });
        this._pump();
    }
    _pump() {
        if (this.active || !this.connected || !this.sock)
            return;
        if (this._transaction) {
            this._issueNext();
            return;
        }
        const item = this.queue.shift();
        if (!item)
            return;
        if (item.isTransaction) {
            this._transaction = Object.assign({}, item, { index: 0, results: [] });
            this._issueNext();
            return;
        }
        this._send(item);
    }
    _issueNext() {
        const tx = this._transaction;
        if (!tx)
            return;
        if (tx.index === tx.steps.length) {
            this._transaction = null;
            this._notify(tx.cb, null, tx.results);
            this._pump();
            return;
        }
        const step = tx.steps[tx.index];
        this._send({ cmd: step.cmd, kind: step.kind, isTransaction: true, cb: (err, result) => {
                if (this._transaction !== tx)
                    return;
                if (err) {
                    this._transaction = null;
                    this._notify(tx.cb, err);
                    this._pump();
                    return;
                }
                try {
                    const value = step.parse ? step.parse(result) : result;
                    if (step.assert)
                        step.assert(value);
                    tx.results.push(value);
                    ++tx.index;
                    this._issueNext();
                }
                catch (e) {
                    this._transaction = null;
                    this._notify(tx.cb, asError(e));
                    this._pump();
                }
            } });
    }
    _send(item) {
        this.active = item;
        try {
            // Prevent a delayed fragment from one command entering the next response.
            if (this.nativeTransport && this.nativeRx && this.nativeRx.pending !== 0)
                throw new Error('SCPI: unread native receive bytes before ' + item.cmd + '; reconnect required');
            this._trace('TX', item.cmd);
            if (item.kind !== 'write') {
                this._clearResponseTimer();
                this._responseTimer = setTimeout(() => {
                    if (this.active === item)
                        this.abort(new Error('SCPI: query timeout (' + this.requestTimeoutMs + ' ms): ' + item.cmd));
                }, this.requestTimeoutMs);
            }
            // The TCP socket accepted the write; remote processing must be
            // checked separately with *OPC?/readback where appropriate.
            this.sock.write(item.cmd + '\n');
            if (this.active === item && item.kind === 'write') {
                this.active = null;
                this._notify(item.cb, null, true);
                if (!item.isTransaction)
                    this._pump();
            }
            else if (this.active === item)
                this._drain();
        }
        catch (e) {
            this.abort(e);
        }
    }
    /**
     * Raw TCP is a byte stream: a chunk can contain part of a reply or multiple
     * replies. This session issues only one response-producing command at a time.
     * Consequently any non-terminator bytes arriving without an active query
     * are unsolicited/stale and must not be given to a later query.
     * CR/LF are harmless here (including a binary block's delayed terminator).
     */
    _checkIdleReceive() {
        if (this.active || !this.connected || !this.sock)
            return;
        if (!this.rx.length)
            return;
        const unexpected = this.rx.some(byte => byte !== 10 && byte !== 13);
        if (unexpected) {
            this.abort(new Error('SCPI: unexpected/stale receive bytes while no query is active; reconnect required'));
            return;
        }
        this.rx = []; // trailing CR/LF after a complete ASCII or binary reply
    }
    _receive(bytes) {
        if (this.rx.length + bytes.length > this.maxResponseBytes)
            throw new Error('SCPI: response exceeds maxResponseBytes');
        for (let i = 0; i < bytes.length; ++i)
            this.rx.push(bytes[i]);
        this._drain();
        this._checkIdleReceive();
    }
    /** Native transport: copy network chunks into native BinaryStream; no per-byte QJSValue array conversion. */
    _receiveNative(chunk) {
        if (!chunk || !this.nativeRx)
            throw new Error('SCPI: missing native receive buffer');
        if (!this.active) {
            // Only delayed CR/LF from the previous reply may arrive when idle.
            if (chunk.length > 4096 || !/^(?:0a|0d)*$/i.test(chunk.toHex(0, chunk.length)))
                throw new Error('SCPI: unexpected native bytes while idle; reconnect required');
            return;
        }
        if (!this.nativeRx.append(chunk))
            throw new Error('SCPI: native receive overflow: ' + this.nativeRx.lastError);
        this._drainNative();
    }
    _nativeBlockToArray(block) {
        // Compatibility for other scope APIs (e.g. screenshot): preserve existing
        // block callback contract, but only the verified RAW stream uses nativeBlock.
        const result = [];
        for (let at = 0; at < block.length; at += 4096) {
            const part = block.toArray(at, Math.min(4096, block.length - at));
            if (!part || part.length === undefined)
                throw new Error('SCPI: native-to-array fallback failed: ' + block.lastError);
            for (let i = 0; i < part.length; ++i)
                result.push(part[i]);
        }
        return result;
    }
    _drainNative() {
        const task = this.active, rx = this.nativeRx;
        if (!task || task.kind === 'write')
            return;
        const binary = task.kind === 'block' || task.kind === 'nativeBlock';
        let reply = binary ? rx.readIEEE() : rx.readLine();
        if (rx.lastError)
            throw new Error('SCPI native frame: ' + rx.lastError);
        if (!binary && (reply === null || reply === undefined) && this._nativeCrDelimiter) {
            // Preserve legacy CR-only SCPI responses too; CRLF is handled by readLine.
            const crLine = rx.readUntil(this._nativeCrDelimiter, false);
            if (rx.lastError)
                throw new Error('SCPI native CR frame: ' + rx.lastError);
            if (crLine) {
                try {
                    reply = crLine.toUtf8(0, crLine.length);
                }
                finally {
                    crLine.release();
                }
            }
        }
        if (reply === null || reply === undefined)
            return;
        // A complete response may be followed by CR/LF, never unsolicited reply bytes.
        // Normal optional IEEE terminators are stripped by BinaryStream.
        if (rx.pending > 0) {
            // The native IEEE parser may be waiting for LF after a trailing CR.
            // In that one case it returns null and we can safely clear the CR.
            const trailing = rx.pending === 1 && binary ? rx.readFixed(1) : null;
            if (rx.pending === 1 && binary && trailing === null && !rx.lastError)
                rx.clear();
            else {
                if (trailing && trailing.release)
                    trailing.release();
                if (binary && reply.release)
                    reply.release();
                throw new Error('SCPI: extra native response bytes after ' + task.cmd + '; reconnect required');
            }
        }
        if (!binary && String(reply).trim().charAt(0) === '#')
            throw new Error('SCPI: unexpected binary header for text query ' + task.cmd);
        this.active = null;
        this._clearResponseTimer();
        let value;
        try {
            value = task.kind === 'nativeBlock' ? reply :
                (task.kind === 'block' ? this._nativeBlockToArray(reply) : String(reply).trim());
            this._trace('RX', binary ? 'native block ' + reply.length + ' bytes' : value);
        }
        finally {
            if (task.kind === 'block' && reply && reply.release)
                reply.release();
        }
        this._notify(task.cb, null, value);
        if (!task.isTransaction)
            this._pump();
    }
    _frame() {
        while (this.rx.length && (this.rx[0] === 10 || this.rx[0] === 13))
            this.rx.shift();
        if (!this.rx.length)
            return null;
        if (this.rx[0] === 35) {
            if (this.rx.length < 2)
                return null;
            const digits = this.rx[1] - 48;
            if (digits < 1 || digits > 9)
                throw new Error('SCPI: unsupported binary block length header');
            if (this.rx.length < 2 + digits)
                return null;
            let len = 0;
            for (let i = 2; i < digits + 2; ++i) {
                const d = this.rx[i] - 48;
                if (d < 0 || d > 9)
                    throw new Error('SCPI: invalid binary block length');
                len = len * 10 + d;
            }
            const end = 2 + digits + len;
            if (end > this.maxResponseBytes)
                throw new Error('SCPI: binary block exceeds maxResponseBytes');
            if (this.rx.length < end)
                return null;
            const bytes = this.rx.slice(2 + digits, end);
            this.rx.splice(0, end);
            return { block: true, bytes: bytes };
        }
        const lf = this.rx.indexOf(10), cr = this.rx.indexOf(13);
        let pos = lf;
        if (pos < 0 || (cr >= 0 && cr < pos))
            pos = cr;
        if (pos < 0)
            return null;
        const bytes = this.rx.slice(0, pos);
        this.rx.splice(0, pos + 1);
        return { block: false, bytes: bytes };
    }
    _drain() {
        if (!this.active || this.active.kind === 'write')
            return;
        const frame = this._frame();
        if (!frame)
            return;
        const task = this.active;
        // The response is complete, but we have not yet sent another command.
        // Bytes already buffered beyond this frame cannot be a legitimate reply
        // to a future command. Allow only CR/LF (including block terminators).
        // Check BEFORE delivering the callback, which can enqueue a new query.
        if (this.rx.some(byte => byte !== 10 && byte !== 13)) {
            this.abort(new Error('SCPI: extra response bytes after ' + task.cmd +
                '; possible stale/unsolicited reply; reconnect required'));
            return;
        }
        this.rx = []; // consume delimiters; late CR/LF is handled by _checkIdleReceive()
        this.active = null;
        this._clearResponseTimer();
        if (task.kind === 'block' && !frame.block) {
            // The active operation must remain reachable by abort(), so its
            // Promise and any queued requests are rejected on protocol errors.
            this.active = task;
            this.abort(new Error('SCPI: expected IEEE definite-length binary block'));
            return;
        }
        if (task.kind === 'query' && frame.block) {
            this.active = task;
            this.abort(new Error('SCPI: unexpected binary block for text query'));
            return;
        }
        const reply = task.kind === 'block' ? frame.bytes : decode(frame.bytes).trim();
        this._trace('RX', frame.block ? 'block ' + frame.bytes.length + ' bytes' : reply);
        this._notify(task.cb, null, reply);
        if (!task.isTransaction)
            this._pump();
    }
}
export default { VERSION: VERSION, ScpiSession: ScpiSession, fromCallback: fromCallback,
    promiseDriver: promiseDriver, invoke: invoke, numeric: numeric, logical: logical,
    identity: identity, errorReply: errorReply };
