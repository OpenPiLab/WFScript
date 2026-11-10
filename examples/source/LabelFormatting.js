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
ui.on('demoUpdate', 'clicked', function () {
    // This Label is configured as Rich Text in its design properties.
    // QLabel interprets supported HTML tags; this is not a browser engine.
    ui.set('demoLive', message);
    print('Updated a Rich Text label from JavaScript.');
});
