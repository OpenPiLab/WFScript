// ┌─ WFScript LEARNING LAB: Multiline and Rich Text Labels
// │ Level: Beginner
// │
// │ GOAL: Use plain multiline text and formatted HTML in Label widgets.
// │ BEFORE RUN: No hardware or files required.
// │ TRY FIRST: Run the Panel and click UPDATE RICH TEXT LABEL.
// │ TRACE: `demoUpdate` → `onDemoUpdateClicked()` → `ui.set()` updates the rendered label.
// │ EDIT SAFELY: Change an HTML `<b>` fragment or a line break in the script and observe the
// │    difference.
// │ REMEMBER: Labels can be static instructional material or dynamic status displays; HTML
// │    needs escaping for untrusted data.
// │
// │ EVENT TIP: Select `demoUpdate` in Design mode → Properties → Events → Go to Handler.
// │ Dynamic UI events can use `ui.on` — do NOT duplicate a saved designer binding.
// │ FULL LAB GUIDE: examples/guides/LabelFormatting.md (in the complete source ZIP).
// └────────────────────────────────────────────────────────────────────────────

/**
 * WFScript Example: Multiline and Rich Text Labels
 *
 * Demonstrates:
 *   - Enter creates newlines in the Label Property Inspector.
 *   - Automatic retains legacy QLabel auto-detection.
 *   - Plain Text displays literal <b>tags</b> without rendering them.
 *   - Rich Text renders a subset of HTML, including <b>, <i>, <br>,
 *     <span style="color:..."> and basic text sizing.
 *   - Word Wrap and runtime ui.set() work on the same Label widget.
 *
 * Device: Simulation. No AD3 or WaveForms DLL required.
 * Wiring: None. This example never commands hardware outputs.
 *
 * To edit a Label: Stop/Design, select a Label, open Properties,
 * change Text Format / Word Wrap as needed, then edit Text in the
 * multiline source editor. Click elsewhere to commit; save the Panel.
 */

// SETTINGS ---------------------------------------------------------------
const message = [
    '<b>TEST RESULT</b><br>',
    '<span style="color:#36bd83; font-size:16pt;">PASS</span><br>',
    '<i>Updated from JavaScript via ui.set()</i>'
].join('');

// EVENTS -----------------------------------------------------------------
/**
 * CORE LESSON — Uses ui.set() on a formatted Label. Treat text originating outside the script as
 * untrusted and escape it before including it in rich HTML.
 */
function onDemoUpdateClicked() {
    // This Label is configured as Rich Text in its design properties.
    // QLabel interprets supported HTML tags; this is not a browser engine.
    ui.set('demoLive', message);
    print('Updated a Rich Text label from JavaScript.');
}
