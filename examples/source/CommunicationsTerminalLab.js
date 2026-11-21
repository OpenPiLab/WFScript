// ┌─ WFScript LEARNING LAB: Communications Terminal — Serial / TCP / UDP
// │ Level: Beginner–Intermediate
// │
// │ GOAL: Reuse one native Terminal widget with TCP, UDP and optional PC Serial connections.
// │ BEFORE RUN: TCP/UDP use localhost; the Serial Page needs a COM device or virtual port
// │    pair.
// │ TRY FIRST: On TCP, click START LOCAL TCP; send text and then a Hex bytes probe.
// │ TRACE: `termTcp` sendRequested → `sendTcp(payload)` → `TCP.write()`; native socket
// │    callbacks call `traffic()`.
// │ EDIT SAFELY: Add a new INFO message or change the binary `probe` bytes; rerun localhost
// │    echo.
// │ REMEMBER: Terminal widgets display and request sends; JavaScript chooses the transport.
// │    TX acceptance is not confirmed remote delivery.
// │
// │ EVENT TIP: Select `tcpStartTerm` in Design mode → Properties → Events → Go to Handler.
// │ Dynamic UI events can use `ui.on` — do NOT duplicate a saved designer binding.
// │ FULL LAB GUIDE: examples/guides/CommunicationsTerminalLab.md (in the complete source ZIP).
// └────────────────────────────────────────────────────────────────────────────

/** WFScript v0.2-dev.7 — Communications Terminal workshop.
 * One generic communications_terminal widget per Page. Widget sendRequested
 * events deliberately route through the Panel's JavaScript connection object;
 * a terminal never opens a socket or sends data on its own.
 * TCP and UDP are fully self-contained localhost echo tests. Serial is optional.
 * Each terminal shows RX/TX as text, hex, or both, with timestamp and direction.
 * Mixed binary/text uses data(bytes), NOT the line parser: binary can contain LF.
 */
let tcpServer = null,
    tcpClient = null,
    tcpPeers = [];
let udpReceiver = null,
    udpSender = null;
let serialPort = null,
    serialName = '';
const probe = [0, 1, 2, 127, 128, 254, 255, 0];
function info(id, protocol, message) {
    ui.set(id, {
        append: {
            direction: 'INFO',
            transport: protocol,
            note: message
        }
    });
    print('[CommTerminal ' + protocol + '] ' + message);
}
function traffic(id, protocol, direction, bytes, peer) {
    ui.set(id, {
        append: {
            direction: direction,
            transport: protocol,
            data: bytes,
            peer: peer || ''
        }
    });
}
function sentBytes(value) {
    return value.mode === 'hex' ? value.bytes : value.text;
}
// In the examples we use the ORIGINAL payload bytes to show a TX preview.
// TCP/serial write() accepting bytes does not guarantee the remote read them.
function logAccepted(id, protocol, bytes, number) {
    traffic(id, protocol, 'TX', bytes.slice(0, number));
    info(id, protocol, 'Accepted ' + number + ' of ' + bytes.length + ' byte(s) for transmission');
}
/**
 * CORE LESSON — Starts the local TCP echo transport. The communications_terminal widget only
 * displays RX/TX and requests sends; it does not own the network socket.
 */
function startTcp() {
    if (tcpServer) {
        info('termTcp', 'TCP', 'Already listening');
        return;
    }
    try {
        tcpServer = TCP.createServer({
            host: '127.0.0.1',
            port: 0
        });
        tcpServer.on('error', e => info('termTcp', 'TCP', 'Server error: ' + e));
        tcpServer.on('connection', function (peer) {
            tcpPeers.push(peer);
            info('termTcp', 'TCP', 'Echo server accepted client');
            peer.on('data', function (bytes) {
                peer.write(bytes);
            });
            peer.on('error', e => info('termTcp', 'TCP', 'Peer error: ' + e));
        });
        tcpServer.on('listening', function () {
            const port = tcpServer.info().localPort;
            tcpClient = TCP.create({
                host: '127.0.0.1',
                port: port
            });
            tcpClient.on('connect', () => info('termTcp', 'TCP', 'Connected to localhost:' + port));
            tcpClient.on('data', bytes => traffic('termTcp', 'TCP', 'RX', bytes));
            tcpClient.on('error', e => info('termTcp', 'TCP', 'Client error: ' + e));
            tcpClient.on('close', () => info('termTcp', 'TCP', 'Client closed'));
            tcpClient.connect();
        });
        tcpServer.listen();
    } catch (e) {
        info('termTcp', 'TCP', String(e));
        stopTcp();
    }
}
function sendTcp(payload) {
    if (!tcpClient || !tcpClient.info().open) {
        info('termTcp', 'TCP', 'Start TCP and allow connection first');
        return;
    }
    try {
        const wire = sentBytes(payload);
        const n = tcpClient.write(wire);
        logAccepted('termTcp', 'TCP', payload.bytes, n);
    }
    catch (e) {
        info('termTcp', 'TCP', 'TX error: ' + e);
    }
}
function stopTcp() {
    for (const conn of [tcpClient].concat(tcpPeers)) if (conn) {
        try {
            conn.close();
        } catch (e) { }
    }
    if (tcpServer) try {
        tcpServer.close();
    } catch (e) { }
    tcpClient = null;
    tcpPeers = [];
    tcpServer = null;
    info('termTcp', 'TCP', 'Stopped');
}
function sendTcpProbe() {
    sendTcp({
        mode: 'hex',
        bytes: probe
    });
}

