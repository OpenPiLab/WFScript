> **v0.2-dev.1 extension:** Native UART auto-RX, callbacks and directly updated Terminal widget are documented separately in [UART_ASYNC_V02_DEV1.md](UART_ASYNC_V02_DEV1.md).

# WFScript v0.2-dev — Digilent digital protocol scripting

**Development candidate; Windows Qt 6.9.2 / MinGW and physical Analog Discovery 3 testing are PENDING.**

This extension uses the **March 4, 2026 WaveForms SDK Reference Manual, Chapter 13** and the supplied Digilent SDK Python samples (`Digital_Uart.py`, `Digital_Spi.py`, `Digital_I2c.py`, `Digital_I2c_Spy.py`, `Digital_Can.py`, `Digital_Swd.py`). Native Digilent functions are dynamically resolved at runtime from the existing `dwf.dll`; no vendor SDK code or DLLs are redistributed.

## What's included

- `Protocol1` and unnumbered alias `Protocol`, exposed in both standalone scripts and persistent Panel project scripts.
- `Protocol1.UART`: binary TX, UTF-8 TX, RX polling, baud/data/parity/stop/inversion, WaveForms-style Send/Receive aliases.
- `Protocol1.SPI`: conventional master, 1/2/4 data lanes, 1..32-bit words, 1..8-bit command/dummy-cycle transfers, modes 0..3, order, duty, delays, selection, WaveForms-style helper aliases.
- `Protocol1.I2C`: 7-bit address register read/write/repeated-start, clock stretching, timeout, native SDK I²C Spy start/poll, and partial WaveForms-style aliases.
- `Protocol1.CAN`: **classical CAN** 11/29-bit IDs, 0..8 byte frames, basic remote-frame support, native TX and RX error status.
- `Protocol1.SWD`: native DP/AP read/write, line clear/reset, decoded ACK and parity status.
- Central `DwfRuntime` native protocol ownership; rejects conflict with active Logic capture, any touched Pattern output, or enabled StaticIO output pins. Existing Patterns/Logic/StaticIO are blocked from taking over protocol-owned resources.
- Explicit `Protocol1.stop()` and runtime shutdown attempt to release outputs. Failed protocol operations request shutdown and surface a warning when shutdown confirmation fails.
- Five simulation-safe embedded `.wfpanel` examples, each with a status indicator and native Event Log supporting Copy/CSV export.
- Editor autocomplete, F1 help, API Explorer and source-level diagnostics updated for all documented methods.

## Clearly unsupported by this SDK-native revision

**This is a native protocol controller/Spy scripting foundation, NOT the complete WaveForms graphical Protocol Analyzer.** It does not claim every WaveForms application Script method or GUI mode. In particular, **passive SPI/UART edge-decoding, time-aligned annotated waveforms, SPI/I²C slave emulation, CAN FD, I3C, JTAG, CEC, LIN and I²S decoders are not included**. Digilent's separate Python DigitalIn SPI Spy examples implement software sampling/decoding; that work has not been integrated here. I²C Spy is available natively, but its timestamps come from host polling, not precise bus edges.

WaveForms-style alias methods are **partial compatibility conveniences**, not complete behavioral compatibility. Prefer lower-case methods returning structured maps for automated validation. For example, `Protocol1.I2C.Receive()` produces START/RESTART/STOP sentinels and **raw 8-bit bytes**, not WaveForms' 9-bit data+ACK encoding; `spyPoll()` exposes `nackIndex` separately. `Protocol1.CAN.Receive()` omits physical CRC and ACK fields unavailable from the SDK. Neither method makes an invalid bus transaction appear successful.

## JavaScript usage

```javascript
// I2C: 7-bit address (the native SDK expects address << 1).
if (!Protocol1.I2C.configure({scl:0,sda:1,rateHz:100000}))
    throw Error(Protocol1.I2C.lastError);
const r=Protocol1.I2C.writeRead(0x48,[0x00],2);
if (!r.ok) throw Error(r.error);
if (!r.acknowledged) throw Error('I2C NACK: '+r.nackIndex);
print('Register data:',r.data);
if (!Protocol1.stop()) throw Error(Protocol1.lastError);
```

