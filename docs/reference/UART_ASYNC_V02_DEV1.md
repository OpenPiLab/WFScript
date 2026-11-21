# WFScript v0.2-dev.1 — Native UART reception, JavaScript events, and Terminal widget

**Development source candidate.** No Windows Qt compilation or physical Analog Discovery 3 validation of these **new** asynchronous features has been performed. The user has independently validated earlier v0.2-dev UART loopback for manual TX/RX; that result does not validate this new receiver.

## Design

- A `QTimer` owned by `ProtocolUartPort` runs on the **existing PanelRuntime QJSEngine thread**. It polls `FDwfDigitalUartRx` at a configurable interval; all SDK transactions on that DWF handle stay serialized on the same thread.
- New data is stored in a bounded native byte queue and broadcast as a native `dataReceived(QVariantMap)` Qt signal. `PanelRuntime` forwards these value-only chunks to all `uart_terminal` widgets, including widgets on inactive Pages, over the existing queued worker-to-GUI `controlUpdate` mechanism. Widget display **does not call JavaScript**.
- An optional `Protocol1.UART.onData(callback)` registers **one JS callback per UART session**. It sees the same received chunk, but **does not consume** the native queue. The callback executes only on the QJSEngine thread, with errors reported through existing script diagnostics. `clearOnData()` unregisters it.
- The original `Protocol1.UART.read(maxBytes)` remains available and still reads the **SDK directly** when native auto receive is disabled and its queued backlog is empty. While auto receive is enabled, `read()` drains **only WFScript's queue**, preventing two consumers from stealing bytes from the SDK. After `stopAutoRx()`, any buffered data is returned first; subsequent reads use the SDK.
- `stopAutoRx()` stops the Qt timer but does not release UART hardware. `Protocol1.stop()` releases protocol resources, invalidates the receiver, and clears its queue/callback. Runtime shutdown still attempts safe output release.
- In standalone `ScriptWorker` scripts, the worker lifetime ends when the script returns; continuous async reception is intended for persistent Panel scripts. This deliberately does **not** introduce another SDK handle or a second worker thread; therefore **a long-running blocking JS callback or SDK transaction can delay the QTimer and cause SDK receive overflow**. Timers are best-effort, not precise bit-edge timing.

## Added JavaScript surface

```javascript
if (!Protocol1.UART.configure({tx:0,rx:1,baudRate:115200,
                               dataBits:8,parity:'none',stopBits:1}))
    throw Error(Protocol1.UART.lastError);

// Optional: observe copies of chunks without consuming received bytes.
Protocol1.UART.onData(function (chunk) {
    print(chunk.text, chunk.count, chunk.timestampSource);
});

if (!Protocol1.UART.startAutoRx({pollIntervalMs:10,bufferSize:65536}))
    throw Error(Protocol1.UART.lastError);

Protocol1.UART.writeText('PING\n');
const bytesWaiting=Protocol1.UART.available();
const r=Protocol1.UART.read(4096); // reads WFScript queue in auto mode
print(r.data, r.count, r.overflow);
print(JSON.stringify(Protocol1.UART.stats()));

Protocol1.UART.clearOnData();
Protocol1.UART.stopAutoRx();
Protocol1.stop();
```

| Method / property | Behavior |
| --- | --- |
| `startAutoRx({pollIntervalMs:10,bufferSize:65536})` | Start native poller, initialize SDK RX if necessary; interval 2–1000ms; queue 1 KiB–1 MiB |
| `stopAutoRx()` | Stop timer, preserve buffered bytes and SDK UART session |
| `onData(function(chunk){...})` | Optional callback; receives a copy of the chunk with `data`, `text`, `count`, `overflow`, `parityErrorIndex`, `timestamp` and `timestampSource` |
| `clearOnData()` | Unregister callback; widget and queue continue |
| `available()` | Unread number of bytes in native queue |
| `stats()` | `autoReceiving`, `pollIntervalMs`, `bufferCapacity`, `available`, `receivedBytes`, `readBytes`, `queueDroppedBytes`, `sdkOverflows`, `parityErrors`, `simulated` |
| `autoReceiving` | Read-only state property |
| `read(maxBytes)` | **Preserved**. Direct DWF SDK read in manual mode; bounded native queue in auto mode |

