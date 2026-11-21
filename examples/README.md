# WFScript v0.2 — Built-in Examples

The **Examples** menu in Panel Designer provides 40 bundled, editable `.wfpanel` projects embedded by `src/resources/examples.qrc`. Each example is a normal project that can be inspected, run, and saved.

## Learn from the examples

**Start with [LEARN_WFSCRIPT.md](LEARN_WFSCRIPT.md)** and [build a voltage divider from a blank Panel](YOUR_FIRST_PANEL.md) for a six-lab beginner path, then follow the 36 original [Lab Guides](guides/README.md) and the new [Panel Data Lab](guides/PanelDataLab.md). Each example script embeds a focused Goal / Try First / Trace / Edit Safely walkthrough, so the learning instructions are still present when the `.wfpanel` is exported alone.

The recommended workflow is: **Design → select widget → Properties → Events → Go to Handler → Run → Stop/Design → Save As**.

## Groups

| Group | Focus |
| --- | --- |
| Getting Started | Orientation and general widgets |
| Graphics & Charts | Labels, equations, images, and plotting techniques |
| Tables & Data | Equations, editable tables, calibration, and table-driven workflows |
| Analog Instruments | Wavegen, Scope, and supplies examples |
| Digital Instruments | Static I/O, logic/timing, clocks, quadrature, sequences, UART, SPI, I²C, CAN, SWD |
| Test Sequences & Executive | Test widgets, run information, and automated test flows |

**New in v0.2-dev.8:** **PanelDataLab.wfpanel** is a one-Page live workbench, simulation-only example of typed shared variables, display and input Value bindings, and `Data.get()` / `Data.set()`. Select a widget → Properties → Value Binding → **+ Create New Variable...**, or use the project-level Properties → Panel Data tab. See [Panel Data reference](../docs/reference/PANEL_DATA_V02_DEV8.md).

## Particularly useful examples

- **Getting Started** — first orientation example.
- **TableDrivenWavegen** — operator-entered table rows used to drive a sweep and retain traces.
- **TableLimitPlot** — table data turned into nominal/observed/limit plots.
- **SignalLab** — practical analog controls.
- **AD3HardwareTestStation** — full automated test example.
- **ProtocolUART / ProtocolSPI / ProtocolI2C / ProtocolCAN / ProtocolSWD** — five SDK-native protocol scripting demonstrations, with wiring notes and an Event Log per Panel. These are **not** passive waveform decoders. All outputs require explicit button actions; simulation does not fabricate acknowledgements. See [protocol scripting guide](../docs/reference/PROTOCOL_SCRIPTING_V02_DEV.md).

- **ProtocolUartAsync — UART Async Lab** — three Pages showing native direct-update Terminal, optional JavaScript callback, and a native Test Executive loopback PASS/FAIL step. No DIO outputs enabled on project open. [UART async guide](../docs/reference/UART_ASYNC_V02_DEV1.md).

## Example maintenance

Readable source is kept in `examples/source/` and embedded project panels in `examples/panels/`.
After editing example scripts, run:

```powershell
python tools/build_examples.py
python tools/build_examples.py --check
```


### Communications Terminal Lab (v0.2-dev.7)

`CommunicationsTerminalLab.wfpanel` is a three-page example of the generic
`communications_terminal` widget. TCP and UDP each create local echo peers and
show text and binary TX/RX. Serial is optional and requires a COM device or
virtual pair. The `sendRequested` widget event is routed by JavaScript, not
by the terminal itself. Open the .wfpanel directly or use Examples → Communications.

## Visual Events tutorial (v0.2-dev.7.3)

In **Design mode**, select a widget, open **Properties → Events**, choose a supported event, and use **Go to Handler** to find its JavaScript function. **Create Handler** inserts a correctly parameterized function using the event catalog.

Start with **Getting Started → Events Tutorial**. The Widget Playground, Panel Timers, SQLite Data Lab, and the Serial/TCP/UDP/Communications Terminal examples now demonstrate the same visual-binding workflow. Connection events (`Serial/TCP/UDP.on`), database results, and timers remain JavaScript callbacks.

Avoid adding `ui.on()` for a widget event that already has an enabled designer binding, or both callbacks execute. See [Visual Event Examples v0.2-dev.7.3](../docs/reference/VISUAL_EVENT_EXAMPLES_V02_DEV7_3.md).

### RC Step Response and Time Constant Laboratory (v0.2-dev.8.9.3)

The `RCStepResponseLaboratory.wfpanel` is a fully embedded, eleven-Page companion to the AC Network Laboratory. It teaches the RC differential equation and exponential response, generates clearly labeled synthetic step/response data, uses pure JavaScript exponential least squares to determine τ, and offers an operator-gated physical AD3 W1 Square + two-channel Scope acquisition path. It includes a 63.2% crossing comparison, full fit residuals, a source-code walkthrough, time-vs-frequency cutoff comparison, and explicit timestamped CSV/JSON exports. See [its detailed guide](guides/RCStepResponseLaboratory.md). The project owner has successfully captured real AD3 waveforms and validated the exponential-fitting, Notebook, and CSV export workflows for this example; full hardware and native-test coverage is still selective.
