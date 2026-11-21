# WFScript Table Performance and JavaScript API — Architecture Roadmap

**Source baseline:** WFScript RC1.5.14.3; Event Log Phase 1 patch RC1.5.14.4. **Status:** This is a design roadmap; examples marked *proposed* are **not implemented**.

## Current behavior verified in source

### Event Log (`event_log`)

Uses a read-only native `QTableView` and filtered `QAbstractTableModel`, with `ui.set(id,{append:entry})`, append-array, full replacement, and `{clear:true}`. Defaults to 500 entries; configurable up to 100,000. Export CSV and Copy buttons already exist. RC1.5.14.4 makes capacity rollover incremental; it does not change the JavaScript API.

### Experiment DataTable (`experiment_table`)

Currently an editable `QTableWidget`; **2,000 rows** maximum, **32 columns**, and a maximum of 256 characters per cell. `ui.set(id, arrayOfRowArrays)` replaces the whole table, `ui.get(id)` reads the entire table, and `valueChanged(rows)` sends the entire table after an editable cell change. **Export CSV already exists** through the widget's right-click menu, along with Copy as TSV. There is **no corresponding CSV import workflow** or incremental append/update path in the present widget. Full-cell rebuilding and whole-table `valueChanged` are a poor fit for streaming TCP/UDP measurements.

### Measurement Table (`measurement_table`)

Already uses native `QAbstractTableModel` and incremental insertion. The Test Executive feeds it `{append:measurement}` and `{append:[measurements...]}`. JavaScript can also call:

```javascript
ui.set('measurementTable', {
    append: {name:'VOUT',value:3.300,low:3.25,high:3.35,unit:'V'}
});
ui.set('measurementTable', {reset:true});
// Replacing the whole history is also possible, but should not be repeated at high speed:
ui.set('measurementTable', [
    {name:'VOUT',value:3.300,low:3.25,high:3.35,unit:'V'}
]);
```

The widget evaluates limits for its display, but the native Test Executive owns **authoritative test outcomes**, runner states, and archival reports. The Measurement Table is a presentation view and should not be mistaken for a way to change Test Executive verdicts.

### Archived Test Results Browser

Its large step and measurement tables still use `QTableWidget`. The archived report's formatted JSON also loads synchronously. A report near the 100,000-measurement cap could materialize a large number of individual cell items. This should be converted to lazy model/view pages after Phase 1.

## Phase 2 — Experiment DataTable as a streaming data destination

**Goal:** support microcontroller/DAQ samples arriving over native TCP, UDP, serial, or instrument callbacks without rebuilding cells and without overwhelming the GUI thread.

Proposed capabilities (API shape subject to implementation review):

```javascript
// PROPOSED / NOT IMPLEMENTED IN RC1.5.14.4
ui.set('experimentTable', {appendRows: [
    [123.000, 3.298, 22.1],
    [123.010, 3.301, 22.1]
]});
ui.set('experimentTable', {updateCell:{row:100,column:1,value:3.305}});
ui.set('experimentTable', {updateRows:{start:100,rows:[[123.100,3.302,22.2]]}});
ui.set('experimentTable', {clear:true});
```

Recommended implementation: editable `QAbstractTableModel` with typed or string cell values; efficient row append and range updates; bounded rolling retention; stable record IDs for scrolling; batch GUI notifications; explicit overflow counters/backpressure. Preserve `ui.set(id, fullRows)` and `ui.get(id)` for existing Panels. Replace whole-table cell-change callbacks with a new granular event, while preserving legacy `valueChanged(rows)` as a compatibility option, ideally opt-in for larger tables.

Unlike an Event Log, the Experiment DataTable must support stable editable row identities, so a separate editable model is preferable to forcing all tables into one model class.

## Phase 3 — Import/export and CSV/Excel workflows

- **CSV import** with preview, UTF-8/BOM handling, headers, column mapping, quoted fields, delimiter detection/selection, validation and useful error reporting. Never run an untrusted CSV as code.
- **CSV export** from *all retained rows* with RFC 4180-style quoting, a spreadsheet-formula injection policy, configurable headers/time column and file-dialog or explicitly permissioned file API.
- Excel compatibility should initially mean reliable CSV import/export, not claiming native `.xlsx` editing. Native `.xlsx` support is a separate decision.
- **Continuous recording to disk** must be distinct from the table's bounded on-screen history. It should not require retaining millions of GUI rows.
- Support CSV or columnar import from experiments, and callbacks for import/export progress or errors without blocking the GUI event loop.

## Phase 4 — Measurement Table / Results Browser tuning

Keep the native Test Executive as the owner of measurement results. Verify per-append latency, large-batch behavior, header auto-sizing and scrolling under 100,000 measurements. Add incremental report browsing and lazy JSON rendering. Avoid unnecessary changes to the accepted Test Executive API or report format.

## Performance and correctness criteria across all table widgets

1. **No per-update O(capacity) work** on hot paths at configured retention limits.
2. Model/view separation; painting only visible cells; stable manual scroll position.
3. Batching high-rate producer updates without silently dropping records; bounded queues and explicit loss metrics.
4. No large synchronous JavaScript/Qt deep-copy operations on every measurement. Ingestion, retention, and persistence are distinct.
5. Native automated tests for append, update, eviction, filtering, editing, CSV round-trip, sorted/filtered views, hidden pages, and stress beyond capacity.
6. Benchmarks for 1,000, 10,000, 50,000, and 100,000 rows with measured GUI stalls, memory growth and transport latency.

**Recommended sequence:** (1) validate Event Log RC1.5.14.4 with the new timer Panel and CSV; (2) implement the editable Experiment DataTable model and incremental JS API; (3) CSV import/export enhancements and a TCP/UDP logging demonstration; (4) Test Results Browser model/view and Measurement Table benchmarks.
