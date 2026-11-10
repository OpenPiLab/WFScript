# WFScript architecture

WFScript is a Qt desktop application centered around four cooperating layers:

1. **Main application shell** — workspaces such as Device, Script Editor, Oscilloscope, and Panel Designer.
2. **Panel Designer / Panel Runtime** — editable `.wfpanel` projects with widgets, pages, and events.
3. **JavaScript environment** — a long-lived `QJSEngine` runtime with UI bridges, example libraries, and instrument objects.
4. **Native bridges and test executive** — Digilent device integration, plotting support, results archiving, and automated test execution.

## Major subsystems

### Script Editor

Provides syntax highlighting, completions, F1 help, diagnostics, and API exploration.

### Panel Designer

Lets users place widgets, edit properties, organize multiple Pages, and bind events to JavaScript.

### Panel Runtime

Runs the panel's shared JavaScript with live widget callbacks and access to `ui`, plots, and instrument bridges.

### Test Executive

Provides native execution of structured test sequences with Setup/Main/Cleanup, limits, run information, archiving, and results browsing.

### Examples system

The built-in examples are embedded as ordinary projects and intended to serve as both demonstrations and practical reference implementations.
