# Lab 06 — SQLite Database & Data Logging

**Level:** Beginner–Intermediate\
**Category:** Tables & Data\
**Pages:** 01 Data Logging → 02 Statistics

## What you will build or observe

Persist synthetic readings in SQLite, run queries, and display results.

## Before you run

No instruments. The database is in WFScript’s shared PanelData directory.

## Follow along

1. Press OPEN DATABASE.
2. Press START LOGGING, allow several sample updates, then STOP LOGGING.
3. Press REFRESH RECENT and view the Statistics Page.
4. Close the database when finished.

## Follow the JavaScript

`startSqlLog` → `startLogging()` → `Database.exec()`; query callbacks update table/chart widgets.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| 01 Data Logging | `openSqlDb` (button) | `clicked` | `openDatabase()` |
| 01 Data Logging | `closeSqlDb` (button) | `clicked` | `closeDatabase()` |
| 01 Data Logging | `startSqlLog` (button) | `clicked` | `startLogging()` |
| 01 Data Logging | `stopSqlLog` (button) | `clicked` | `stopLogging()` |
| 01 Data Logging | `querySql` (button) | `clicked` | `refreshRecent()` |
| 01 Data Logging | `filterSql` (button) | `clicked` | `filterHigh()` |
| 02 Statistics | `queryStats` (button) | `clicked` | `refreshStats()` |
| 02 Statistics | `invalidSql` (button) | `clicked` | `demonstrateSafety()` |

## Make one small change

Change the synthetic reading range or a query filter, then refresh the statistics.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

SQL commands execute asynchronously: the result callback, not the line after `query()`, owns the returned rows.

## Safety and scope

Database calls are local; this example does not read AD3 voltage.

## If something does not work

If no rows appear, confirm the database was opened and the asynchronous operation completed before querying.

## Related files

- **Editable project:** `examples/panels/SQLiteDataLab.wfpanel`
- **Readable JavaScript:** `examples/source/SQLiteDataLab.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
