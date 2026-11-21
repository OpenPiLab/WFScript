// ┌─ WFScript LEARNING LAB: Mathematical Equations and Experiment Tables
// │ Level: Beginner–Intermediate
// │
// │ GOAL: Combine a theory Page, a frequency slider and editable experiment tables.
// │ BEFORE RUN: No hardware; equations and predicted measurements are simulated.
// │ TRY FIRST: Examine the Equations & Theory Page.
// │ TRACE: `frequencyInput` → `populateLab()` → table updates; `studentTable` →
// │    `onStudentTableValueChanged(rows)`.
// │ EDIT SAFELY: Edit the mathematical input value or a predicted-data cell and watch the
// │    table update.
// │ REMEMBER: A table can accept operator-entered values and a separate results table can
// │    show calculated values.
// │
// │ EVENT TIP: Select `frequencyInput` in Design mode → Properties → Events → Go to Handler.
// │ Dynamic UI events can use `ui.on` — do NOT duplicate a saved designer binding.
// │ FULL LAB GUIDE: examples/guides/EquationExperimentLab.md (in the complete source ZIP).
// └────────────────────────────────────────────────────────────────────────────

/**
 * Equation & Experiment Tables — hardware-safe, self-contained demonstration.
 * No AD3 output or external modules are used.
 * Equation source is a SUPPORTED LaTeX-style subset, NOT a complete TeX engine.
 * Right-click an Experiment Data Table to export its contents to CSV.
 */
const R = 10000;
const C = 10e-9;
const sweep = [100, 500, 1000, 1592, 2000, 5000];
function format(n, d) {
    return Number(n).toFixed(d);
}
function gainAt(f) {
    return 1 / Math.sqrt(1 + Math.pow(2 * Math.PI * f * R * C, 2));
}
function rowsFor(freq) {
    return sweep.map(function (f) {
        const predicted = gainAt(f);
        // The measured column is left blank until a real instrument or student supplies a value.
        return [String(f), format(predicted, 4), '', ''];
    });
}
/**
 * CORE LESSON — Updates predicted experiment values from the selected frequency. It is
 * deliberately distinct from the student-editable table and physical measurement.
 */
function populateLab() {
    const f = Number(ui.get('frequencyInput'));
    if (!isFinite(f)) throw new Error('Invalid frequency');
    const omega = 2 * Math.PI * f;
    const predicted = gainAt(f);
    ui.set('selectedFrequency', f);
    ui.set('selectedGain', predicted);
    ui.set('liveEquation', '|H(j\\omega)|=\\frac{1}{\\sqrt{1+(\\omega RC)^2}}');
    ui.set('predictionTable', rowsFor(f));
    ui.set('studentTable', rowsFor(f));
    print('RC lab simulation: ' + f + ' Hz, predicted gain=' + format(predicted, 4));
}


function onClearTableClicked() {
    ui.set('predictionTable', []);
    ui.set('studentTable', []);
}
function onStudentTableValueChanged(rows) {
    print('Student edited an observation: ' + rows.length + ' data rows; edits are runtime values, not saved design defaults.');
}
populateLab();
