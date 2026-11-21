# SQLite in WFScript v0.2-dev.5.1

This development feature adds an **optional**, Panel-owned SQLite database and the
`Database` JavaScript API. It uses Qt `Qt6::Sql` and the `QSQLITE` plugin. It does
**not** change the existing `.wfpanel` schema or native JSON TestRunner report archive.

## What starts when?

- WFScript creates a lightweight `SqliteBridge` with the Panel runtime, but
  **does not create a SQL thread, database file, or connection** until the script
  requests a `Database.*` operation.
- All Qt SQL connections are created **and used** exclusively on the new SQL
  worker thread. Requests are serialized; responses return to the existing
  PanelRuntime/QJSEngine thread by queued Qt signals.
- Every operation completes via a callback `{ok, operation, error?, requestId,
  ...}`. An accepted asynchronous request returns a positive request ID; it
  is not a database handle or proof that the SQL succeeded.
- No SQL callback executes after the Panel starts its shutdown cleanup. The
  worker closes its database connections on its own thread, before deletion.
- The AD3 DWF SDK, Scope, Wavegen, Logic, Patterns, StaticIO and native
  TestRunner are **not** modified.

## Basic workflow

```javascript
let db = 0;

Database.open('experiment.db', function(r) {
    if (!r.ok) { print('Open failed:', r.error); return; }
    db = r.db; // This is a connection handle for this Panel run only.
    Database.exec(db, 'CREATE TABLE IF NOT EXISTS points (time TEXT, volts REAL)', [], function(c) {
        if (!c.ok) { print(c.error); return; }
        Database.exec(db, 'INSERT INTO points(time,volts) VALUES (?,?)',
                      [new Date().toISOString(), 3.301], function(w) {
            if (!w.ok) { print(w.error); return; }
            Database.query(db, 'SELECT time,volts FROM points ORDER BY time DESC LIMIT 20', [], function(q) {
                if (!q.ok) { print(q.error); return; }
                // q.rows: actual JS arrays of arrays, q.columns: header strings.
                ui.set('recentSamplesTable', q.rows);
            });
        });
    });
});

// When done (or automatically when Panel stops):
Database.close(db, function(r) { if (!r.ok) print(r.error); });
```

`exec(db, sql, callback)` and `query(db, sql, callback)` are also supported:
when the parameter array is omitted, the callback may be the third argument.

## API and results

| Method | Success result | Notes |
|---|---|---|
| `Database.open(name, cb)` | `{ok:true,db,path,...}` | Creates if absent; shared PanelData directory |
| `Database.exec(db,sql,params,cb)` | `{ok:true,changes,insertId,...}` | Prepared parameter binding; schema and writes |
| `Database.query(db,sql,params,cb)` | `{ok:true,columns,rows,rowCount,...}` | SELECT and bounded result, arrays of arrays |
| `Database.close(db,cb)` | `{ok:true,db,...}` | Closes connection, preserves file |

`Database.exec` and `Database.query` return `ok:false` to their callbacks
for SQL errors. Invalid JavaScript invocations (e.g., missing callback) throw
immediately. Do **not** assume success based solely on the positive request ID.

## Access and limits (v1)

- Database names are simple ASCII filenames, optionally `.db` or `.sqlite`;
  they cannot contain a directory component. All trusted Panels access the same
  `QStandardPaths::AppDataLocation/PanelData` directory used by `FileIO.dataPath()`.
  Renaming or moving a `.wfpanel` does not move its data or change database paths.
  **Different Panels opening the same database name access the same SQLite file.**
  SQLite still permits one writer at a time; handle BUSY errors and avoid long writes.
  For another database use a distinct name; no automatic per-project isolation applies.
- `FileIO.dataDirectory()` reports the shared storage folder. The desktop File
  menu has **Open Shared Panel Data Folder**. Legacy hashed directories are
  deliberately not migrated or deleted by this revision; see
  `docs/SHARED_PANEL_DATA_V02_DEV5_1.md` for safe manual migration.
- No arbitrary file database paths or SQLite `ATTACH`/`DETACH`/`VACUUM`/
  `PRAGMA`, triggers, virtual tables or multiple statements. These restrictions
  prevent the SQL API becoming an alternate route around Panel file permissions.
- `Database.exec` accepts ordinary CREATE/DROP/ALTER TABLE or INDEX operations
  and INSERT/UPDATE/DELETE. `Database.query` accepts SELECT.
- Maximum 4 open database handles, 64 queued callbacks, 64 SQL parameters,
  8,192 SQL characters, 250 returned rows, 16 columns, 4,096 characters per
  cell and 512 Ki characters per returned result. Use WHERE and LIMIT.
- SQL parameter values are strings, finite numbers, booleans or null. BLOB
  results are not supported in this first revision. SQLite integers beyond
  JavaScript's precise integer range are returned as decimal strings.
- `QSQLITE_BUSY_TIMEOUT=750` (ms) and WAL journal mode are enabled on open.
  SQLite permits only one writer at a time, and this isn't a substitute for
  multi-user database infrastructure. SQLite WAL may create `-wal` and `-shm`
  companion files while in use; don't delete these manually.
- SQL is dispatched asynchronously, but **there is not yet a hard query
  execution-time limit**. Avoid expensive joins/large scans, especially when
  closing a Panel. The bounded output is not a bound on SQL computation time.
- This is not a general-purpose SQL administration console; advanced SQL,
  transactions/batch APIs, blob support, external databases, and query model
  views are potential later enhancements.

## Displaying SQLite rows

WFScript already ships an **Experiment Data Table** widget (`experiment_table`)
that accepts a nested array of row arrays using `ui.set()`. Define the widget's
Column headers in the Property Editor, matching the columns selected in SQL;
maximum 8 columns and 250 configured rows. The example uses 3 and 2 columns.
Do not use the Measurement Table for generic SQL results; it expects named
measurement result objects and 4 fixed test-specific columns.

## Qt build and portable distribution

CMake requires the Qt `Sql` component and links `Qt6::Sql`. For portable
Windows builds, include the compatible Qt SQL SQLite driver DLL at:

`<application directory>/sqldrivers/qsqlite.dll`

Check that Windows `windeployqt` has included it in the actual deployment. A
missing plugin produces a diagnostic callback: `Qt QSQLITE driver unavailable`.
No separate database server, Python module or OpenTAP runtime is needed.

## Test plan

1. Build in Windows Qt 6.9.2 MinGW Debug and Release; use the same matching Qt SQL module.
2. Run `ctest --output-on-failure` with `-DWFSCRIPT_BUILD_TESTS=ON` (now 72 registered tests).
3. Open **SQLite Database & Data Logging** in Simulation and press **OPEN DATABASE**.
4. Start 250 ms logging, stop after several samples, press REFRESH RECENT.
5. Visit Page 02, press REFRESH STATISTICS; count/min/avg/max must appear.
6. Use FILTER BY VOLTAGE, press TEST BLOCKED SQL and verify expected error.
7. CLOSE DATABASE, Stop Panel, Run Panel again, OPEN DATABASE. Prior samples
   must remain. No database should be opened until the button is pressed.
8. Repeat basic Scope CH1 + Wavegen W1 loopback and StaticIO input monitoring
   while logging to check that disk traffic does not disturb validated AD3
   workflows; these coexistence results must be user verified.

**Native compilation, 72 CTests, portable plugin deployment, Windows GUI and
physical AD3 behavior have not been performed in the assistant environment.**
