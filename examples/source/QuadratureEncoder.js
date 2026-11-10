/**
 * WFScript Example: Quadrature Encoder + Index
 * Generate synchronized A/B/index and verify RPM and phase.
 *
 * Device: Simulation by default. Choose Physical Device before Run Panel
 *         for real AD3 operations. No output is enabled merely by opening.
 * Wiring: DIO0 → 1 kΩ → DIO3, DIO1 → 1 kΩ → DIO4, DIO2 → 1 kΩ → DIO5.
 *
 * The script belongs to ONE .wfpanel project. If it contains multiple Pages,
 * the Pages share this JavaScript runtime and globally unique widget IDs.
 * Read the SETTINGS / FUNCTIONS / EVENTS sections from top to bottom.
 * Edit this .js file in the source tree, then run:
 *      python tools/build_examples.py
 * to regenerate the packaged .wfpanel resource and keep JS in sync.
 */

// 15 ENCODER — HARDWARE SYNCHRONIZED A/B/INDEX
// WFScript v5.6.49.2 — full 16-bit DigitalIn transfer and encoder continuity diagnostics.
// Three simultaneous Custom DigitalOut buffers run from one device clock.
// NEVER starts on panel load. Wire DIO0 -> DIO3, DIO1 -> DIO4,
// DIO2 -> DIO5 through separate 1k series resistors before VERIFY.
// DIO0=A, DIO1=B, DIO2=INDEX. DIO3/4/5 are read-only inputs.
function encSettings() {
  const ppr=Number(ui.get('encPpr'));
  const rpm=Number(ui.get('encRpm'));
  const reverse=String(ui.get('encDirection'))==='Reverse';
  if(!Number.isInteger(ppr)||ppr<1||ppr>1024||!isFinite(rpm)||rpm<1||rpm>10000)
    throw Error('Invalid encoder PPR or RPM');
  return {ppr,rpm,reverse};
}
function encInfo() {
  const s=encSettings();
  const rate=Math.round(4096*s.rpm/144); // target approx 2.4 mechanical revolutions
  ui.set('encInfo','A/B '+(s.ppr*s.rpm/60).toFixed(1)+' Hz  |  Index '+
    (s.rpm/60).toFixed(1)+' Hz  |  x4 '+(4*s.ppr*s.rpm/60).toFixed(1)+' edges/s\n'+
    'CAPTURE '+(rate/1000).toFixed(1)+' kS/s × 4096 (~2.4 rev)');
}
function encStop() {
  const stopped=Patterns1.stop();
  ui.set('encStatus',stopped?'STOPPED — ALL THREE PATTERN OUTPUTS RELEASED (Z)':
    'DANGER: THREE-CHANNEL SHUTDOWN NOT CONFIRMED: '+Patterns1.lastError);
  print('Encoder stop: '+(stopped?'OUTPUTS RELEASED':'FAILED: '+Patterns1.lastError));
  return stopped;
}
function encStart() {
  try {
    const s=encSettings();
    if(Patterns1.running && !Patterns1.stop()) throw Error('Cannot stop prior Pattern: '+Patterns1.lastError);
    if(!StaticIO.stop()) throw Error('StaticIO release failed: '+StaticIO.lastError);
    if(!Patterns1.runEncoder(0,1,2,s.ppr,s.rpm,s.reverse)) throw Error(Patterns1.lastError);
    const st=Patterns1.status();
    ui.set('encStatus',(st.simulation?'SIMULATION ONLY':'RUNNING HARDWARE')+' / A=DIO0 B=DIO1 Z=DIO2 / '+
      st.encoderActualRpm.toFixed(3)+' RPM / '+s.ppr+' PPR / '+(s.reverse?'REVERSE':'FORWARD'));
    print('Encoder started: '+st.encoderActualRpm.toFixed(4)+' RPM, '+s.ppr+' PPR, '+
          (s.reverse?'reverse':'forward')+' / '+(st.simulation?'SIMULATION':'PHYSICAL'));
  } catch(e) {
    const stopped=Patterns1.stop();
    ui.set('encStatus','ENCODER START ERROR: '+String(e)+(stopped?' / released':' / DANGER: '+Patterns1.lastError));
    print('Encoder start failed: '+String(e));
  }
}
function encPreview() {
  const s=encSettings(), rate=Math.max(1000,s.ppr*s.rpm/60*40),t=[],a=[],b=[],z=[];
  const forward=[[0,0],[1,0],[1,1],[0,1]];
  const reverse=[[0,0],[0,1],[1,1],[1,0]];
  for(let i=0;i<4096;i++) {
    const secs=i/rate;
    const quarter=Math.floor(secs*s.rpm/60*s.ppr*4);
    const state=(s.reverse?reverse:forward)[quarter%4];
    t.push(secs*1000);a.push(state[0]);b.push(state[1]);z.push((quarter%(4*s.ppr))===0?1:0);
  }
  plots.setSeries('encChart','A (ideal simulation)',t,a);
  plots.setSeries('encChart','B (ideal simulation)',t,b);
  plots.setSeries('encChart','INDEX (ideal simulation)',t,z);
  ui.set('encStatus','IDEAL PREVIEW — no SDK output, timing or physical measurement');
}
// Process captured PHYSICAL bits 3,4,5 (not the driving pins) and require:
// - matching driven pin and physical readback for all three channels
// - Gray-code transitions in the chosen direction
// - repeated index pulse at the expected once-per-revolution spacing
function encSdkSummary(st) {
  if(!st.sdkStatusAvailable)return 'DigitalOut state unavailable (cached '+(st.running?'running':'stopped')+')';
  const rep=st.sdkRepeatAvailable?st.sdkRepeatCount:'unknown';
  return 'DigitalOut='+st.sdkStateName+'('+st.sdkStateCode+') / repeat='+rep+
         ' (diagnostic; Run=0 continuous) / cached='+st.running;
}
function encCaptureSummary(d) {
  return 'DigitalIn transfer: '+d.samplesReturned+'/'+d.requestedSamples+
    ' words, '+d.bytesRequested+' requested bytes / SDK valid='+
    (d.sdkValidSamples<0?'unavailable':d.sdkValidSamples)+
    ' / DigitalIn state='+d.sdkStateCode+
    ' / changes first/second half='+d.firstHalfChanges+'/'+d.secondHalfChanges+
    ' / nonzero first/second half='+d.firstHalfHighWords+'/'+d.secondHalfHighWords;
}
function encAnalyze(words,rate,s,actualRpm) {
  if(words.length<100 || !isFinite(rate) || rate<=0)throw Error('Missing DigitalIn samples');
  const time=[],a=[],b=[],z=[];let matches=0;
  for(let i=0;i<words.length;i++) {
    const w=Number(words[i]);
    const abit=(w>>>3)&1,bbit=(w>>>4)&1,zbit=(w>>>5)&1;
    a.push(abit);b.push(bbit);z.push(zbit);time.push(i*1000/rate);
    for(let n=0;n<3;n++)if(((w>>>n)&1)===((w>>>(n+3))&1))matches++;
  }
  plots.setSeries('encChart','A / DIO3',time,a);
  plots.setSeries('encChart','B / DIO4',time,b);
  plots.setSeries('encChart','INDEX / DIO5',time,z);
  const states=[0,1,3,2],rises=[];
  let forward=0,reverse=0,invalid=0,indexBad=0;
  let prev=states.indexOf(a[0]+2*b[0]);
  for(let i=1;i<words.length;i++) {
    if(z[i]&&!z[i-1]){
      rises.push(i);
      if(a[i]!==0 || b[i]!==0)indexBad++;
    }
    const state=states.indexOf(a[i]+2*b[i]);
    if(state!==prev){
      if(state===(prev+1)%4)forward++;
      else if(state===(prev+3)%4)reverse++;
      else invalid++;
      prev=state;
    }
  }
  const agreement=100*matches/(3*words.length);
  const good=s.reverse?reverse:forward,bad=s.reverse?forward:reverse;
  const rpm=rises.length>=2?60*rate*(rises.length-1)/(rises[rises.length-1]-rises[0]):NaN;
  const validRpm=isFinite(rpm)&&Math.abs(rpm-actualRpm)/actualRpm<=0.05;
  // A full ~2.4 revolution capture must contain ~2.4*4*PPR Gray changes.
  // Require >=80% to detect future half-transferred DigitalIn frames, while
  // tolerating the capture starting/ending between encoder state boundaries.
  const expectedSteps=(words.length-1)/rate*(actualRpm/60)*4*s.ppr;
  const pass=rises.length>=2 && validRpm && indexBad===0 && invalid===0 &&
             good>=expectedSteps*0.80 && bad===0 && agreement>=97;
  const result=(pass?'PASS':'CHECK')+' | '+rises.length+' index pulses | '+
    (isFinite(rpm)?rpm.toFixed(2):'n/a')+' RPM (expected '+actualRpm.toFixed(2)+') | '+
    (s.reverse?'reverse':'forward')+' steps '+good+' (opposite '+bad+', invalid '+invalid+') | '+
    'index phase errors '+indexBad+' | 3-pair pin agreement '+agreement.toFixed(1)+'%';
  print('Encoder: '+result);
  return result;
}
function encCapture(verify) {
  if(Patterns1.simulation){encPreview();print('Encoder: SIMULATION ONLY — no physical loopback test');return;}
  let result='';
  try {
    const selected=encSettings();
    if(verify){
      if(Patterns1.running && !Patterns1.stop())throw Error('Previous Pattern stop failed: '+Patterns1.lastError);
      if(!StaticIO.stop())throw Error('StaticIO release failed: '+StaticIO.lastError);
      if(!Patterns1.runEncoder(0,1,2,selected.ppr,selected.rpm,selected.reverse))throw Error(Patterns1.lastError);
    } else if(!Patterns1.running || Patterns1.status().mode!=='Encoder')
      throw Error('Press START ENCODER first');
    const st=Patterns1.status();
    print('Encoder SDK BEFORE: '+encSdkSummary(st));
    if(st.sdkStatusAvailable && !st.sdkRunning)
      throw Error('DigitalOut is '+st.sdkStateName+' instead of Running; restart encoder');
    // Capture-only measures the RUNNING configuration, not un-applied
    // changes in selector widgets.
    const s=verify?selected:{ppr:st.encoderPpr,rpm:st.encoderActualRpm,reverse:st.encoderReverse};
    const rpm=st.encoderActualRpm;
    // 4096 samples spanning about 2.4 rev gives index repetitions without
    // undersampling the quadrature states (at most 64 PPR on this Page).
    const rate=Math.round(4096*rpm/144);
    if(4096/(2.4*4*s.ppr)<6)
      throw Error('More than 64 PPR needs a longer DigitalIn buffer or fewer revs');
    Logic1.Time.Rate.value=rate;
    Logic1.Time.Samples.value=4096;
    if(!Logic1.single())throw Error('DigitalIn arm failed: '+Logic1.lastError);
    if(!Logic1.wait(6))throw Error('DigitalIn wait failed: '+Logic1.lastError);
    const diag=Logic1.captureDiagnostics();
    print('Encoder '+encCaptureSummary(diag));
    const after=Patterns1.status();
    print('Encoder SDK AFTER: '+encSdkSummary(after));
    result=encAnalyze(Logic1.samples(),Logic1.sampleRateHz,s,rpm);
    // The generator's actual state must remain Running through acquisition.
    if(after.sdkStatusAvailable && !after.sdkRunning)
      result='CHECK: DigitalOut stopped during capture ('+after.sdkStateName+') | '+result;
  } catch(e){
    Logic1.stop();
    result='ERROR: '+String(e);
    print('Encoder capture error: '+String(e));
  } finally {
    if(verify){
      const stopped=Patterns1.stop();
      result+=stopped?' / ALL OUTPUTS RELEASED':' / DANGER: OUTPUT STOP NOT CONFIRMED: '+Patterns1.lastError;
    }
    ui.set('encStatus',result);
    print('Encoder test: '+result);
  }
}
ui.on('encStart','clicked',encStart);
ui.on('encStop','clicked',encStop);
ui.on('encCapture','clicked',function(){encCapture(false);});
ui.on('encVerify','clicked',function(){encCapture(true);});
ui.on('encPreview','clicked',encPreview);
ui.on('encPpr','selectionChanged',encInfo);
ui.on('encRpm','selectionChanged',encInfo);
ui.on('encDirection','selectionChanged',encInfo);
encInfo();


// ============================================================================
