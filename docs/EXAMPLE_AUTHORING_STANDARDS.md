# WFScript Example Authoring Standards

These standards apply when adding or updating an example in the built-in Examples menu. They are meant to make projects **easy to learn, easy to modify and safe to run**, without reducing what an advanced application can demonstrate.

> **Readability standard (v0.2-dev.8.9.4):** First-party example JavaScript is teaching material. Follow [JAVASCRIPT_EDUCATIONAL_STYLE.md](JAVASCRIPT_EDUCATIONAL_STYLE.md) for consistent indentation, meaningful names, mathematical comments, provenance, and QJSEngine-compatible syntax. Keep new code uncluttered and put one operation per line. The v8.9.4 baseline reformats without changing executable tokens; intentional later renaming and refactoring must be independently reviewed and tested.

## 1. Put the learning task first

A beginner should be able to answer these questions by reading the first screen of the script:

1. **What does this Panel do?** State the observable outcome.
2. **Can I run it without hardware?** State Simulation and physical prerequisites.
3. **What do I click first?** Name the real control caption and the relevant Page.
4. **Where is the code?** Name the saved widget ID, event, and handler.
5. **What single change can I safely make?** Suggest one observable modification.
6. **What are the limitations?** Label synthetic data and hardware safety requirements.

The `examples/source/*.js` file must include a short self-contained learning banner; the companion `examples/guides/*.md` must include the full, specific lab walkthrough.

## 2. Event handling: prefer the designer

For a fixed control, select the widget and set **Properties → Events → handler**, then define a named global function:

```javascript
// Designer metadata: calculateButton.clicked -> onCalculateClicked
function onCalculateClicked() {
    const value = Number(ui.get('userInput'));
    ui.set('displayResult', value * 2); // simulated example
}
```

Do **not** call `ui.on('calculateButton', 'clicked', onCalculateClicked)` for the same event. Doing both can call the handler twice. `ui.on()` is still valid for genuinely dynamic groups of controls, as demonstrated by the 16 selectors in Static Digital I/O.

Native events have different ownership: `Serial.on('data')`, `TCP.on('data')`, `UDP.on('message')`, `StaticIO.onChange` and native TestRunner completion callbacks remain in JavaScript. Timers are asynchronous callbacks too.

## 3. Organize by responsibility, not by widget creation order

Use meaningful sections when they are actually needed:

- **Configuration / constants:** Known ranges, user-editable defaults and units.
- **Pure calculations:** Parse, validate, calculate; do not call hardware if avoidable.
- **Native operations:** Connect, acquire, transmit, configure, receive, log.
- **UI presentation:** Convert state/results into labels, charts, tables and status.
- **Designer event handlers:** Small named functions that call the above helpers.
- **Lifecycle:** `onInitialize` may show read-only status. `onShutdown` must clean up timers, sockets and outputs owned by the Panel.

Do not create artificial abstractions just to match a template. Small Panels may use only one named event handler.

## 4. Make async state explicit

Database queries, native communication callbacks and timers do not complete synchronously just because a function returned. Keep a clear lifecycle and handle failures:

- Start only once; hold a timer/connection handle.
- Display connection or logging state.
- Check returned data and errors.
- Stop timers and release resources when no longer used.
- Treat queued writes/accepted bytes as **not proof that the remote peer received them**.

## 5. Hardware safety and scientific honesty

- Use an explicit **START**, **POWER ON**, **CONFIGURE** or **TEST** action before energizing an output.
- Keep simulation and actual hardware measurements visibly separate.
- State electrical limits and the exact pin/wiring prerequisites.
- Validate ranges before configuring hardware.
- Use a best-effort safe stop on exceptions and an explicit shutdown path.
- An unsuccessful stop is **not** a confirmed safe state; show the error.
- Never show a requested setpoint as if it were a measured voltage.
- Do not label simulated traffic as a physical ACK or hardware PASS.
- Native digital input monitoring is not an emergency-stop safety circuit.

## 6. The Test Executive is project-owned

When using TestRunner, the **Test Sequence Editor** owns groups, order, timeouts, limits, Run Information and report storage. Exported functions in `.mjs` modules perform individual measurements or actions. The Panel JavaScript only controls operator actions, status and report display. Do not silently duplicate native configuration in the UI script.

## 7. Verify and publish

For edits to an existing Panel, keep `examples/source/ExampleName.js` synchronized with its embedded `.wfpanel` script by running:

```powershell
python tools/build_examples.py
python tools/build_examples.py --check
python tests/example_curriculum_check.py
```

The curriculum check protects the v0.2-dev.7.5 tested baseline. **For intentional future logic changes** you must evaluate the changed behavior, extend appropriate mocks/native tests and explicitly update the baseline lock after review—never bypass a failed lock simply to make a test green.

Verify on Windows Qt before treating a new C++ or widget behavior as working. Physical AD3/COM tests require separate hardware confirmation. If a documentation-only edit adds a claim about an API, validate it against the current script and native bridge first.
