/**
 * Equation & Experiment Tables — hardware-safe, self-contained demonstration.
 * No AD3 output or external modules are used.
 * Equation source is a SUPPORTED LaTeX-style subset, NOT a complete TeX engine.
 * Right-click an Experiment Data Table to export its contents to CSV.
 */
const R=10000;
const C=10e-9;
const sweep=[100,500,1000,1592,2000,5000];
function format(n,d) { return Number(n).toFixed(d); }
function gainAt(f) { return 1/Math.sqrt(1+Math.pow(2*Math.PI*f*R*C,2)); }
function rowsFor(freq) {
    return sweep.map(function(f){
        const predicted=gainAt(f);
        // The measured column is left blank until a real instrument or student supplies a value.
        return [String(f),format(predicted,4),'',''];
    });
}
function populateLab() {
    const f=Number(ui.get('frequencyInput'));
    if (!isFinite(f)) throw new Error('Invalid frequency');
    const omega=2*Math.PI*f;
    const predicted=gainAt(f);
    ui.set('selectedFrequency',f);
    ui.set('selectedGain',predicted);
    ui.set('liveEquation','|H(j\\omega)|=\\frac{1}{\\sqrt{1+(\\omega RC)^2}}');
    ui.set('predictionTable',rowsFor(f));
    ui.set('studentTable',rowsFor(f));
    print('RC lab simulation: '+f+' Hz, predicted gain='+format(predicted,4));
}
ui.on('frequencyInput','valueChanged',populateLab);
ui.on('fillTable','clicked',populateLab);
ui.on('clearTable','clicked',function(){ui.set('predictionTable',[]);ui.set('studentTable',[]);});
ui.on('studentTable','valueChanged',function(rows){
    print('Student edited an observation: '+rows.length+' data rows; edits are runtime values, not saved design defaults.');
});
populateLab();
