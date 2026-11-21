# WFScript — Learn by Building Real Panels

This is the learning guide for the **v0.2-dev.8 Panel Data revision**. It complements the guidance embedded at the top of the 36 original example scripts. The examples are working `.wfpanel` projects; you can run them, inspect their Events bindings, and save a modified copy.

## Start here: six short labs

1. **Getting Started** — run one designer-bound button and jump to its handler.
2. **Widget Playground** — connect multiple inputs to simulated displays; see Page-shared state.
3. **Panel Timers** — start/stop nonblocking periodic work and clean up callbacks.
4. **TCP Client/Server** — run a self-contained echo test; understand socket events.
5. **Communications Terminal** — reuse a widget with multiple transports, text and binary.
6. **SQLite Data Lab** — store and query synthetic measurements asynchronously.

No Analog Discovery or serial adapter is needed for these six labs. **New:** After these six labs, try **Panel Data Lab** to learn how named shared variables eliminate repeated `ui.set()` calls. It has a single Live Workbench Page showing input controls and results simultaneously; no hardware is needed.

After them, use the tables below to choose more specialized experiments.

## The five concepts to learn first

**1. `.wfpanel` = application:** Pages, widget positions, widget IDs, event bindings, test definitions (when present), and JavaScript are saved together.

**2. Design mode vs Run:** In Design mode select a widget, edit Properties → Events, then use Go to Handler. Run executes the project and automatically focuses the Panel.

**3. `ui.get` / `ui.set`:** The first reads a saved widget by its ID; the second changes a widget or supplies data to it. IDs must match exactly.

**4. Event origins are different:** `clicked` and `valueChanged` are Panel events bound in Properties; `client.on("data")`, `StaticIO.onChange` and `setInterval` are native/device/timer callbacks that remain in JavaScript. Do not register a duplicate `ui.on` for a saved designer event.

**5. Source of truth matters:** In Test Executive applications, the Test Sequence editor owns step order, timing, limits and Run Information; the operator Panel owns buttons and status. Do not duplicate test definitions in JavaScript.

**6. Panel Data (new):** Define variables in Properties → Panel Data, or create them from Properties → Value Binding. `Data.set()` keeps multiple bound widgets synchronized; optional input bindings update shared values before widget event callbacks. Hardware commands stay explicit.

## Choosing your next example