```javascript
// SPI: JEDEC ID is meaningful ONLY with a compatible external flash chip.
if (!Protocol1.SPI.configure({clock:0,mosi:1,miso:2,select:3,
    frequencyHz:1000000,mode:0}))
    throw Error(Protocol1.SPI.lastError);
const id=Protocol1.SPI.command(8,0x9F,0,[],3,8,1);
if (!id.ok) throw Error(id.error);
print('RX:',id.rx,id.simulated?'SIMULATED':'HARDWARE');
Protocol1.stop();
```

Important: a missing DWF export or SDK call failure results in a descriptive error. In Simulation, data is marked `simulated:true`, RX is empty or synthetic zero-filled, and I²C/SWD acknowledgement is NEVER fabricated as a real device response.

### API conventions

| Protocol | Configure | Transaction methods | Native result metadata |
|---|---|---|---|
| UART | `UART.configure({tx,rx,baudRate,dataBits,parity,stopBits})` | `startRx()`, `write(bytes)`, `writeText(text)`, `read(maxBytes)` | `data`, `count`, `parityErrorIndex`, `overflow` |
| SPI | `SPI.configure({clock,mosi,miso,select,frequencyHz,mode,...})` | `transfer(tx,readCount,bits,lanes)`, `command(...)`, `Start()/Stop()` | `rx`, `words` |
| I²C | `I2C.configure({scl,sda,rateHz,...})` | `writeRead(address7,tx,count)`, `write`, `read`, `Clear`, `Receiver`, `spyPoll` | `data`, `nackIndex`, `acknowledged`, spy markers |
| CAN | `CAN.configure({tx,rx,rateHz})` | `startRx()`, `send(id,extended,remote,data)`, `receive()` | `id`, `extended`, `remote`, `dlc`, `status`, `data` |
| SWD | `SWD.configure({clock,data,rateHz})` | `clear()`, `readRegister(ap,index)`, `writeRegister(ap,index,value)` | `ack` (1/2/4), `ackName`, `parityError`, `value` |

The alias `Protocol` is the same object as `Protocol1`. Only one native SDK protocol may be active at a time; call `Protocol1.stop()` before switching protocols. `SPI.Stop()` only deasserts chip select; **it does not free the shared DigitalOut resource**.

## Hardware safety and wiring

- **Every physical transmit/configure action can drive DIO pins**; use an electrically safe fixture and select Physical Device deliberately. Opening/running each example alone does not configure the native output.
- **UART**: DIO0 TX -> 1 kΩ -> DIO1 RX loopback, common GND. SDK UART TX initialization is performed at configure time; call `startRx()` before polling.
- **SPI**: DIO0 CLK, DIO1 MOSI, DIO2 MISO, DIO3 CS. Connect MOSI -> 1 kΩ -> MISO for a loopback, but a JEDEC flash needs a compatible IC and proper chip select/power.
- **I²C**: DIO0 SCL, DIO1 SDA, with external pull-ups to a safe **3.3 V** rail and a real I²C device; use the actual device register map. For native Spy, avoid driving a bus under test.
- **CAN**: DIO0 TX / DIO1 RX connect to a **suitable logic-level CAN transceiver**, not the physical CANH/CANL lines. Use a valid terminated bus, not raw DIO connections.
- **SWD**: DIO0 SWCLK, DIO1 SWDIO, target 3.3V-compatible IO, common GND. DP register read is the supplied example; SWD writes can affect a target and must be deliberate.
- Check SDK and board electrical specifications, cable wiring, permitted input voltages, signal direction, and any required isolation before hardware use.

## SDK ABI verification: required on Windows before applying power

