/**
 * WFScript Example: V+ / V− Power Supplies
 * Set and enable independent programmable supplies and inspect status.
 *
 * Device: Simulation by default. Choose Physical Device before Run Panel
 *         for real AD3 operations. No output is enabled merely by opening.
 * Wiring: Measure V+/V− relative to AD3 GND; verify the circuit and safe load.
 *
 * The script belongs to ONE .wfpanel project. If it contains multiple Pages,
 * the Pages share this JavaScript runtime and globally unique widget IDs.
 * Read the SETTINGS / FUNCTIONS / EVENTS sections from top to bottom.
 * Edit this .js file in the source tree, then run:
 *      python tools/build_examples.py
 * to regenerate the packaged .wfpanel resource and keep JS in sync.
 */

// 11 SUPPLIES — INDEPENDENT AD3 V+ / V- PROGRAMMABLE POWER
// ============================================================================
// The documented WaveForms nested property names are used below. WFScript
// deliberately stages values until Supplies.run()/config() is called. POWER ON
// is the explicit authorization to energize physical outputs. Hardware status
// is read from AnalogIO; simulation status is clearly labeled as an ideal model.
let psIsOn = false;
function psSettings() {
  const vp=Number(ui.get('psVPlus'));
  const vn=Number(ui.get('psVMinus'));
  const ep=!!ui.get('psEnablePlus');
  const en=!!ui.get('psEnableMinus');
  if(!isFinite(vp) || vp<0.5 || vp>5 || !isFinite(vn) || vn<-5 || vn>-0.5)
    throw Error('AD3 range V+ 0.5..5 V, V- -5..-0.5 V');
  Supplies.Output.PositiveSupply.Voltage.value=vp;
  Supplies.Output.NegativeSupply.Voltage.value=vn;
  Supplies.Output.PositiveSupply.Enable.value=ep?1:0;
  Supplies.Output.NegativeSupply.Enable.value=en?1:0;
  return {vp,vn,ep,en};
}
function psShowStatus(prefix) {
  const s=Supplies.status();
  // Use the bridge state, not just the last button click. An SDK stop failure
  // must never be presented as a confirmed power-off.
  const live=!!s.running;
  psIsOn=live;
  ui.set('psMaster',live?1:0);
  ui.set('psState',live?2:(!s.simulation && !s.measuredAvailable ? 1:0));
  // Do not present an unverified physical value as an actual measurement.
  ui.set('psPlusReadout',s.positiveVoltage==null?0:s.positiveVoltage);
  ui.set('psMinusReadout',s.negativeVoltage==null?0:s.negativeVoltage);
  const readback=s.simulation ? 'SIMULATED ideal voltage' :
      (s.measuredAvailable?'AD3 voltage readback':'AD3 readback unavailable');
  ui.set('psStatus',prefix+' | '+readback+' | V+ '+s.positiveSetpoint.toFixed(2)+
      ' V / V- '+s.negativeSetpoint.toFixed(2)+' V | master '+(live?'ON':'OFF'));
}
function psStart() {
  try {
    const s=psSettings();
    if(!s.ep && !s.en) throw Error('Enable at least one V+ or V- rail before POWER ON');
    if(!Supplies.run()) throw Error(Supplies.lastError);
    psIsOn=true;
    psShowStatus('POWER ON');
  } catch(e) {
    const stopped=Supplies.stop();
    psShowStatus((stopped?'POWER OFF / ERROR: ':'STOP NOT CONFIRMED / ERROR: ')+String(e));
    print('Supplies: '+e);
  }
}
function psStop() {
  const ok=Supplies.stop();
  psShowStatus(ok?'POWER OFF':'STOP NOT CONFIRMED — CHECK HARDWARE: '+Supplies.lastError);
}
function psChanged() {
  try {
    psSettings();
    if(psIsOn) {
      if(!Supplies.config())throw Error(Supplies.lastError);
      psIsOn=Supplies.running;
      psShowStatus('LIVE APPLIED');
    } else psShowStatus('STAGED / POWER OFF');
  } catch(e) {
    if(psIsOn && !Supplies.stop())
      print('WARNING: Supply shutdown not confirmed: '+Supplies.lastError);
    psShowStatus('SETTINGS ERROR: '+String(e));
  }
}
ui.on('psPowerOn','clicked',psStart);
ui.on('psPowerOff','clicked',psStop);
ui.on('psRefresh','clicked',function(){psShowStatus('STATUS REFRESH');});
ui.on('psVPlus','valueChanged',psChanged);
ui.on('psVMinus','valueChanged',psChanged);
ui.on('psEnablePlus','toggled',psChanged);
ui.on('psEnableMinus','toggled',psChanged);
// Initialization is strictly software-only; no physical supply output commands.
psShowStatus('READY / POWER OFF');