function startUdp() {
    if (udpReceiver) {
        info('termUdp', 'UDP', 'Already bound');
        return;
    }
    try {
        udpReceiver = UDP.create({
            host: '127.0.0.1',
            port: 0
        });
        udpSender = UDP.create({
            host: '127.0.0.1',
            port: 0
        });
        udpReceiver.on('error', e => info('termUdp', 'UDP', 'Receiver error: ' + e));
        udpSender.on('error', e => info('termUdp', 'UDP', 'Sender error: ' + e));
        udpReceiver.on('message', function (bytes, remote) {
            // Separate native peer receives and echoes the exact datagram.
            udpReceiver.send(bytes, remote.address, remote.port);
        });
        udpSender.on('message', function (bytes, remote) {
            traffic('termUdp', 'UDP', 'RX', bytes, remote.address + ':' + remote.port);
        });
        udpReceiver.bind();
        udpSender.bind();
        info('termUdp', 'UDP', 'Local echo bound on port ' + udpReceiver.info().localPort);
    } catch (e) {
        info('termUdp', 'UDP', String(e));
        stopUdp();
    }
}
function sendUdp(payload) {
    if (!udpReceiver || !udpSender) {
        info('termUdp', 'UDP', 'Bind two UDP sockets first');
        return;
    }
    try {
        const wire = sentBytes(payload);
        const n = udpSender.send(wire, '127.0.0.1', udpReceiver.info().localPort);
        logAccepted('termUdp', 'UDP', payload.bytes, n);
    } catch (e) {
        info('termUdp', 'UDP', 'TX error: ' + e);
    }
}
function stopUdp() {
    if (udpSender) try {
        udpSender.close();
    } catch (e) { }
    if (udpReceiver) try {
        udpReceiver.close();
    } catch (e) { }
    udpSender = null;
    udpReceiver = null;
    info('termUdp', 'UDP', 'Stopped');
}
function sendUdpProbe() {
    sendUdp({
        mode: 'hex',
        bytes: probe
    });
}

function discoverSerial() {
    const ports = Serial.ports();
    if (!ports.length) {
        info('termSerial', 'SERIAL', 'No COM ports available');
        return;
    }
    if (!serialName) serialName = ports[0].port;
    for (const p of ports) info('termSerial', 'SERIAL', p.port + ' — ' + p.description);
}
function selectSerial() {
    Tool.getText('Windows COM port', serialName || 'COM5', function (result) {
        if (!result.accepted) return;
        serialName = String(result.value).trim();
        info('termSerial', 'SERIAL', 'Selected ' + serialName + ' (not yet open)');
    });
}
function openSerial() {
    if (serialPort) {
        info('termSerial', 'SERIAL', 'Already opened');
        return;
    }
    if (!serialName) {
        discoverSerial();
        if (!serialName) return;
    }
    try {
        serialPort = Serial.create({
            port: serialName,
            baudRate: 115200
        });
        serialPort.on('open', () => info('termSerial', 'SERIAL', 'Port opened: ' + serialName));
        serialPort.on('data', bytes => traffic('termSerial', 'SERIAL', 'RX', bytes));
        serialPort.on('error', e => info('termSerial', 'SERIAL', 'Error: ' + e));
        serialPort.on('close', () => info('termSerial', 'SERIAL', 'Port closed'));
        serialPort.open();
    } catch (e) {
        info('termSerial', 'SERIAL', 'Open failed: ' + e);
        closeSerial();
    }
}
function sendSerial(payload) {
    if (!serialPort || !serialPort.info().open) {
        info('termSerial', 'SERIAL', 'Select and open a COM port first');
        return;
    }
    try {
        const wire = sentBytes(payload);
        const n = serialPort.write(wire);
        logAccepted('termSerial', 'SERIAL', payload.bytes, n);
    }
    catch (e) {
        info('termSerial', 'SERIAL', 'TX error: ' + e);
    }
}
function closeSerial() {
    if (serialPort) try {
        serialPort.close();
    } catch (e) { }
    serialPort = null;
}
function sendSerialProbe() {
    sendSerial({
        mode: 'hex',
        bytes: probe
    });
}

// UI actions use Property Editor > Events (select a control, then Go to Handler).
// Serial/TCP/UDP .on() below are transport callbacks, NOT widget events.

function onInitialize() {
    info('termTcp', 'TCP', 'Idle; press START LOCAL TCP to create the echo server and client');
    info('termUdp', 'UDP', 'Idle; press START LOCAL UDP to bind two sockets');
    info('termSerial', 'SERIAL', 'Idle; a COM device or virtual port pair is required');
}
function onShutdown() {
    stopTcp();
    stopUdp();
    closeSerial();
}
