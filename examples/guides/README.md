# WFScript — Example Learning Guides

All 36 original examples have a short, project-specific learning banner inside their JavaScript and a standalone, more detailed lab guide. **No executable JavaScript statements were changed for this revision.**

Begin with [Learn WFScript](../LEARN_WFSCRIPT.md).

| Example | Level | Learning objective |
|---|---|---|
| [Getting Started](GettingStarted.md) | Beginner | Understand a Panel project, Design vs Run, and saved visual event bindings. |
| [Widget Playground](WidgetPlayground.md) | Beginner | Understand input widgets, simulated calculations, and displays shared across Pages. |
| [Multiline and Rich Text Labels](LabelFormatting.md) | Beginner | Use plain multiline text and formatted HTML in Label widgets. |
| [Panel JavaScript Timers — Interval & Timeout](PanelTimers.md) | Beginner | Start, stop and cancel nonblocking timers without freezing the user interface. |
| [File I/O & Operator Dialogs](ScriptUtilityLab.md) | Beginner | Write and read CSV, JSON and binary data; use asynchronous operator dialogs. |
| [SQLite Database & Data Logging](SQLiteDataLab.md) | Beginner–Intermediate | Persist synthetic readings in SQLite, run queries, and display results. |
| [Embedded Images and Instruction Cards](EducationalVisuals.md) | Beginner | Put an image, equation and instructional cards into a self-contained Panel. |
| [Chart Techniques and Analytics](ChartTechniques.md) | Intermediate | Create and style plotted data, visible/hidden series and axis configurations. |
| [Analog Charts and Frequency](AnalogCharts.md) | Beginner–Intermediate | Compare time-series, XY and frequency-domain plots. |
| [Digital Timing and Protocol Charts](DigitalTimingCharts.md) | Beginner–Intermediate | Understand sampled digital signals and schematic protocol timing. |
| [Mathematical Equations and Experiment Tables](EquationExperimentLab.md) | Beginner–Intermediate | Combine a theory Page, a frequency slider and editable experiment tables. |
| [Calibration Lookup — Editable Table](CalibrationTable.md) | Intermediate | Build an editable calibration curve and use it to convert sensor input to engineering units. |
| [Table-to-Plot — Limits and Observations](TableLimitPlot.md) | Intermediate | Turn operator-edited data into observed, nominal and tolerance-limit plots. |
| [Table-Driven Wavegen — Multi-Trace Sweep](TableDrivenWavegen.md) | Advanced | Use an editable frequency recipe to generate preview traces or deliberately drive W1 and capture CH1. |
| [Signal Lab — Wavegen and Scope](SignalLab.md) | Beginner–Intermediate | Control waveform shape and amplitude; preview W1 and acquire Scope CH1. |
| [AD3 Scope and Range Discovery](AD3ScopeExplorer.md) | Beginner–Intermediate | Acquire a Scope CH1 snapshot, compute statistics and query SDK hardware range choices. |
| [V+ / V− Power Supplies](PowerSupplies.md) | Intermediate | Set separate V+/V− rails and watch actual readback follow changes over time. |
| [Static Digital I/O](StaticDigitalIO.md) | Intermediate | Set GPIO lines to Z/LOW/HIGH and read back the 16-bit digital input mask. |
| [Digital Input Test Station — Native Monitor](DigitalInputMonitor.md) | Advanced | Monitor debounced DIO input changes and optionally trigger the Test Executive. |
| [Logic Analyzer](LogicAnalyzer.md) | Intermediate | Capture DigitalIn samples and display transitions with configurable rate and depth. |
| [Digital Clock / PWM](ClockPWM.md) | Intermediate | Generate a hardware-timed clock/PWM and compare requested duty to captured duty. |
| [Quadrature Encoder + Index](QuadratureEncoder.md) | Advanced | Generate quadrature A/B with index and verify phase, direction and speed. |
| [Multi-Channel Sequence Lab](SequenceLab.md) | Advanced | Generate synchronized binary, Gray and walking-one sequences on several digital pins. |
| [Automated Test Bench — Measurement Results](AutomatedTestBench.md) | Beginner–Intermediate | Display measurement PASS/FAIL verdicts and maintain a compact table of results. |
| [Automated Test Station — Project-Owned C++ Executive](AutomatedTestStation.md) | Advanced | Operate a project-owned Setup/Main/Cleanup Test Executive and inspect archived reports. |
| [AD3 Hardware Test Station — W1 to CH1](AD3HardwareTestStation.md) | Advanced | Run a real W1→Scope CH1 loopback test with native limit checking and reports. |
| [UART Protocol — Native SDK Terminal](ProtocolUART.md) | Intermediate | Use native AD3 UART commands to transmit text and inspect received data. |
| [SPI Protocol — Master Transactions](ProtocolSPI.md) | Intermediate | Configure an AD3 SPI master and inspect a command/response transfer. |
| [I²C Protocol — Master and Spy](ProtocolI2C.md) | Intermediate | Configure a native I²C master, issue a register read and inspect Spy traffic. |
| [CAN Protocol — Classical CAN](ProtocolCAN.md) | Advanced | Configure classical CAN, transmit a frame and poll the receive side. |
| [SWD Protocol — Target Registers](ProtocolSWD.md) | Advanced | Read a debug-port identification register from a compatible ARM SWD target. |
| [UART Asynchronous Lab — Native Terminal, Callbacks and Test](ProtocolUartAsync.md) | Advanced | Compare native UART Terminal display, JavaScript receive callbacks and automated echo testing. |
| [PC Serial — Text & Binary](SerialCommsLab.md) | Beginner–Intermediate | Discover PC COM ports, open a port and send both text and binary data. |
| [TCP Local Client/Server — Text & Binary](TcpCommsLab.md) | Beginner–Intermediate | Use a TCP client/server on localhost and compare text lines with binary stream data. |
| [UDP Local Datagram Echo — Text & Binary](UdpCommsLab.md) | Beginner–Intermediate | Use two localhost UDP sockets to exchange individual datagrams. |
| [Communications Terminal — Serial / TCP / UDP](CommunicationsTerminalLab.md) | Beginner–Intermediate | Reuse one native Terminal widget with TCP, UDP and optional PC Serial connections. |