`timestampSource` currently reports `HostPoll`. It is *not* a device-hardware timestamp. Multiple SDK chunks may divide UTF-8 code points or a textual line; for binary protocols use `chunk.data` instead of assuming text boundaries align with messages. SDK overflow and WFScript queue drops are counted independently. Native terminal display history is bounded by `maxLines`, separate from the byte queue.

## New Panel Designer widget

`UART Terminal (native RX)`, type ID `uart_terminal`, has a scrolling receive text area, optional hexadecimal display, configurable `maxLines` (100–10,000), total received byte count, SDK overflow count, and a Send+LF entry. The receive area is connected to native Qt events and **does not need a JavaScript receive handler**. Sending is intentionally an *operator event* (`sendRequested(text)`) that the Panel author can bind to `Protocol1.UART.writeText(text+'\n')` to keep hardware actions explicit. The display is cleared with `ui.set('uartView',{clear:true})`.

## New three-Page built-in example

**Examples → Digital Instruments → UART Async Lab — Native Terminal, Callback & Test** (`ProtocolUartAsync.wfpanel`):

1. **01 Native Terminal**: configure manual/automatic mode, send from native Terminal, receive through C++ direct updates, call unchanged manual `read()`, inspect `stats()`, stop timer, clear and release.
2. **02 JavaScript Callback**: register/clear `onData()`, observe incoming chunks and callback byte counter, and show that manual queue reads do not consume callback notifications.
3. **03 Automated Test**: editable, project-owned Test Executive definition and embedded `UARTEchoTestSteps.mjs` library. Setup starts auto RX; a Main numeric poll step sends `TEST\n`, checks the **exact five received bytes** on 20ms Test Executive ticks, and evaluates limits 5..5; Cleanup stops reception and attempts to release UART outputs. Hardware-dependent tests fail explicitly in Simulation rather than claiming a PASS.

All three Pages use **one long-lived JavaScript runtime**. Switching Pages does not recreate UART receiver or clear its queue. The Panel does not configure or drive any digital outputs merely by opening/running it; the operator must first choose an action.

## Safe Windows / AD3 verification

1. Open the complete source in a **new Qt Creator build directory**, Qt 6.9.2 MinGW 64-bit, `WFSCRIPT_BUILD_TESTS=ON`, and build Debug and Release with warnings enabled.
2. Run `ctest --test-dir <build_dir> -j 2 --output-on-failure`. The source registers **68** tests: the original 66, the earlier v0.2 protocol bridge smoke and new `WFScriptUartAsyncSmoke`. This new target is a native Qt headless **simulated** fixture (queue bounds, JS callbacks, direct Qt Terminal data, errors and cleanup).
3. Open the new example in **Simulation** and verify Pages, API Explorer/F1, widget behavior and event wiring. Simulated RX will be empty; it cannot PASS the hardware echo test.
4. Power off before wiring: connect **DIO0 TX -> 1 kΩ resistor -> DIO1 RX** on your AD3, with an electrically safe loopback. Do not connect AD3 DIO directly to unsafe external levels. Use Physical AD3 configuration 1.
5. On Page 01 select **Configure Auto RX**, send messages, verify bytes appear in the Terminal with **no receive-button press**. Click manual `read()` and verify it reports a native queue. Stop timer, drain queued bytes and check subsequent manual read; fully Release.
6. On Page 02 register callback; send messages, verify both the native Terminal (on hidden Page) and JS callback receive data; inspect `available()` and `stats()`; clear callback and verify native reception continues.
7. On Page 03 press **RUN TEST**; confirm Setup, Main numeric PASS = 5, Cleanup. Repeat; intentionally disconnect DIO0/DIO1 to confirm timeout/failure then Cleanup still runs. Verify digital outputs return safe state after termination and runtime Stop.
8. Stress test with bursts, increasing baud, delayed reads, repeated page switches, repeated Start/Stop and forced errors. Verify `sdkOverflows` vs `queueDroppedBytes`, avoid claiming lost bytes are impossible.

**Not yet validated in the source creation environment:** native Qt builds, running the 68 CTests, actual callback timing, DWF polling during long computations, and physical loopback on an AD3. Keep v0.1 as the user-tested stable baseline and v0.2-dev as previously tested only for limited manual UART loopback until user validation of this revision.
