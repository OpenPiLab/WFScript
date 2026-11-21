# WFScript RC1.5.14.4 — Event Log Rolling-Capacity Test Guide

**Status:** Native Qt6/Windows and physical UI tests **not yet run**. This source candidate is based on RC1.5.14.3 and has passed offline source/Panel checks only.

## What changed

The Event Log's 50,000-entry rollover no longer resets and rebuilds the complete visible table on each arrival. The native model now uses a `std::deque` for retained entries and stable IDs in another deque for visible rows. It signals only removed and inserted rows; filtering, explicit replacement, clearing, and reconfiguration may still rebuild once. Three header columns have fixed initial, user-resizable widths instead of repeated `ResizeToContents` scans.

No change to `ui.set('stationLog', ...)`, to timestamps, to exported CSV headings, to `.wfpanel` compatibility, to the Timer Manager, or to instrument/Test Executive behavior.

## Installation

Use the **complete source** ZIP for a clean trial, or the incremental patch on top of the exact RC1.5.14.3 source. Do not apply the patch onto older versions. Keep your confirmed Windows project backed up.

Build using the existing Qt 6.9.2 MinGW environment. With `WFSCRIPT_BUILD_TESTS=ON`, the new hardware-free `WFScriptEventLogCapacityRC15144Smoke` test will be included. This test appends 50,000 initial entries, rolls forward 37,500 more entries, verifies retained oldest/newest records, asserts **zero model resets** during live rollover, changes filters, and exercises hidden-page recovery and scroll anchoring. Run the regular native CTests as well. There is no native build validation yet from this environment.

## Manual Panel experiment

Open `WFScript_RC1_5_14_4_EventLog_50k_Rollover_Lab.wfpanel` (supplied separately and inside the source ZIP). This is a hardware-free single-page Panel derived from your `LargeLogTest.wfpanel`.

1. In the Panel, verify the Event Log has **Maximum retained log entries = 50000**, then press **START 3 TIMERS**. It creates one 10 ms interval (`id`), one 20 ms interval (`id1`), and one 30 ms interval (`id2`). Each firing produces an INFO/PASS pair, approximately 366 entries per second. The 30 ms Qt coarse timer may actually behave closer to 31.25 ms.
2. Scroll to the middle or top of the table while it logs. Optionally use the INFO/PASS filters and then return to All. Check whether a visible historical row stays anchored while new events arrive.
3. Run for **at least four minutes** (preferably five). The 50,000-event cap should fill after roughly 2 minutes 17 seconds and then continuously evict old entries. This is an estimate based on nominal rates, not a timing guarantee.
4. Press **STOP TIMERS** *before exporting*, and choose **Export CSV** in the Event Log. Send the CSV and tell us whether you were actively scrolling during rollover and if you noticed any pauses.
5. Optional: repeat the same experiment against the original RC1.5.14.3 build for a before/after comparison, keeping all Panel settings the same.

### How we will analyze the CSV

- `Timestamp`: timestamp assigned by the GUI Event Log when it receives/appends the event (millisecond wall clock).
- `Source`: timer name `id`, `id1` or `id2`.
- `Message` in INFO rows: includes `run`, `seq`, and `callbackEpochMs`, the **JavaScript-worker callback time** from `Date.now()`.
- Each INFO row should be followed by the PASS row with matching timer and sequence. The cap retains only the **last** 50,000 entries; incomplete first pair at the retention boundary does not by itself mean loss.
- For each timer, compare sequence continuity, differences between callback timestamps, differences between GUI timestamps, and the per-event backlog (`GUI epoch time − callbackEpochMs`). The latter can vary slightly due to two wall-clock reads and queueing; it is not a high-resolution latency benchmark.

The new Panel does *not* change the underlying timer type (`Qt::CoarseTimer`). We are investigating **GUI log throughput**, not claiming 10 ms hard real-time scheduling.

## Acceptance criteria

- No sustained timing disturbance or GUI freeze when the 50,000-entry boundary is crossed.
- Table remains responsive during scrolling after rollover; historical row stays anchored until it itself expires.
- INFO/PASS ordering and sequence numbers remain consistent (apart from naturally evicted oldest entries).
- Exported CSV has the intended most-recent 50,000 entries and correct source/filter independence.
- Native rollover smoke and existing CTests pass on the user's Windows Qt build.

## What this revision does not implement

This is **Phase 1**: Event Log performance only. It does not yet convert Experiment DataTable, Test Results Browser, or further optimize Measurement Table; their present APIs remain unchanged. An API/profiling roadmap is supplied separately so that changes to these high-value widgets can be designed and validated independently.