The supplied 2026 PDF conflicts with its own Python samples/earlier headers for some declarations. **The code intentionally follows the SDK Python ctypes samples** for:

- `FDwfDigitalUartBitsSet(H,int)`: PDF describes a double in one printing, Python uses `c_int`.
- `FDwfDigitalCanPolaritySet(H,int)`: PDF prints double, Python uses `c_int`.
- `FDwfDigitalSwdRead/Write`: PDF prints `unsigned char` payload, Python uses 32-bit `c_uint` (correct for SWD 32-bit register data).
- `FDwfDigitalSpiCmdWriteRead*`: document unsigned-char command; script API limits command width to **8 bits** to avoid truncation.

Compare these exact declarations to **your installed** `C:\Program Files\Digilent\WaveFormsSDK\inc\dwf.h`, and verify DLL exports, before hardware experiments. Unsupported/missing symbols fail explicitly and are not silently emulated. No physical signals, real CAN, native timing, SDK stop/release behavior, SWD, or I²C Spy observations have been verified in this environment.

## Validation procedure (Windows Qt Creator 6.9.2 / MinGW 64-bit)

1. Unzip the complete source to a **new short path**, e.g., `C:\Dev\WFScript_v0_2_dev`.
2. Open `CMakeLists.txt` with your existing Qt 6.9.2 MinGW kit. Use a **fresh** build directory and `-DWFSCRIPT_BUILD_TESTS=ON`.
3. Build Debug and Release and inspect warnings (goal zero). Verify that the new `WFScriptProtocolBridgeSmoke` target compiles and executes.
4. `ctest --test-dir <new_build_dir> -j 2 --output-on-failure`; expected at least the original **66** passing native tests **plus** the new protocol smoke test (67 registered in this revision).
5. Open the Instrument & Script API Explorer and search `Protocol`, `UART`, `SPI`, `I2C`, `CAN`, `SWD`; verify method signatures and F1 help.
6. In Simulation, open each of the five new Examples. Press configure/send/poll/stop; verify `simulated:true` or explicit Simulation labels and the Event Log, then export CSV. Ensure **no simulated I²C ACK or CAN/SWD frame** is portrayed as real.
7. On a safely connected AD3, test UART DIO0->1 loopback, SPI DIO1->2 loopback and frequency/mode, I²C ACK/NACK/repeated-start and Spy with a proper device, CAN with proper transceiver(s), and SWD with a known target.
8. While each protocol is configured, verify that `Patterns1`, `Logic1`, and `StaticIO` cannot take control unexpectedly. Stop the protocol, then verify original examples still function.
9. Simulate a missing `dwf.dll`/missing export, deliberate invalid pin, failed I²C transaction, CAN bus absent, timeout, Stop and forced script stop; record any output that fails to return safe state. **Do not treat shutdown as guaranteed under forcibly interrupted native DLL calls.**
10. Reopen and save old public v0.1 `.wfpanel` files; the schema is unchanged.

### Test status at generation

| Validation | Status |
|---|---|
| Source/ZIP integrity, 27 embedded examples, resource paths | Offline checked |
| Node.js syntax of five new Panel scripts | Offline checked |
| Existing Python source/API/repository guards | Offline checked |
| New native `WFScriptProtocolBridgeSmoke` | Source added, NOT compiled in this environment |
| Full Windows Debug/Release builds and native CTests | **PENDING USER VALIDATION** |
| Physical Digilent Analog Discovery 3 UART/SPI/I²C/CAN/SWD tests | **PENDING USER VALIDATION** |

## Deliberate design boundary

The project remains a native C++/Qt instrument runtime with a QJSEngine API and project-owned `.wfpanel` configuration. No new dependencies, no duplicate scripting engine, and no Test Executive changes. A later revision may add hardware-independent DigitalIn UART/SPI/I²C decoders and a waveform-aligned Protocol Monitor widget, with finite and streaming capture tests. Those would be distinct work items, not undocumented assumptions.
