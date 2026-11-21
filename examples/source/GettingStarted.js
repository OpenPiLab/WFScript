// ┌─ WFScript LEARNING LAB: Getting Started
// │ Level: Beginner
// │
// │ GOAL: Understand a Panel project, Design vs Run, and saved visual event bindings.
// │ BEFORE RUN: No equipment is required; stay in Simulation.
// │ TRY FIRST: Read the Guide Page, then switch to Events Tutorial.
// │ TRACE: `tourButton` → `clicked` → `onTourButtonClicked()` → `ui.set("tourResult", …)`.
// │ EDIT SAFELY: Change the message in `onTourButtonClicked()` to use your own words; rerun
// │    and confirm the counter still works.
// │ REMEMBER: The Panel stores the event-to-function link; the main script provides the
// │    named function. All Pages share the same script.
// │
// │ EVENT TIP: Select `tourButton` in Design mode → Properties → Events → Go to Handler.
// │ Dynamic UI events can use `ui.on` — do NOT duplicate a saved designer binding.
// │ FULL LAB GUIDE: examples/guides/GettingStarted.md (in the complete source ZIP).
// └────────────────────────────────────────────────────────────────────────────

/**
 * WFScript Example: Getting Started
 * Quick tour, editor workflow, simulation, and hardware safety.
 *
 * Device: Simulation by default. Choose Physical Device before Run Panel
 *         for real AD3 operations. No output is enabled merely by opening.
 * Wiring: No connections required.
 *
 * The script belongs to ONE .wfpanel project. If it contains multiple Pages,
 * the Pages share this JavaScript runtime and globally unique widget IDs.
 * Read the SETTINGS / FUNCTIONS / EVENTS sections from top to bottom.
 * Edit this .js file in the source tree, then run:
 *      python tools/build_examples.py
 * to regenerate the packaged .wfpanel resource and keep JS in sync.
 */

// Page 02 teaches Property Editor event management with no manual ui.on().
// In Design mode select tourButton > Properties > Events > clicked and choose
// Go to Handler to jump directly to onTourButtonClicked().
let tutorialClicks = 0;
/**
 * CORE LESSON — Designer-bound click handler. Maintains a counter local to this run, then writes
 * text to tourResult. This is the smallest useful event → JavaScript → widget example.
 */
function onTourButtonClicked() {
    tutorialClicks++;
    ui.set('tourResult', 'The designer-managed handler ran ' + tutorialClicks +
        (tutorialClicks === 1 ? ' time.' : ' times.'));
    print('Getting Started: designer event #' + tutorialClicks);
}
print('Getting Started: choose an example from the Panel Designer Examples menu.');
