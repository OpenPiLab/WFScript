/**
 * WFScript Text Input Lab — no instruments, no network connections.
 *
 * Event bindings are saved in the Panel Designer. The status labels and
 * buttons below illustrate the difference between accepted edits, invalid
 * IPv4 text, and programmatic ui.set()/Data.set() updates.
 */
function textStatus(message) {
    ui.set('inputStatus', message);
    print('[TEXT INPUT LAB] ' + message);
}

function onOperatorNameChanged(text) {
    textStatus('Accepted name: ' + text);
}
function onOperatorNameFinished(text) {
    textStatus('Name committed: ' + text);
}
function onScopeAddressChanged(text) {
    textStatus('Valid IPv4: ' + text);
}
function onScopeAddressFinished(text) {
    textStatus('IPv4 committed: ' + text);
}
function onScopeAddressInvalid(text) {
    textStatus('Invalid IPv4 (not published): ' + text);
}
function onViewInputs() {
    textStatus('Name=' + ui.get('operatorName') + ' | IPv4=' + ui.get('scopeIp'));
}
function onSetExamples() {
    // Programmatic changes update the native field without firing user events.
    ui.set('operatorName', 'Bench Operator');
    ui.set('scopeIp', '192.168.6.2');
    textStatus('Script updated two controls using ui.set().');
}
function onClearName() {
    ui.set('operatorName', '');
    textStatus('Name cleared from JavaScript.');
}
function onPanelDataDemo() {
    Data.set('Bench.Alias', 'Oscilloscope A');
    Data.set('Bench.ScopeIp', '192.168.6.2');
    ui.set('dataStatus', 'Data.set updated both linked input pairs.');
}
function onReadDataDemo() {
    ui.set('dataStatus',
        'Alias: ' + Data.get('Bench.Alias') + ' | IP: ' + Data.get('Bench.ScopeIp'));
}
function onDataAliasChanged(text) {
    ui.set('dataStatus', 'Linked alias changed: ' + text);
}
function onDataIpChanged(text) {
    ui.set('dataStatus', 'Linked IPv4 changed: ' + text);
}
function onDataInvalid(text) {
    ui.set('dataStatus', 'Rejected address: ' + text);
}

textStatus('READY — edit an input or use the buttons.');
ui.set('dataStatus', 'Edit either linked field; both copies share Panel Data.');