| Order | Example | Area | Difficulty | Core lesson |
|---:|---|---|---|---|
| 0 | [Panel Data Lab — Shared Variables](guides/PanelDataLab.md) | Getting Started | Beginner–Intermediate | Project-owned typed variables, one-way displays and two-way inputs. |
| 1 | [Getting Started](guides/GettingStarted.md) | Getting Started | Beginner | Understand a Panel project, Design vs Run, and saved visual event bindings. |
| 2 | [Widget Playground](guides/WidgetPlayground.md) | Getting Started | Beginner | Understand input widgets, simulated calculations, and displays shared across Pages. |
| 3 | [Multiline and Rich Text Labels](guides/LabelFormatting.md) | Graphics & Charts | Beginner | Use plain multiline text and formatted HTML in Label widgets. |
| 4 | [Panel JavaScript Timers — Interval & Timeout](guides/PanelTimers.md) | Getting Started | Beginner | Start, stop and cancel nonblocking timers without freezing the user interface. |
| 5 | [File I/O & Operator Dialogs](guides/ScriptUtilityLab.md) | Getting Started | Beginner | Write and read CSV, JSON and binary data; use asynchronous operator dialogs. |
| 6 | [SQLite Database & Data Logging](guides/SQLiteDataLab.md) | Tables & Data | Beginner–Intermediate | Persist synthetic readings in SQLite, run queries, and display results. |
| 7 | [Embedded Images and Instruction Cards](guides/EducationalVisuals.md) | Graphics & Charts | Beginner | Put an image, equation and instructional cards into a self-contained Panel. |
| 8 | [Chart Techniques and Analytics](guides/ChartTechniques.md) | Graphics & Charts | Intermediate | Create and style plotted data, visible/hidden series and axis configurations. |
| 9 | [Analog Charts and Frequency](guides/AnalogCharts.md) | Graphics & Charts | Beginner–Intermediate | Compare time-series, XY and frequency-domain plots. |
| 10 | [Digital Timing and Protocol Charts](guides/DigitalTimingCharts.md) | Graphics & Charts | Beginner–Intermediate | Understand sampled digital signals and schematic protocol timing. |
| 11 | [Mathematical Equations and Experiment Tables](guides/EquationExperimentLab.md) | Tables & Data | Beginner–Intermediate | Combine a theory Page, a frequency slider and editable experiment tables. |
| 12 | [Calibration Lookup — Editable Table](guides/CalibrationTable.md) | Tables & Data | Intermediate | Build an editable calibration curve and use it to convert sensor input to engineering units. |
| 13 | [Table-to-Plot — Limits and Observations](guides/TableLimitPlot.md) | Tables & Data | Intermediate | Turn operator-edited data into observed, nominal and tolerance-limit plots. |
| 14 | [Table-Driven Wavegen — Multi-Trace Sweep](guides/TableDrivenWavegen.md) | Tables & Data | Advanced | Use an editable frequency recipe to generate preview traces or deliberately drive W1 and capture CH1. |
| 15 | [Signal Lab — Wavegen and Scope](guides/SignalLab.md) | Analog Instruments | Beginner–Intermediate | Control waveform shape and amplitude; preview W1 and acquire Scope CH1. |
| 16 | [AD3 Scope and Range Discovery](guides/AD3ScopeExplorer.md) | Analog Instruments | Beginner–Intermediate | Acquire a Scope CH1 snapshot, compute statistics and query SDK hardware range choices. |
| 17 | [V+ / V− Power Supplies](guides/PowerSupplies.md) | Analog Instruments | Intermediate | Set separate V+/V− rails and watch actual readback follow changes over time. |
| 18 | [Static Digital I/O](guides/StaticDigitalIO.md) | Digital Instruments | Intermediate | Set GPIO lines to Z/LOW/HIGH and read back the 16-bit digital input mask. |
| 19 | [Digital Input Test Station — Native Monitor](guides/DigitalInputMonitor.md) | Digital Instruments | Advanced | Monitor debounced DIO input changes and optionally trigger the Test Executive. |
| 20 | [Logic Analyzer](guides/LogicAnalyzer.md) | Digital Instruments | Intermediate | Capture DigitalIn samples and display transitions with configurable rate and depth. |
| 21 | [Digital Clock / PWM](guides/ClockPWM.md) | Digital Instruments | Intermediate | Generate a hardware-timed clock/PWM and compare requested duty to captured duty. |
| 22 | [Quadrature Encoder + Index](guides/QuadratureEncoder.md) | Digital Instruments | Advanced | Generate quadrature A/B with index and verify phase, direction and speed. |
| 23 | [Multi-Channel Sequence Lab](guides/SequenceLab.md) | Digital Instruments | Advanced | Generate synchronized binary, Gray and walking-one sequences on several digital pins. |
| 24 | [Automated Test Bench — Measurement Results](guides/AutomatedTestBench.md) | Test Sequences & Executive | Beginner–Intermediate | Display measurement PASS/FAIL verdicts and maintain a compact table of results. |
| 25 | [Automated Test Station — Project-Owned C++ Executive](guides/AutomatedTestStation.md) | Test Sequences & Executive | Advanced | Operate a project-owned Setup/Main/Cleanup Test Executive and inspect archived reports. |
| 26 | [AD3 Hardware Test Station — W1 to CH1](guides/AD3HardwareTestStation.md) | Test Sequences & Executive | Advanced | Run a real W1→Scope CH1 loopback test with native limit checking and reports. |
| 27 | [UART Protocol — Native SDK Terminal](guides/ProtocolUART.md) | Digital Instruments | Intermediate | Use native AD3 UART commands to transmit text and inspect received data. |
| 28 | [SPI Protocol — Master Transactions](guides/ProtocolSPI.md) | Digital Instruments | Intermediate | Configure an AD3 SPI master and inspect a command/response transfer. |
| 29 | [I²C Protocol — Master and Spy](guides/ProtocolI2C.md) | Digital Instruments | Intermediate | Configure a native I²C master, issue a register read and inspect Spy traffic. |
| 30 | [CAN Protocol — Classical CAN](guides/ProtocolCAN.md) | Digital Instruments | Advanced | Configure classical CAN, transmit a frame and poll the receive side. |
| 31 | [SWD Protocol — Target Registers](guides/ProtocolSWD.md) | Digital Instruments | Advanced | Read a debug-port identification register from a compatible ARM SWD target. |
| 32 | [UART Asynchronous Lab — Native Terminal, Callbacks and Test](guides/ProtocolUartAsync.md) | Digital Instruments | Advanced | Compare native UART Terminal display, JavaScript receive callbacks and automated echo testing. |
| 33 | [PC Serial — Text & Binary](guides/SerialCommsLab.md) | Communications | Beginner–Intermediate | Discover PC COM ports, open a port and send both text and binary data. |
| 34 | [TCP Local Client/Server — Text & Binary](guides/TcpCommsLab.md) | Communications | Beginner–Intermediate | Use a TCP client/server on localhost and compare text lines with binary stream data. |
| 35 | [UDP Local Datagram Echo — Text & Binary](guides/UdpCommsLab.md) | Communications | Beginner–Intermediate | Use two localhost UDP sockets to exchange individual datagrams. |
| 36 | [Communications Terminal — Serial / TCP / UDP](guides/CommunicationsTerminalLab.md) | Communications | Beginner–Intermediate | Reuse one native Terminal widget with TCP, UDP and optional PC Serial connections. |

