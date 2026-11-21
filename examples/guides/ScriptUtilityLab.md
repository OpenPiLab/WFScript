# Lab 05 — File I/O & Operator Dialogs

**Level:** Beginner\
**Category:** Getting Started\
**Pages:** 01 Data Logging → 02 Operator Dialogs

## What you will build or observe

Write and read CSV, JSON and binary data; use asynchronous operator dialogs.

## Before you run

No connected device. FileIO writes only to the application-managed data directory unless a user explicitly grants another file.

## Follow along

1. On Data Logging, press START LOGGING and then STOP LOGGING.
2. Use READ CSV and SAVE/READ JSON CONFIG to inspect stored data.
3. On Operator Dialogs, try CONFIRM and TEXT INPUT.

## Follow the JavaScript

`startLog` → `onStartLogClicked()` → timer/file work; dialog buttons → `Tool` callbacks → label updates.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| 01 Data Logging | `startLog` (button) | `clicked` | `onStartLogClicked()` |
| 01 Data Logging | `stopLog` (button) | `clicked` | `stopLogging()` |
| 01 Data Logging | `saveConfig` (button) | `clicked` | `onSaveConfigClicked()` |
| 01 Data Logging | `readConfig` (button) | `clicked` | `onReadConfigClicked()` |
| 01 Data Logging | `readCsv` (button) | `clicked` | `onReadCsvClicked()` |
| 01 Data Logging | `writeBinary` (button) | `clicked` | `onWriteBinaryClicked()` |
| 01 Data Logging | `exportCsv` (button) | `clicked` | `onExportCsvClicked()` |
| 01 Data Logging | `importCsv` (button) | `clicked` | `onImportCsvClicked()` |
| 02 Operator Dialogs | `askConfirm` (button) | `clicked` | `onAskConfirmClicked()` |
| 02 Operator Dialogs | `askText` (button) | `clicked` | `onAskTextClicked()` |
| 02 Operator Dialogs | `askNumber` (button) | `clicked` | `onAskNumberClicked()` |
| 02 Operator Dialogs | `askItem` (button) | `clicked` | `onAskItemClicked()` |
| 02 Operator Dialogs | `askFileOpen` (button) | `clicked` | `onAskFileOpenClicked()` |
| 02 Operator Dialogs | `askFileSave` (button) | `clicked` | `onAskFileSaveClicked()` |

## Make one small change

Change one field in the saved JSON configuration and inspect the value after reading it back.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

Dialog results arrive through callbacks; saving a file and asking an operator are separate operations.

## Safety and scope

Avoid hardcoded paths outside the FileIO sandbox. Do not assume an operator accepted a dialog.

## If something does not work

If a file appears missing, use File → Open Shared Panel Data Folder and inspect the application-managed directory.

## Related files

- **Editable project:** `examples/panels/ScriptUtilityLab.wfpanel`
- **Readable JavaScript:** `examples/source/ScriptUtilityLab.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
