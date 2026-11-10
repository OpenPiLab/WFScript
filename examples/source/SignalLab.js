/**
 * WFScript Example: Signal Lab — Wavegen and Scope
 * Control W1, preview waveforms, and capture Scope CH1.
 *
 * Device: Simulation by default. Choose Physical Device before Run Panel
 *         for real AD3 operations. No output is enabled merely by opening.
 * Wiring: W1 to Scope CH1+, AD3 GND to CH1− for the loopback.
 *
 * The script belongs to ONE .wfpanel project. If it contains multiple Pages,
 * the Pages share this JavaScript runtime and globally unique widget IDs.
 * Read the SETTINGS / FUNCTIONS / EVENTS sections from top to bottom.
 * Edit this .js file in the source tree, then run:
 *      python tools/build_examples.py
 * to regenerate the packaged .wfpanel resource and keep JS in sync.
 */

// 02 SIGNAL LAB — INSTRUMENT CONTROLS, PREVIEW, AND EXPLICIT AD3 OPERATIONS
// ============================================================================
// Selecting Device > Physical Device (before Run Panel) causes the actions
// below to operate on real hardware. The UI never starts W1 automatically.
// Preview is pure math and never starts W1. After explicit Start W1, editing
// the shape or setpoints updates an already-running hardware output.
//
// SAFETY: Connect W1 to CH1+ and GND to CH1- for the basic safe loopback.
// Verify the circuit and output voltage limits BEFORE clicking Start W1.
// Scope CH1 acquisition is independent: Capture does not start the generator.
function sigValue(id) {
  const n=Number(ui.get(id));
  if(!isFinite(n)) throw new Error('Invalid value for '+id);
  return n;
}
function sigSettings() {
  const shape=String(ui.get('sigShape'));
  const hz=sigValue('sigFrequency');
  const amplitude=sigValue('sigAmplitude'); // amplitude = V PEAK, not Vpp
  const offset=sigValue('sigOffset');
  if(['Sine','Square','Triangle'].indexOf(shape)<0)throw Error('Unsupported shape');
  if(hz<100 || hz>5000 || amplitude<0.1 || amplitude>2 ||
     offset< -1 || offset>1 || Math.abs(offset)+amplitude>3)
    throw Error('W1 settings outside conservative showcase limits');
  return {shape:shape,hz:hz,amplitude:amplitude,offset:offset};
}
function sigSample(shape,phase) {
  if(shape==='Square')return Math.sin(phase)>=0?1:-1;
  if(shape==='Triangle')return (2/Math.PI)*Math.asin(Math.sin(phase));
  return Math.sin(phase);
}
function sigPreviewWaveform() {
  const s=sigSettings();
  const x=[], y=[];
  const duration=4/s.hz; // show four waveform periods at every frequency
  for(let i=0;i<401;i++) {
    const t=i*duration/400;
    x.push(t);
    y.push(s.offset+s.amplitude*sigSample(s.shape,2*Math.PI*s.hz*t));
  }
  plots.clear('sigPlot');
  plots.setSeries('sigPlot','W1 preview (math only)',x,y,{color:'#49c9e4',width:2.3});
  ui.set('sigStatus','PREVIEW ONLY  |  '+s.shape+'  •  '+s.hz.toFixed(0)+
     ' Hz  •  '+s.amplitude.toFixed(2)+' Vpk  •  offset '+s.offset.toFixed(2)+' V');
}
// Script-side lifecycle: W1 is never started by editing a setting.
// Once the operator explicitly presses Start W1, subsequent edits are applied
// live to the active output. Stop W1 disables live configuration again.
let sigW1Running=false;
function sigStartW1() {
  const s=sigSettings();
  // Calling run() actually enables W1 in Physical Device mode. Only here!
  Wavegen1.Channel1.enable=true;
  Wavegen1.Channel1.Mode.text='Simple';
  Wavegen1.Channel1.Simple.Type.text=s.shape;
  Wavegen1.Channel1.Simple.Frequency.value=s.hz;
  Wavegen1.Channel1.Simple.Amplitude.value=s.amplitude;
  Wavegen1.Channel1.Simple.Offset.value=s.offset;
  // Confirm the QJSEngine property mapping before sending the command.
  // A setter rejecting the text must not silently leave Sine selected.
  if(Wavegen1.Channel1.Simple.Type.text!==s.shape)
    throw Error('Wavegen API rejected waveform type '+s.shape);
  if(!Wavegen1.run())throw Error(Wavegen1.lastError);
  sigW1Running=true;
  // The native adapter optionally reports the function returned by the SDK's
  // FDwfAnalogOutNodeFunctionGet. It is absent on older runtimes/simulators.
  // If present, warn and disable W1 rather than claim the wrong output is safe.
  const state=Wavegen1.status(1);
  if(state && state.carrierFunction && state.carrierFunction!==s.shape) {
    Wavegen1.stop();
    sigW1Running=false;
    throw Error('AD3 reports '+state.carrierFunction+' on W1, expected '+s.shape+'; output stopped');
  }
  const verified=state && state.carrierFunction ? ' [SDK: '+state.carrierFunction+']' : '';
  ui.set('sigStatus',(Device.SN==='DEMO'?'SIMULATED':'PHYSICAL')+' W1 ACTIVE  |  '+s.shape+' '+s.hz.toFixed(0)+
     ' Hz, '+s.amplitude.toFixed(2)+' Vpk'+verified+'  |  changes apply live; Stop W1 disables output');
}
function sigStopW1() {
  Wavegen1.stop();
  sigW1Running=false;
  ui.set('sigStatus',(Device.SN==='DEMO'?'SIMULATED':'PHYSICAL')+' W1 STOPPED  |  output disabled; settings are preview-only');
}
function sigCaptureCH1() {
  const rateLabel=String(ui.get('sigRate'));
  const sampleRate={'250 kS/s':250000,'1 MS/s':1000000,'2 MS/s':2000000}[rateLabel];
  const sampleCount=sigValue('sigSamples');
  const rangeValue=parseFloat(String(ui.get('sigRange')));
  if(!sampleRate || [2048,4096,8192].indexOf(sampleCount)<0 ||
     !(rangeValue>0 && rangeValue<=50))throw Error('Invalid Scope settings');
  // Capture is one-shot and DOES NOT start or modify W1.
  Scope1.Time.Samples.value=sampleCount;
  Scope1.Time.Rate.value=sampleRate;
  Scope1.Channel1.Range.value=rangeValue;
  Scope1.Trigger.Trigger.text='None';
  if(!Scope1.single())throw Error(Scope1.lastError);
  if(!Scope1.wait(3))throw Error(Scope1.lastError);
  plots.clear('sigPlot');
  plots.fromScope('sigPlot',1,'CH1 captured');
  const m=Scope1.Channel1.statistics();
  ui.set('sigRms',m.rms);
  ui.set('sigStatus',(Device.SN==='DEMO'?'SIMULATED':'PHYSICAL')+' CH1 CAPTURE  |  '+m.count+
     ' samples  •  DC '+Number(m.mean).toFixed(4)+' V  •  RMS '+Number(m.rms).toFixed(4)+
     ' V  •  Range '+rangeValue+' -> '+Number(Scope1.Channel1.Range.real).toFixed(3)+' Vpp');
}
// This wrapper reports errors in the Panel, rather than losing the callback.
function sigAction(id,handler) {
  ui.on(id,'clicked',function(){
    try {handler();print('[Signal Lab] '+id+' succeeded');}
    catch(e) {ui.set('sigStatus','ERROR: '+String(e)); print('[Signal Lab] '+id+': '+String(e));}
  });
}
sigAction('sigPreview',sigPreviewWaveform);
sigAction('sigStart',sigStartW1);
sigAction('sigStop',sigStopW1);
sigAction('sigCapture',sigCaptureCH1);
// Input changes always update the preview. They modify real W1 *only* after
// the user has explicitly started the output. No other page starts W1.
function sigInputsChanged() {
  try {
    sigPreviewWaveform();
    if(sigW1Running) sigStartW1(); // SDK commits new waveform + re-arms W1
  } catch(e) {
    // If a reconfiguration fails, never silently leave an unexpected output
    // active at the old waveform. Fail safe to W1 OFF and report the cause.
    if(sigW1Running) {
      Wavegen1.stop();
      sigW1Running=false;
    }
    ui.set('sigStatus','W1 SETTINGS ERROR / OUTPUT STOPPED: '+String(e));
    print('[Signal Lab] Reconfigure failed (W1 stopped): '+String(e));
  }
}
for(const id of ['sigFrequency','sigAmplitude','sigOffset'])
  ui.on(id,'valueChanged',sigInputsChanged);
ui.on('sigShape','selectionChanged',sigInputsChanged);

// Startup only calculates a software waveform preview; never starts W1.
try { sigPreviewWaveform(); } catch (e) { print('Signal Lab: '+String(e)); }
