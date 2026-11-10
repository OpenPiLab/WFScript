/**
 * AD3LoopbackSteps.mjs — REAL W1 -> Scope CH1 instrument measurements.
 * This library runs inside WFScript's ONE QJSEngine. No module imports are
 * necessary; Wavegen1, Scope1, Device, plots, print are native bindings.
 * The visual Test Sequence owns limit checks and execution order.
 *
 * SAFE WIRING: W1 -> CH1+; AD3 GND -> CH1-. Nothing else connected.
 * Physical operation is explicit: select Physical Device BEFORE Run Panel.
 * The Panel script does not start outputs on load.
 */
let capture = null;

function requireCaptured(key) {
    if (capture === null) throw Error('No CH1 capture available; capture step must run first');
    const v = Number(capture[key]);
    if (!isFinite(v)) throw Error('Invalid CH1 ' + key + ' measurement');
    return v;
}

export function configureLoopback() {
    capture = null;
    if (!Device.isConnected()) throw Error('No AD3 connected or simulator available');
    // Turn off any previous W1/W2 waveform before configuring.
    Wavegen1.stop();
    Wavegen1.Channel1.enable = true;
    Wavegen1.Channel2.enable = false;
    Wavegen1.Channel1.Mode.text = 'Simple';
    Wavegen1.Channel1.Simple.Type.text = 'Sine';
    Wavegen1.Channel1.Simple.Frequency.value = 1000;
    Wavegen1.Channel1.Simple.Amplitude.value = 1.0; // V peak, 2 Vpp
    Wavegen1.Channel1.Simple.Offset.value = 0;
    Wavegen1.Channel1.Simple.Phase.value = 0;
    if (Wavegen1.Channel1.Simple.Type.text !== 'Sine')
        throw Error('Wavegen rejected Sine function');
    Scope1.Time.Rate.value = 1000000; // 1 MS/s
    Scope1.Time.Samples.value = 16384;
    Scope1.Channel1.Range.value = 5;  // supported AD3 hardware 5 Vpp range
    Scope1.Channel1.Offset.value = 0;
    Scope1.Channel1.Attenuation.value = 1;
    Scope1.Trigger.Trigger.text = 'None';
    if (!Wavegen1.run()) throw Error('W1 start failed: ' + Wavegen1.lastError);
    const w = Wavegen1.status(1);
    if (w && w.carrierFunction && w.carrierFunction !== 'Sine') {
        Wavegen1.stop();
        throw Error('Unexpected W1 function: ' + w.carrierFunction);
    }
    print('[AD3 test] W1 1 kHz sine, 1.0 Vpk, CH1 5 Vpp range; 350 ms settle next');
    return true;
}

export function captureAcRms() {
    capture = null;
    if (!Scope1.single()) throw Error('Scope start failed: ' + Scope1.lastError);
    if (!Scope1.wait(3)) throw Error('Scope acquisition failed: ' + Scope1.lastError);
    const st = Scope1.Channel1.statistics();
    if (!st || Number(st.count) < 2048)
        throw Error('Insufficient CH1 samples: ' + (st ? st.count : 0));
    const mean = Number(st.mean), rms = Number(st.rms);
    const frequency = Number(st.frequencyHz);
    const vpp = Number(st.vpp);
    if (![mean, rms, frequency, vpp].every(isFinite))
        throw Error('Non-finite scope statistics (check W1 -> CH1 wiring)');
    // Ignore DC offset when checking AC RMS, unlike total RMS.
    const acRms = Math.sqrt(Math.max(0, rms * rms - mean * mean));
    capture = {acRms:acRms,frequency:frequency,vpp:vpp,mean:mean, count:Number(st.count)};
    print('[AD3 test] CH1 N=' + capture.count + ', AC RMS=' + acRms.toFixed(4) +
          ' V, f=' + frequency.toFixed(2) + ' Hz, Vpp=' + vpp.toFixed(4) +
          ' V, DC=' + mean.toFixed(4) + ' V');
    // Plot is an optional visualization: an invalid plot must not turn a
    // valid measurement into a failed test.
    try { plots.fromScope('hwTrace', 1, 'Last captured CH1'); }
    catch (e) { print('[AD3 test] Chart not updated: ' + String(e)); }
    return acRms;
}

export function measureFrequency() { return requireCaptured('frequency'); }
export function measureVpp()       { return requireCaptured('vpp'); }
export function measureMean()      { return requireCaptured('mean'); }

export function cleanupLoopback() {
    // Always attempt BOTH shutdown operations. Even a Scope error must not
    // prevent W1 shutdown. Best effort hardware-off is more important than UI.
    let scopeError = null;
    try { Scope1.stop(); } catch (e) { scopeError = String(e); }
    try { Wavegen1.stop(); }
    catch (e) { throw Error('W1 could not be stopped: ' + String(e)); }
    print('[AD3 test] Cleanup: Scope stopped; W1 disabled' +
          (scopeError ? ' (scope warning: '+scopeError+')' : ''));
    return true;
}
