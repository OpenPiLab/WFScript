// LEARNING TIP: Setup enables a real receiver, sendEchoTest() transmits known bytes, pollEchoTest() checks the queue, and Cleanup releases pins. Simulation cannot earn a hardware PASS.

/** UARTEchoTestSteps.mjs — project-owned Test Executive handlers.
 * C++ polls the SDK independently, while TestRunner uses its own nonblocking
 * 20 ms scheduler. No nested event loops, JS timers, or busy-wait loops.
 * This test REQUIRES DIO0 -> 1k -> DIO1. Simulation is not a PASS.
 */
let uartEchoData = [];
const uartExpected = [84, 69, 83, 84, 10];

// "TEST\n"
export function setupUartEcho() {
    uartEchoData = [];
    if (Protocol1.simulation) throw Error('Physical AD3 required; simulation cannot PASS a UART loopback');
    if (!Protocol1.stop()) throw Error(Protocol1.lastError);
    if (!Protocol1.UART.configure({
        tx: 0,
        rx: 1,
        baudRate: 115200,
        dataBits: 8,
        parity: 'none',
        stopBits: 1
    }))
        throw Error(Protocol1.UART.lastError);
    if (!Protocol1.UART.startAutoRx({
        pollIntervalMs: 10,
        bufferSize: 65536
    }))
        throw Error(Protocol1.UART.lastError);
    print('[UART Test] Setup: native receiver enabled; DIO0 -> 1k -> DIO1');
    return true;
}
export function sendEchoTest() {
    uartEchoData = [];
    // Discard previous bytes before measuring the new response.
    while (Protocol1.UART.available() > 0) {
        const old = Protocol1.UART.read(4096);
        if (!old.ok) throw Error(old.error);
    }
    const tx = Protocol1.UART.writeText('TEST\n');
    if (!tx.ok) throw Error(tx.error);
    print('[UART Test] TX TEST\\n, await exactly 5 echoed bytes');
}
export function pollEchoTest() {
    const r = Protocol1.UART.read(1024);
    // native queue, not direct SDK
    if (!r.ok) throw Error(r.error);
    if (r.overflow) throw Error('SDK UART receive overflow');
    for (const b of r.data) uartEchoData.push(b);
    if (uartEchoData.length < uartExpected.length) return undefined;
    const s = Protocol1.UART.stats();
    if (s.queueDroppedBytes > 0 || s.sdkOverflows > 0 || s.parityErrors > 0)
        throw Error('UART receive errors: ' + JSON.stringify(s));
    if (uartEchoData.length !== uartExpected.length ||
        uartEchoData.some((v, i) => v !== uartExpected[i]))
        throw Error('Unexpected UART echo: ' + JSON.stringify(uartEchoData));
    print('[UART Test] PASS: expected bytes received, ' + uartEchoData.length + ' bytes');
    return uartEchoData.length; // numeric TestRunner limits 5..5
}
export function cleanupUartEcho() {
    Protocol1.UART.clearOnData();
    Protocol1.UART.stopAutoRx();
    if (!Protocol1.stop()) throw Error('Cannot confirm digital shutdown: ' + Protocol1.lastError);
    print('[UART Test] Cleanup: SDK released, DIO outputs safe');
    return true;
}
