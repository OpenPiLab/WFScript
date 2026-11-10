/**
 * WFScript Example: Multi-Channel Sequence Lab
 * Generate and verify binary, Gray and walking-one multi-pin sequences.
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

// 16 SEQUENCE LAB — SYNCHRONIZED, REPEATING CUSTOM DIGITAL BIT STRINGS
// ============================================================================
// All channels use ONE native DigitalOut timebase and a single start command.
// WaveForms-style channel properties remain available for single clocks.
// runSequence is a WFScript-specific extension, not native WaveForms syntax.
// Preview never drives outputs. Starting and verifying are explicit actions.
function seqSettings() {
  const preset=String(ui.get('seqPreset'));
  const bitRate=Number(ui.get('seqBitRate'));
  if(!isFinite(bitRate) || bitRate<10 || bitRate>1000000) throw Error('Invalid bit rate');
  const choices={
    'Binary Counter':['01010101','00110011','00001111'],
    'Gray Counter':['01100110','00111100','00001111'],
    'Walking One':['10000000','00100000','00001000']
  };
  if(!choices[preset])throw Error('Invalid pattern preset: '+preset);
  return {preset:preset,bitRate:bitRate,bits:choices[preset]};
}
function seqInfo() {
  const s=seqSettings();
  ui.set('seqInfo',s.preset+' | '+s.bitRate+' bit/s | 8 bits per sequence (period '+
         (8000/s.bitRate).toFixed(3)+' ms) | '+s.bits.join(' / '));
}
function seqPreview() {
  const s=seqSettings(),x=[],lanes=[[],[],[]];
  for(let i=0;i<512;i++) {
    x.push(i*1000/(s.bitRate*16));
    const index=Math.floor(i/16)%8;
    for(let k=0;k<3;k++)lanes[k].push(Number(s.bits[k].charAt(index)));
  }
  plots.clear('seqChart');
  for(let k=0;k<3;k++)plots.setSeries('seqChart','PREVIEW DIO'+k,x,lanes[k]);
  ui.set('seqStatus','MATHEMATICAL PREVIEW ONLY — NO AD3 OUTPUT');
}
function seqStart() {
  try {
    const s=seqSettings();
    if(Patterns1.running && !Patterns1.stop())throw Error('Previous Patterns stop failed: '+Patterns1.lastError);
    if(!StaticIO.stop())throw Error('StaticIO release failed: '+StaticIO.lastError);
    if(!Patterns1.runSequence([0,1,2],s.bits,s.bitRate))throw Error(Patterns1.lastError);
    const st=Patterns1.status();
    ui.set('seqStatus',(st.simulation?'SIMULATION ONLY':'RUNNING HARDWARE')+' | '+s.preset+
           ' / '+st.sequenceActualBitRateHz.toFixed(3)+' bit/s / repeat '+st.sequenceRepeatFactor);
    print('Sequence started: '+s.preset+' '+st.sequenceActualBitRateHz.toFixed(3)+' bits/s '+
          (st.simulation?'SIMULATION':'PHYSICAL'));
  } catch(e) {
    const safe=Patterns1.stop();
    const msg='SEQUENCE START ERROR: '+String(e)+(safe?' / output release requested':' / OUTPUT STOP FAILED: '+Patterns1.lastError);
    ui.set('seqStatus',msg);print(msg);
  }
}
function seqStop() {
  const safe=Patterns1.stop();
  ui.set('seqStatus',safe?'SEQUENCE STOPPED / OUTPUTS Z':'DANGER: stop not confirmed: '+Patterns1.lastError);
  print('Sequence stop: '+(safe?'OUTPUTS RELEASED':'FAILED: '+Patterns1.lastError));
}
// Search for the repeating bit period using the middle of each bit cell.
// Since DigitalIn can start at any point in the cycle, search phase offsets
// across the entire 8-bit period, never assuming input/output trigger sync.
function seqAnalyze(words,rate,st) {
  const bits=st.sequenceBitStrings,bitRate=st.sequenceActualBitRateHz;
  const n=words.length,spb=rate/bitRate,period=spb*bits[0].length;
  if(n<128 || !isFinite(spb) || spb<8)throw Error('DigitalIn sampling must be >=8× bit rate');
  const x=[],lanes=[[],[],[]];let matches=0,changes=0,secondChanges=0;
  for(let i=0;i<n;i++) {
    const word=Number(words[i]);x.push(i*1000/rate);
    for(let k=0;k<3;k++) {
      const input=(word>>>(k+3))&1;
      lanes[k].push(input);
      if(input===((word>>>k)&1))matches++;
      if(i>0 && input!==lanes[k][i-1]) {
        changes++;if(i>=n/2)secondChanges++;
      }
    }
  }
  plots.clear('seqChart');
  for(let k=0;k<3;k++)plots.setSeries('seqChart','DIO'+(k+3)+' / physical',x,lanes[k]);
  const agreement=100*matches/(3*n);
  const checks=Math.min(40,Math.floor((n-period-2)/spb));
  if(checks<8)throw Error('Capture too short for a sequence validation');
  let best=-1,phaseBest=0;
  const end=Math.min(Math.floor(period),Math.floor(n-(checks+1)*spb));
  for(let phase=0;phase<=end;phase++) {
    let valid=0;
    for(let j=0;j<checks;j++) {
      const index=Math.round(phase+j*spb);
      if(index<0 || index>=n)continue;
      for(let k=0;k<3;k++)if(lanes[k][index]===Number(bits[k].charAt(j%bits[k].length)))valid++;
    }
    if(valid>best){best=valid;phaseBest=phase;}
  }
  const patternMatch=100*best/(3*checks);
  const pass=agreement>=99 && patternMatch>=97 && changes>=12 && secondChanges>=4;
  const status=(pass?'PASS':'CHECK')+' | '+st.sequenceActualBitRateHz.toFixed(2)+' bit/s'+
     ' | loopback '+agreement.toFixed(2)+'% | pattern match '+patternMatch.toFixed(1)+'%'+
     ' | transitions '+changes+' (second half '+secondChanges+') | phase '+phaseBest;
  print('Sequence: '+status);
  return status;
}
function seqCapture(verify) {
  if(Patterns1.simulation){seqPreview();print('Sequence: SIMULATION ONLY / no physical verification');return;}
  let result='';
  try {
    if(verify) {
      const s=seqSettings();
      if(Patterns1.running && !Patterns1.stop())throw Error('Previous Pattern stop failed: '+Patterns1.lastError);
      if(!StaticIO.stop())throw Error('StaticIO release failed: '+StaticIO.lastError);
      if(!Patterns1.runSequence([0,1,2],s.bits,s.bitRate))throw Error(Patterns1.lastError);
    } else if(!Patterns1.running || Patterns1.status().mode!=='Sequence')
      throw Error('Press START SEQUENCE first');
    const st=Patterns1.status();
    if(st.mode!=='Sequence' || !st.running)throw Error('Sequence is not running');
    if(st.sdkStatusAvailable && !st.sdkRunning)throw Error('DigitalOut not Running: '+st.sdkStateName);
    const actual=st.sequenceActualBitRateHz;
    Logic1.Time.Rate.value=Math.min(1000000,Math.max(20000,Math.round(actual*32)));
    Logic1.Time.Samples.value=4096;
    if(!Logic1.single())throw Error('DigitalIn arm failed: '+Logic1.lastError);
    if(!Logic1.wait(6))throw Error('DigitalIn wait failed: '+Logic1.lastError);
    const diag=Logic1.captureDiagnostics();
    print('Sequence DigitalIn: '+diag.samplesReturned+'/'+diag.requestedSamples+
          ' samples, '+diag.bytesRequested+' requested bytes, first/second transitions '+
          diag.firstHalfChanges+'/'+diag.secondHalfChanges);
    result=seqAnalyze(Logic1.samples(),Logic1.sampleRateHz,st);
    const after=Patterns1.status();
    if(after.sdkStatusAvailable && !after.sdkRunning)result='CHECK: generator stopped during capture / '+result;
  }catch(e){Logic1.stop();result='ERROR: '+String(e);print('Sequence error: '+String(e));}
  finally {
    if(verify) {
      const stopped=Patterns1.stop();
      result+=stopped?' / ALL OUTPUTS RELEASED':' / DANGER: STOP NOT CONFIRMED: '+Patterns1.lastError;
    }
    ui.set('seqStatus',result);print('Sequence test: '+result);
  }
}
ui.on('seqStart','clicked',seqStart);
ui.on('seqStop','clicked',seqStop);
ui.on('seqCapture','clicked',function(){seqCapture(false);});
ui.on('seqVerify','clicked',function(){seqCapture(true);});
ui.on('seqPreview','clicked',seqPreview);
ui.on('seqPreset','selectionChanged',seqInfo);
ui.on('seqBitRate','selectionChanged',seqInfo);
seqInfo();
