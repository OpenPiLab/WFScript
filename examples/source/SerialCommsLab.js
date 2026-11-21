// ┌─ WFScript LEARNING LAB: PC Serial — Text & Binary
// │ Level: Beginner–Intermediate
// │
// │ GOAL: Discover PC COM ports, open a port and send both text and binary data.
// │ BEFORE RUN: A real USB serial device, suitable TX/RX loopback, or virtual COM pair is
// │    required to receive echoes.
// │ TRY FIRST: Click DISCOVER PORTS.
// │ TRACE: `serOpen` → `openPort()` → `Serial.create()`/open; `Serial.on("data")` handles
// │    asynchronous RX.
// │ EDIT SAFELY: Change the transmitted text and inspect the raw byte representation.
// │ REMEMBER: PC Serial is separate from the AD3 `Protocol1.UART` digital pins.
// │
// │ EVENT TIP: Select `serDiscover` in Design mode → Properties → Events → Go to Handler.
// │ Dynamic UI events can use `ui.on` — do NOT duplicate a saved designer binding.
// │ FULL LAB GUIDE: examples/guides/SerialCommsLab.md (in the complete source ZIP).
// └────────────────────────────────────────────────────────────────────────────

/** WFScript v0.2-dev.6 — Native PC Serial Laboratory.
 * Native COM port is DISTINCT from Protocol1.UART on AD3 DIO pins.
 * A PC cannot self-echo a COM port without an external loopback wire or a
 * virtual COM pair. TX and RX pins must use compatible voltage levels.
 * No serial port is opened merely by loading/running this Panel.
 */
let serial = null,
    chosenPort = '';
function log(msg, level) {
    print('[Serial] ' + msg);
    ui.set('serEvents', {
        append: {
            level: level || 'INFO',
            source: 'PC Serial',
            message: String(msg)
        }
    });
}
function status(text) {
    ui.set('serStatus', text);
}
function discoverPorts() {
    const list = Serial.ports();
    if (!Array.isArray(list)) throw Error('Serial.ports did not return an array');
    if (!list.length) {
        status('No COM ports available');
        log('No PC serial devices found', 'WARN');
        return;
    }
    if (!chosenPort) chosenPort = list[0].port;
    status('Selected ' + chosenPort + ' | ' + list.length + ' available port(s)');
    for (const p of list) log(p.port + ' — ' + p.description + ' (' + p.manufacturer + ')');
}
function choosePort() {
    Tool.getText('Choose a PC COM port', chosenPort || 'COM5', function (r) {
        if (!r.accepted) return;
        chosenPort = String(r.value).trim();
        status('Selected ' + chosenPort + ' (not opened)');
    });
}
/**
 * CORE LESSON — Opens a selected PC COM port using the Qt SerialPort bridge. Receive data arrives
 * later through native callbacks; echo requires a connected device or loopback.
 */
function openPort() {
    if (serial) {
        log('Close the existing serial connection first', 'WARN');
        return;
    }
    if (!chosenPort) {
        discoverPorts();
        if (!chosenPort) return;
    }
    try {
        const conn = Serial.create({
            port: chosenPort,
            baudRate: 115200,
            dataBits: 8,
            parity: 'none',
            stopBits: 1
        });
        serial = conn;
        conn.on('open', function () {
            status('OPEN ' + chosenPort + ' @ 115200');
            log('Port opened', 'PASS');
        });
        conn.on('data', function (bytes) {
            log('RX raw ' + bytes.length + ' bytes: ' + bytes.join(','));
        });
        conn.on('line', function (text) {
            log('RX line: ' + JSON.stringify(text), 'PASS');
        });
        conn.on('error', function (err) {
            log('Serial error: ' + err, 'ERROR');
        });
        conn.on('close', function () {
            log('Port closed');
            status('Closed');
        });
        conn.open();
    } catch (e) {
        if (serial) {
            try {
                serial.close();
            } catch (ignored) { }
        }
        serial = null;
        log(String(e), 'ERROR');
        status('Open failed');
    }
}
function sendText() {
    if (!serial) {
        log('Open a COM port first', 'WARN');
        return;
    }
    const text = 'PING from WFScript\n';
    try {
        log('TX text: ' + JSON.stringify(text) + ' / ' + serial.write(text) + ' bytes');
    }
    catch (e) {
        log('Text TX failed: ' + e, 'ERROR');
    }
}
function sendBinary() {
    if (!serial) {
        log('Open a COM port first', 'WARN');
        return;
    }
    const bytes = [0, 1, 2, 3, 127, 128, 254, 255];
    try {
        log('TX binary: ' + bytes.join(',') + ' / ' + serial.write(bytes) + ' bytes');
    }
    catch (e) {
        log('Binary TX failed: ' + e, 'ERROR');
    }
}
function closePort() {
    if (!serial) return;
    const p = serial;
    serial = null;
    try {
        p.close();
    } catch (e) {
        log(String(e), 'ERROR');
    }
}

// UI actions use Property Editor > Events (select a control, then Go to Handler).
// Serial/TCP/UDP .on() below are transport callbacks, NOT widget events.
function onInitialize() {
    status('Serial CLOSED — choose a COM port, then OPEN');
    log('Native PC COM port lab. Requires a physical loopback or virtual COM pair for RX.');
}
function onShutdown() {
    closePort();
}
