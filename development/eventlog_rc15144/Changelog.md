# WFScript RC1.5.14.4 — Incremental Event Log Rollover

**Development candidate; Windows Qt native build and hardware/GUI execution pending.**

## Modified native files

- `src/widgets/EventLogWidget.h` / `.cpp`: retain a bounded `std::deque<Entry>` with monotonically assigned IDs; maintain visible filtered IDs in a second deque. Remove expired rows with `beginRemoveRows`/`endRemoveRows` and insert new visible rows with `beginInsertRows`/`endInsertRows`. No capacity-triggered `beginResetModel` or prefix-vector shifting. Keep full reset on explicit filter, clear, replacement, configure, or hidden-page reappearance. Anchor manual browsing by event ID; follow tail only if already at bottom. Switch three header columns to user-resizable fixed initial widths to avoid content scanning on each insert.
- `CMakeLists.txt`: add `WFScriptEventLogCapacityRC15144Smoke` hardware-free offscreen native CTest.
- `tests/`: add capacity/scroll/filter smoke, offline source contract, JS Panel mock; update two older source assertions to recognize the new incremental implementation.
- `development/eventlog_rc15144/`: include standalone Event Log timer test Panel.

## Unchanged

- All five Digilent and Rigol instrument driver/control engines, PanelRuntime, ScriptTimerManager, TestRunnerBridge, TestSequence, MeasurementTable, ExperimentWidgets, FileIO, SQLite and communications code; all 40 built-in example Panels and their JS source; public `.wfpanel` format, event log CSV columns, UI event-payload format, and 500/100,000 capacity defaults/bounds.

## Validation performed locally

- Python offline source checks passed.
- Node simulation of Panel timer callbacks/start/stop/clear passed.
- Repository and release static checks passed.
- Full source and patch ZIP integrity and exact patch reconstruction verified during packaging.

**Not performed:** Qt6 compilation, native CTests, Windows GUI timing/scroll testing, or physical hardware testing. The supplied Windows test guide describes how to validate using the user's 50k-Event-Log experiment.
