// ┌─ WFScript LEARNING LAB: UDP Local Datagram Echo — Text & Binary
// │ Level: Beginner–Intermediate
// │
// │ GOAL: Use two localhost UDP sockets to exchange individual datagrams.
// │ BEFORE RUN: No hardware or outside network.
// │ TRY FIRST: Click BIND TWO SOCKETS.
// │ TRACE: `udpStart` → `startLocal()` → `UDP.bind()`; `on("message")` handles received
// │    datagrams.
// │ EDIT SAFELY: Change the binary payload and verify that the returned byte array is
// │    identical.
// │ REMEMBER: Unlike TCP, UDP preserves datagram boundaries but provides no delivery
// │    guarantee.
// │
// │ EVENT TIP: Select `udpStart` in Design mode → Properties → Events → Go to Handler.
// │ Dynamic UI events can use `ui.on` — do NOT duplicate a saved designer binding.
// │ FULL LAB GUIDE: examples/guides/UdpCommsLab.md (in the complete source ZIP).
// └────────────────────────────────────────────────────────────────────────────

/** WFScript v0.2-dev.6 — UDP localhost datagram echo.
 * Both sender and receiver are bound to ephemeral localhost ports.
 * Separate byte-array message and decoded UTF-8 text event examples.
 * Each UDP callback corresponds to ONE datagram. Datagram delivery not guaranteed.
 */
let rx = null,
    tx = null;
const packet = [0, 1, 2, 127, 128, 254, 255, 0];
function log(msg, level) {
    print('[UDP] ' + msg);
    ui.set('udpEvents', {
        append: {
            level: level || 'INFO',
            source: 'UDP Loopback',
            message: String(msg)
        }
    });
}
function status(s) {
    ui.set('udpStatus', s);
}
/**
 * CORE LESSON — Binds two localhost UDP endpoints. Message callbacks receive complete datagrams;
 * delivery on an actual network is not guaranteed.
 */
function startLocal() {
    if (rx) {
        log('UDP sockets already running', 'WARN');
        return;
    }
    rx = UDP.create({
        host: '127.0.0.1',
        port: 0
    });
    tx = UDP.create({
        host: '127.0.0.1',
        port: 0
    });
    rx.on('error', e => log('RX UDP error: ' + e, 'ERROR'));
    tx.on('error', e => log('TX UDP error: ' + e, 'ERROR'));
    rx.on('message', function (bytes, remote) {
        log('Receiver got ' + bytes.length + ' bytes from ' + remote.address + ':' + remote.port);
        // Echo complete datagram unchanged.
        rx.send(bytes, remote.address, remote.port);
    });
    rx.on('text', function (text) {
        if (text.startsWith('HELLO')) log('Received text: ' + JSON.stringify(text));
    });
    tx.on('message', function (bytes) {
        const same = bytes.length === packet.length && bytes.every((v, i) => v === packet[i]);
        if (same) log('Binary echo [' + bytes.join(',') + '] PASS', 'PASS');
        else log('Reply bytes: [' + bytes.join(',') + ']');
    });
    tx.on('text', function (text) {
        if (text.startsWith('HELLO')) log('Text reply: ' + JSON.stringify(text), 'PASS');
    });
    rx.bind();
    tx.bind();
    status('UDP bound, receiver port ' + rx.info().localPort + ', sender port ' + tx.info().localPort);
    log('Localhost UDP sockets ready', 'PASS');
}
function textTest() {
    if (!rx || !tx) {
        log('Start UDP first', 'WARN');
        return;
    }
    const text = 'HELLO UDP from WFScript';
    tx.send(text, '127.0.0.1', rx.info().localPort);
    log('TX text: ' + JSON.stringify(text));
}
function binaryTest() {
    if (!rx || !tx) {
        log('Start UDP first', 'WARN');
        return;
    }
    tx.send(packet, '127.0.0.1', rx.info().localPort);
    log('TX binary [' + packet.join(',') + ']');
}
function stopLocal() {
    if (tx) try {
        tx.close();
    } catch (e) { }
    if (rx) try {
        rx.close();
    } catch (e) { }
    tx = null;
    rx = null;
    status('UDP stopped');
    log('UDP sockets closed');
}

// UI actions use Property Editor > Events (select a control, then Go to Handler).
// Serial/TCP/UDP .on() below are transport callbacks, NOT widget events.
function onInitialize() {
    status('UDP stopped — press BIND TWO SOCKETS');
    log('Local test sends both text and raw bytes without external equipment.');
}
function onShutdown() {
    stopLocal();
}
