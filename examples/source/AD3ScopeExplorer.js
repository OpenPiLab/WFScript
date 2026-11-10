/**
 * WFScript Example: AD3 Scope and Range Discovery
 * Capture CH1, measure signed DC/RMS, query supported SDK input ranges.
 *
 * Device: Simulation by default. Choose Physical Device before Run Panel
 *         for real AD3 operations. No output is enabled merely by opening.
 * Wiring: For supply loopback, V+ to CH1+, GND to CH1−; select a safe scope range.
 *
 * The script belongs to ONE .wfpanel project. If it contains multiple Pages,
 * the Pages share this JavaScript runtime and globally unique widget IDs.
 * Read the SETTINGS / FUNCTIONS / EVENTS sections from top to bottom.
 * Edit this .js file in the source tree, then run:
 *      python tools/build_examples.py
 * to regenerate the packaged .wfpanel resource and keep JS in sync.
 */

let captureN=0; // count successive physical acquisitions

// Explicit hardware use only. No generated output, no implicit acquisition.
ui.on('ad3Capture','clicked',function(){
 try{
   Scope1.Time.Samples.value=4096;
   Scope1.Time.Rate.value=1000000;
   // 10 Vpp covers the +3.5 V supply test that clipped at 5 Vpp.
   Scope1.Channel1.Range.value=10;
   if(!Scope1.single())throw Error(Scope1.lastError);
   if(!Scope1.wait(3))throw Error(Scope1.lastError);
   plots.fromScope('ad3Scope',1,'Captured CH1');
   const st=Scope1.Channel1.statistics();
   plots.appendPoint('ad3Trend','RMS',captureN++,st.rms);
   ui.set('ad3Status','CH1 DC '+st.mean.toFixed(4)+' V | RMS '+st.rms.toFixed(4)+
       ' V | range '+Scope1.Channel1.Range.value+' -> '+
       Number(Scope1.Channel1.Range.real).toFixed(3)+' Vpp');
 }catch(e){ui.set('ad3Status','Capture error: '+e);print('AD3 capture error: '+e);}
});
// Does not change Scope configuration or arm an acquisition.
ui.on('ad3Ranges','clicked',function(){
 try {
   const caps=Scope1.capabilities();
   const steps=Scope1.Channel1.Range.steps;
   const text=caps.rangeStepsAvailable ? steps.map(function(v){return Number(v)+' Vpp';}).join(', ') :
        'SDK range steps unavailable (or Simulation mode)';
   ui.set('ad3RangeStatus','SDK selectable ranges: '+text);
   print('[AD3] '+text);
   if(caps.rangeMinVpp!==undefined && caps.rangeMaxVpp!==undefined)
     print('[AD3] Info: '+caps.rangeMinVpp+' .. '+caps.rangeMaxVpp+' Vpp ('+caps.rangeStepCount+' steps)');
   if(caps.rangeStepsError)print('[AD3] '+caps.rangeStepsError);
 } catch(e) { ui.set('ad3RangeStatus','Range query error: '+String(e)); }
});
