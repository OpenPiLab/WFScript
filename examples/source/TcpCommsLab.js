// ┌─ WFScript LEARNING LAB: TCP Local Client/Server — Text & Binary
// │ Level: Beginner–Intermediate
// │
// │ GOAL: Use a TCP client/server on localhost and compare text lines with binary stream
// │    data.
// │ BEFORE RUN: No hardware or external network; localhost (`127.0.0.1`) is enough.
// │ TRY FIRST: Click START SERVER.
// │ TRACE: `tcpStart` → `startLocal()` → TCP listener/client; socket `on("data")` callbacks
// │    receive bytes.
// │ EDIT SAFELY: Change the example text and observe the echoed reply.
// │ REMEMBER: TCP preserves byte order but not message boundaries. Delimiters/framing belong
// │    in the application.
// │
// │ EVENT TIP: Select `tcpStart` in Design mode → Properties → Events → Go to Handler.
// │ Dynamic UI events can use `ui.on` — do NOT duplicate a saved designer binding.
// │ FULL LAB GUIDE: examples/guides/TcpCommsLab.md (in the complete source ZIP).
// └────────────────────────────────────────────────────────────────────────────

/** WFScript v0.2-dev.6 — Local TCP client & server.
 * No extra computer, instrument, firewall rule or hardware is needed.
 * TCP is a byte stream: use on('line') for LF text and on('data') for bytes.
 * Two distinct connections prevent binary packets from contaminating a
 * line-oriented text parser. Start/stop is always explicit.
 */
let server = null,
    textClient = null,
    binaryClient = null;
let accepted = [];
const pattern = [0, 1, 2, 127, 128, 254, 255, 0];
function log(msg, level) {
    print('[TCP] ' + msg);
    ui.set('tcpEvents', {
        append: {
            level: level || 'INFO',
            source: 'TCP Loopback',
            message: String(msg)
        }
    });
}
function status(s) {
    ui.set('tcpStatus', s);
}
/**
 * CORE LESSON — Creates a localhost TCP listener and client. TCP delivers ordered bytes, not
 * message boundaries; the example separately illustrates text framing and binary data.
 */
function startLocal() {
    if (server) {
        log('Server already started', 'WARN');
        return;
    }
    server = TCP.createServer({
        host: '127.0.0.1',
        port: 0
    });
    server.on('error', e => log('Server error: ' + e, 'ERROR'));
    server.on('connection', function (peer) {
        accepted.push(peer);
        log('Server accepted client #' + accepted.length);
        peer.on('data', function (bytes) {
            // Echo exactly the received bytes; TCP can split one write into chunks.
            peer.write(bytes);
        });
        peer.on('error', e => log('Peer error: ' + e, 'ERROR'));
    });
    server.on('listening', function () {
        const port = server.info().localPort;
        status('Server listening on 127.0.0.1:' + port);
        log('Ephemeral local TCP port ' + port + ' (no network exposure)', 'PASS');
        textClient = TCP.create({
            host: '127.0.0.1',
            port: port
        });
        textClient.on('connect', () => log('Text TCP client connected', 'PASS'));
        textClient.on('line', text => log('Text echo: ' + JSON.stringify(text), 'PASS'));
        textClient.on('error', e => log('Text client error: ' + e, 'ERROR'));
        textClient.connect();
        binaryClient = TCP.create({
            host: '127.0.0.1',
            port: port
        });
        let bytesReceived = [];
        binaryClient.on('connect', () => log('Binary TCP client connected', 'PASS'));
        binaryClient.on('data', function (bytes) {
            bytesReceived = bytesReceived.concat(bytes);
            while (bytesReceived.length >= pattern.length) {
                const received = bytesReceived.splice(0, pattern.length);
                const ok = received.every((v, i) => v === pattern[i]);
                log('Binary echo: [' + received.join(',') + '] ' + (ok ? 'PASS' : 'FAIL'), ok ? 'PASS' : 'FAIL');
            }
        });
        binaryClient.on('error', e => log('Binary client error: ' + e, 'ERROR'));
        binaryClient.connect();
    });
    server.listen();
}
function textTest() {
    if (!textClient || !textClient.info().open) {
        log('Start server and allow connection first', 'WARN');
        return;
    }
    const text = 'SCPI *IDN?\n';
    log('TX text ' + JSON.stringify(text));
    textClient.write(text);
}
function binaryTest() {
    if (!binaryClient || !binaryClient.info().open) {
        log('Start server and allow connection first', 'WARN');
        return;
    }
    log('TX binary [' + pattern.join(',') + ']');
    binaryClient.write(pattern);
}
function stopLocal() {
    for (const p of [textClient, binaryClient].concat(accepted)) if (p) {
        try {
            p.close();
        } catch (e) { }
    }
    if (server) {
        try {
            server.close();
        } catch (e) { }
    }
    textClient = null;
    binaryClient = null;
    accepted = [];
    server = null;
    status('TCP stopped');
    log('All TCP connections closed');
}

// UI actions use Property Editor > Events (select a control, then Go to Handler).
// Serial/TCP/UDP .on() below are transport callbacks, NOT widget events.
function onInitialize() {
    status('TCP stopped — start local listener');
    log('Ready. Click START SERVER to create client/server on 127.0.0.1.');
}
function onShutdown() {
    stopLocal();
}