## Build something from a blank Panel

The [step-by-step voltage divider Panel](YOUR_FIRST_PANEL.md) starts from a blank canvas and builds a numeric calculator using visual event bindings, `ui.get()` and `ui.set()`—with no hardware or manual `ui.on()` required.

## A practical first custom project

1. Open **Widget Playground** and use **Save As** so the built-in example is preserved.
2. In Design mode rename one label, reposition a display and select the LOAD slider.
3. In Properties → Events, choose `valueChanged` and use Go to Handler to find `wbRefresh()`.
4. Find the calls to `ui.get("wbLoad")` and `ui.set(...)`; read the simulated calculation and its units.
5. Change a single simulation constant, Run Panel, move the slider, and compare the output.
6. Stop/Design and save the modified `.wfpanel` as your own reusable Panel.

## Example conventions (recommended for new projects)

- Put user-editable constants at the beginning of the script and name them descriptively.
- Give one clearly named function responsibility for each task. Delegate repeated logic to helper functions.
- Use Properties → Events for fixed widget actions; reserve `ui.on()` for genuinely dynamic controls.
- Keep external device/socket and timer callbacks separate from widget event handlers.
- Always say whether a value is **simulated**, **requested/setpoint**, or **measured/actual**.
- On hardware, make actions explicit; check valid electrical connections, handle errors, and stop/release outputs.
- On TestRunner projects, author limits and step order in **Test Sequence**, not duplicated in the operator script.
- Prefer short, informative messages that tell the operator the next action after an error.

## Packaging and verification

`examples/source/*.js` is the editable script authority. The same text is embedded as `script` in each project file. The source tree contains `examples/guides/*.md` for instructor/developer reference; individual `.wfpanel` files still include their own short walkthrough as script comments.

```powershell
python tools/build_examples.py
python tools/build_examples.py --check
python tests/example_curriculum_check.py
```

## Hardware safety

Running an example in Simulation does not establish physical-device correctness. The protocol Panels may configure DIO pins when explicitly started, and supply/Wavegen Panels can produce physical voltages. Check every example’s own safety and wiring notes before selecting Physical Device.
