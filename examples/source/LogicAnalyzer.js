/**
 * WFScript Example: Logic Analyzer
 * Capture 16-bit DigitalIn samples and display a timing chart.
 *
 * Device: Simulation by default. Choose Physical Device before Run Panel
 *         for real AD3 operations. No output is enabled merely by opening.
 * Wiring: For loopback, DIO0 → 1 kΩ → DIO1.
 *
 * The script belongs to ONE .wfpanel project. If it contains multiple Pages,
 * the Pages share this JavaScript runtime and globally unique widget IDs.
 * Read the SETTINGS / FUNCTIONS / EVENTS sections from top to bottom.
 * Edit this .js file in the source tree, then run:
 *      python tools/build_examples.py
 * to regenerate the packaged .wfpanel resource and keep JS in sync.
 */

// 13 LOGIC ANALYZER — REAL 16-BIT DIGITALIN ACQUISITION
// ============================================================================
// single() arms an untriggered acquisition; wait() transfers it afterward.
// CAPTURE INPUTS never enables digital outputs. LOOPBACK deliberately drives
// DIO0 (only) while DigitalIn captures both DIO0 and DIO1.
function liSettings() {
  const rate=Number(ui.get('liRate'));
  const samples=Number(ui.get('liSamples'));
  const pinLabel=String(ui.get('liPin'));
  const pin=Number(pinLabel.replace('DIO',''));
  if(!isFinite(rate)||rate<1||!isFinite(samples)||samples<16||
     !Number.isInteger(pin)||pin<1||pin>15)throw Error('Invalid Logic capture setting');
  Logic1.Time.Rate.value=rate;
  Logic1.Time.Samples.value=samples;
  return {rate,samples,pin};
}
function liPlot(words,rate,pin,prefix) {
  if(!words.length || !isFinite(rate) || rate<=0)throw Error('No valid DigitalIn capture samples');
  const times=[],output=[],input=[];
  let edges=0,high=0;
  for(let i=0;i<words.length;i++) {
    times.push(i*1000/rate); // milliseconds for the Digital Timing chart
    const word=Number(words[i]);
    output.push(word&1);
    const bit=(word>>>pin)&1;
    input.push(bit);
    if(bit)high++;
    if(i>0 && bit!==input[i-1])edges++;
  }
  if(!plots.setSeries('liChart','DIO0',times,output))throw Error(plots.lastError);
  if(!plots.setSeries('liChart','DIO'+pin,times,input))throw Error(plots.lastError);
  const duration=(1000*words.length/rate).toFixed(3);
  const result=prefix+' | '+words.length+' samples / '+rate.toFixed(1)+
      ' Hz / '+duration+' ms / DIO'+pin+' edges='+edges+' / HIGH '+
      (100*high/words.length).toFixed(1)+'%';
  ui.set('liStatus',result);
  print('Logic capture: '+result);
  return edges;
}
function liCaptureOnly() {
  try {
    const cfg=liSettings();
    ui.set('liStatus','CAPTURING INPUTS / no output commands');
    if(!Logic1.single())throw Error(Logic1.lastError);
    if(!Logic1.wait(3))throw Error(Logic1.lastError);
    liPlot(Logic1.samples(),Logic1.sampleRateHz,cfg.pin,
           StaticIO.simulation?'SIMULATED INPUT DATA':'PHYSICAL INPUT CAPTURE');
  } catch(e) {
    Logic1.stop();
    ui.set('liStatus','INPUT CAPTURE ERROR: '+String(e));
    print('Logic input capture error: '+String(e));
  }
}
function liLoopback() {
  liSettings();
  // The self-generated loopback must have a long enough capture window.
  ui.set('liRate','10000');
  ui.set('liSamples','4096');
  ui.set('liPin','DIO1');
  const simulated=!!StaticIO.simulation;
  let outcome='',released=false;
  try {
    if(!simulated) {
      // This operation intentionally releases any previously driven static pins.
      if(!StaticIO.stop())throw Error('Cannot establish all inputs Z: '+StaticIO.lastError);
      if(!StaticIO.setPin(0,0))throw Error('Cannot set DIO0 LOW: '+StaticIO.lastError);
      if(!wait(0.020))throw Error('Script interrupted during digital loopback');
    }
    // Conservative fixed acquisition window: 4096/10000 = 409.6ms.
    // The page settings are reset explicitly so a previous fast capture cannot
    // silently make the software-timed test miss its edges.
    Logic1.Time.Rate.value=10000;
    Logic1.Time.Samples.value=4096;
    if(!Logic1.single())throw Error('Arm failed: '+Logic1.lastError);
    if(!simulated) {
      if(!wait(0.025))throw Error('Script interrupted during digital loopback');
      if(!StaticIO.setPin(0,1))throw Error(StaticIO.lastError);
      if(!wait(0.030))throw Error('Script interrupted during digital loopback');
      if(!StaticIO.setPin(0,0))throw Error(StaticIO.lastError);
      if(!wait(0.030))throw Error('Script interrupted during digital loopback');
      if(!StaticIO.setPin(0,1))throw Error(StaticIO.lastError);
      if(!wait(0.030))throw Error('Script interrupted during digital loopback');
      if(!StaticIO.setPin(0,0))throw Error(StaticIO.lastError);
    }
    if(!Logic1.wait(3))throw Error('DigitalIn wait failed: '+Logic1.lastError);
    const words=Logic1.samples();
    const edges=liPlot(words,Logic1.sampleRateHz,1,
                      simulated?'SIMULATED LOOPBACK':'DIO0→DIO1 LOOPBACK');
    let matching=0;
    for(let i=0;i<words.length;i++)
      if((words[i]&1)===((words[i]>>>1)&1))matching++;
    const agreement=100*matching/words.length;
    outcome=simulated?'SIMULATION ONLY — no hardware tested':
            (edges>=2 && agreement>=95 ?
              'PASS: DIO1 transitions captured ('+edges+' edges, '+agreement.toFixed(1)+'% matches DIO0)':
              'CHECK WIRING / TIMING: '+edges+' DIO1 edges, DIO0/DIO1 match '+agreement.toFixed(1)+'%');
  } catch(e) {
    outcome='ERROR: '+String(e);
    Logic1.stop();
  } finally {
    // On hardware: never claim high-Z unless the SDK accepted the stop.
    if(simulated)released=true;
    else released=StaticIO.stop();
    if(released) {
      // Static I/O is released; its selector widgets belong to a different example.
      ui.set('liStatus',outcome+' | STATIC OUTPUTS Z');
    } else ui.set('liStatus','DANGER — HIGH-Z NOT CONFIRMED: '+StaticIO.lastError+' / '+outcome);
    print('Logic loopback: '+outcome+(released?' / output released':' / STOP FAILED'));
  }
}
ui.on('liCapture','clicked',liCaptureOnly);
ui.on('liLoopback','clicked',liLoopback);
ui.on('liClear','clicked',function(){
  plots.clear('liChart');
  ui.set('liStatus','CHART CLEARED — acquisition idle');
});

// Show the currently selected DigitalIn acquisition window instead of a stale
// 20 kS/s default. Source v5.6.47 behavior remains otherwise unchanged.
function liUpdateInfo(){
  const rate=Number(ui.get('liRate')),samples=Number(ui.get('liSamples'));
  if(rate>0 && samples>0)
    ui.set('liInfo','CAPTURE WINDOW '+(samples*1000/rate).toFixed(3)+
        ' ms\n'+(rate/1000).toFixed(1)+' kS/s × '+samples+' samples');
}
ui.on('liRate','selectionChanged',liUpdateInfo);
ui.on('liSamples','selectionChanged',liUpdateInfo);
liUpdateInfo();

// ============================================================================
