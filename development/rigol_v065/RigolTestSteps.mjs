/** Rigol v0.6.5 — native Test Executive module.
 * Exports only step entry points. Hardware operations are provided by the
 * shared panel-level RigolTestStation adapter, which reuses the verified v0.6.4
 * Rigol driver instances and serialized runP ownership.
 * No TestRunner.configure/add/loadDefinition() is used here.
 */
export function pollDone() { return RigolTestStation.pollAction(); }
export function pollPreflight() {
    var result=RigolTestStation.pollAction();
    if(result===true)RigolTestStation.recordHardwareMetadata();
    return result;
}
export function pollValue() { return RigolTestStation.pollNumber(); }
export function preflight() { RigolTestStation.beginPreflight(); }
export function powerOff() { RigolTestStation.beginPowerOff(); }
export function cleanupGen() { RigolTestStation.beginCleanup('gen'); }
export function cleanupPsu() { RigolTestStation.beginCleanup('psu'); }
export function power500() { RigolTestStation.beginPower(5.00); }
export function dmm500() { RigolTestStation.beginDmm(5.00); }
export function power475() { RigolTestStation.beginPower(4.75); }
export function dmm475() { RigolTestStation.beginDmm(4.75); }
export function power450() { RigolTestStation.beginPower(4.50); }
export function dmm450() { RigolTestStation.beginDmm(4.50); }
export function power425() { RigolTestStation.beginPower(4.25); }
export function dmm425() { RigolTestStation.beginDmm(4.25); }
export function power400() { RigolTestStation.beginPower(4.00); }
export function dmm400() { RigolTestStation.beginDmm(4.00); }
export function power375() { RigolTestStation.beginPower(3.75); }
export function dmm375() { RigolTestStation.beginDmm(3.75); }
export function power350() { RigolTestStation.beginPower(3.50); }
export function dmm350() { RigolTestStation.beginDmm(3.50); }
export function power325() { RigolTestStation.beginPower(3.25); }
export function dmm325() { RigolTestStation.beginDmm(3.25); }
export function power300() { RigolTestStation.beginPower(3.00); }
export function dmm300() { RigolTestStation.beginDmm(3.00); }
export function power275() { RigolTestStation.beginPower(2.75); }
export function dmm275() { RigolTestStation.beginDmm(2.75); }
export function power250() { RigolTestStation.beginPower(2.50); }
export function dmm250() { RigolTestStation.beginDmm(2.50); }
export function power225() { RigolTestStation.beginPower(2.25); }
export function dmm225() { RigolTestStation.beginDmm(2.25); }
export function power200() { RigolTestStation.beginPower(2.00); }
export function dmm200() { RigolTestStation.beginDmm(2.00); }
export function capture_sine_1Vpp_1000Hz() { RigolTestStation.beginWave("sine",1,1000,"sine_1Vpp_1000Hz"); }
export function actual_vpp_sine_1Vpp_1000Hz() { return RigolTestStation.metric("sine_1Vpp_1000Hz","vpp"); }
export function actual_hz_sine_1Vpp_1000Hz() { return RigolTestStation.metric("sine_1Vpp_1000Hz","hz"); }
export function actual_shape_sine_1Vpp_1000Hz() { return RigolTestStation.metric("sine_1Vpp_1000Hz","shape"); }
export function capture_sine_1Vpp_5000Hz() { RigolTestStation.beginWave("sine",1,5000,"sine_1Vpp_5000Hz"); }
export function actual_vpp_sine_1Vpp_5000Hz() { return RigolTestStation.metric("sine_1Vpp_5000Hz","vpp"); }
export function actual_hz_sine_1Vpp_5000Hz() { return RigolTestStation.metric("sine_1Vpp_5000Hz","hz"); }
export function actual_shape_sine_1Vpp_5000Hz() { return RigolTestStation.metric("sine_1Vpp_5000Hz","shape"); }
export function capture_sine_2Vpp_1000Hz() { RigolTestStation.beginWave("sine",2,1000,"sine_2Vpp_1000Hz"); }
export function actual_vpp_sine_2Vpp_1000Hz() { return RigolTestStation.metric("sine_2Vpp_1000Hz","vpp"); }
export function actual_hz_sine_2Vpp_1000Hz() { return RigolTestStation.metric("sine_2Vpp_1000Hz","hz"); }
export function actual_shape_sine_2Vpp_1000Hz() { return RigolTestStation.metric("sine_2Vpp_1000Hz","shape"); }
export function capture_sine_2Vpp_5000Hz() { RigolTestStation.beginWave("sine",2,5000,"sine_2Vpp_5000Hz"); }
export function actual_vpp_sine_2Vpp_5000Hz() { return RigolTestStation.metric("sine_2Vpp_5000Hz","vpp"); }
export function actual_hz_sine_2Vpp_5000Hz() { return RigolTestStation.metric("sine_2Vpp_5000Hz","hz"); }
export function actual_shape_sine_2Vpp_5000Hz() { return RigolTestStation.metric("sine_2Vpp_5000Hz","shape"); }
export function capture_square_1Vpp_1000Hz() { RigolTestStation.beginWave("square",1,1000,"square_1Vpp_1000Hz"); }
export function actual_vpp_square_1Vpp_1000Hz() { return RigolTestStation.metric("square_1Vpp_1000Hz","vpp"); }
export function actual_hz_square_1Vpp_1000Hz() { return RigolTestStation.metric("square_1Vpp_1000Hz","hz"); }
export function actual_shape_square_1Vpp_1000Hz() { return RigolTestStation.metric("square_1Vpp_1000Hz","shape"); }
export function capture_square_1Vpp_5000Hz() { RigolTestStation.beginWave("square",1,5000,"square_1Vpp_5000Hz"); }
export function actual_vpp_square_1Vpp_5000Hz() { return RigolTestStation.metric("square_1Vpp_5000Hz","vpp"); }
export function actual_hz_square_1Vpp_5000Hz() { return RigolTestStation.metric("square_1Vpp_5000Hz","hz"); }
export function actual_shape_square_1Vpp_5000Hz() { return RigolTestStation.metric("square_1Vpp_5000Hz","shape"); }
export function capture_square_2Vpp_1000Hz() { RigolTestStation.beginWave("square",2,1000,"square_2Vpp_1000Hz"); }
export function actual_vpp_square_2Vpp_1000Hz() { return RigolTestStation.metric("square_2Vpp_1000Hz","vpp"); }
export function actual_hz_square_2Vpp_1000Hz() { return RigolTestStation.metric("square_2Vpp_1000Hz","hz"); }
export function actual_shape_square_2Vpp_1000Hz() { return RigolTestStation.metric("square_2Vpp_1000Hz","shape"); }
export function capture_square_2Vpp_5000Hz() { RigolTestStation.beginWave("square",2,5000,"square_2Vpp_5000Hz"); }
export function actual_vpp_square_2Vpp_5000Hz() { return RigolTestStation.metric("square_2Vpp_5000Hz","vpp"); }
export function actual_hz_square_2Vpp_5000Hz() { return RigolTestStation.metric("square_2Vpp_5000Hz","hz"); }
export function actual_shape_square_2Vpp_5000Hz() { return RigolTestStation.metric("square_2Vpp_5000Hz","shape"); }
