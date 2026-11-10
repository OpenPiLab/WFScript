/**
 * WFScript Example: Digital Clock / PWM
 * Generate hardware-timed DigitalOut clock and verify duty/frequency.
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

// 14 PATTERNS — AD3 DIGITALOUT CLOCK/PWM, CAPTURED WITH DIGITALIN
// ============================================================================
// Output is never enabled on startup; only START or TEST calls Patterns1.run().
// This first release uses one Clock channel, logical DIO0, at physical DIO0.
function pgSettings(){
  const hz=Number(ui.get('pgFreq')),duty=Number(ui.get('pgDuty'));
  if(!isFinite(hz)||hz<1||hz>1000000||!isFinite(duty)||duty<1||duty>99)
    throw Error('Invalid Pattern clock frequency or duty');
  const ch=Patterns1.Channels.DIO0;
  ch.Pin.value=0;
  ch.Output.text='PP';
  ch.Idle.text='Z';
  ch.Type.text='Clock';
  ch.Frequency.value=hz;
  ch.Duty.value=duty;
  return {hz,duty};
}
function pgAutoRate(hz){return Math.min(1000000,Math.max(20000,hz*50));}
function pgInfo(){
  const cfg=pgSettings();
  const rate=pgAutoRate(cfg.hz),duration=4096*1000/rate;
  ui.set('pgInfo','CAPTURE '+(rate/1000).toFixed(1)+' kS/s × 4096\n'+
     duration.toFixed(3)+' ms window');
}
function pgStop(){
  const ok=Patterns1.stop();
  ui.set('pgStatus',ok?'STOPPED — Pattern output OFF / high impedance':
    'DANGER — Pattern output shutdown NOT CONFIRMED: '+Patterns1.lastError);
  print('Patterns stop: '+(ok?'OUTPUT RELEASED':'FAILED: '+Patterns1.lastError));
  return ok;
}
function pgStart(){
  try{
    const cfg=pgSettings();
    // Release previously selected static output pins before DigitalOut starts.
    if(Patterns1.running && !Patterns1.stop())throw Error('Cannot stop running Pattern: '+Patterns1.lastError);
    if(!StaticIO.stop())throw Error('Cannot release StaticIO: '+StaticIO.lastError);
    if(!Patterns1.run())throw Error(Patterns1.lastError);
    const s=Patterns1.status();
    ui.set('pgStatus',(s.simulation?'SIMULATION ONLY — no physical output':'RUNNING HARDWARE')+
      ' / DIO0 / '+s.actualFrequencyHz.toFixed(2)+' Hz / '+
      s.actualDutyPercent.toFixed(2)+'% duty');
    print('Patterns started: DIO0 / '+s.actualFrequencyHz+' Hz / '+s.actualDutyPercent+'% duty / '+
          (s.simulation?'SIMULATION':'PHYSICAL'));
  }catch(e){
    const stopped=Patterns1.stop();
    ui.set('pgStatus','PATTERN START ERROR: '+String(e)+
        (stopped?' / output stopped':' / STOP UNCONFIRMED: '+Patterns1.lastError));
    print('Patterns start error: '+String(e));
  }
}
function pgPlot(words,rate,suffix){
  if(!words.length||!isFinite(rate)||rate<=0)throw Error('No valid captured DigitalIn samples');
  const t=[],d0=[],d1=[];
  for(let i=0;i<words.length;i++){
    const w=Number(words[i]); t.push(i*1000/rate);
    d0.push(w&1);d1.push((w>>>1)&1);
  }
  if(!plots.setSeries('pgChart','DIO0',t,d0))throw Error(plots.lastError);
  if(!plots.setSeries('pgChart','DIO1',t,d1))throw Error(plots.lastError);
  const rises=[];
  for(let i=1;i<d1.length;i++)if(d1[i] && !d1[i-1])rises.push(i);
  let frequency=NaN,duty=NaN,matching=0;
  for(let i=0;i<d1.length;i++)if(d0[i]===d1[i])matching++;
  const agreement=100*matching/d1.length;
  if(rises.length>=3){
    const first=rises[0],last=rises[rises.length-1];
    const periods=rises.length-1;
    frequency=rate*periods/(last-first);
    let high=0;
    for(let i=first;i<last;i++)high+=d1[i];
    duty=100*high/(last-first);
  }
  const details='CAPTURE '+suffix+' | '+rises.length+' rising edges | '+
      (isFinite(frequency)?frequency.toFixed(2):'n/a')+' Hz | '+
      (isFinite(duty)?duty.toFixed(1):'n/a')+'% duty | '+agreement.toFixed(1)+'% pin match';
  print('Patterns: '+details);
  return {rises:rises.length,frequency,duty,agreement,details};
}
function pgIdealPreview(){
  const cfg=pgSettings(),rate=pgAutoRate(cfg.hz);
  const t=[],d=[];
  for(let i=0;i<2048;i++){
    const ms=i*1000/rate;
    t.push(ms);
    d.push(((i/rate*cfg.hz)%1)<(cfg.duty/100)?1:0);
  }
  plots.setSeries('pgChart','DIO0 (ideal simulation)',t,d);
  ui.set('pgStatus','SIMULATION PREVIEW ONLY — no SDK capture or physical timing measured');
}
function pgAcquire(requireTest){
  if(Patterns1.simulation){
    pgIdealPreview();
    print('Patterns: SIMULATION ONLY — loopback test requires physical AD3');
    return;
  }
  let outcome='',result=null;
  try{
    if(requireTest){
      pgSettings();
      if(Patterns1.running && !Patterns1.stop())throw Error('Pattern restart stop failed: '+Patterns1.lastError);
      if(!StaticIO.stop())throw Error('StaticIO release failed: '+StaticIO.lastError);
      if(!Patterns1.run())throw Error('DigitalOut start failed: '+Patterns1.lastError);
    }else if(!Patterns1.running)throw Error('Press START CLOCK / PWM first');
    const actual=Patterns1.status();
    const rate=pgAutoRate(actual.actualFrequencyHz || Number(ui.get('pgFreq')));
    Logic1.Time.Rate.value=rate;
    Logic1.Time.Samples.value=4096;
    // Clock is already hardware-running; we do not create the edges using wait().
    if(!Logic1.single())throw Error('DigitalIn arm failed: '+Logic1.lastError);
    if(!Logic1.wait(3))throw Error('DigitalIn capture failed: '+Logic1.lastError);
    result=pgPlot(Logic1.samples(),Logic1.sampleRateHz,
      requireTest?'DIO0→DIO1 LOOPBACK':'RUNNING CLOCK');
    const fOk=isFinite(result.frequency) &&
      Math.abs(result.frequency-actual.actualFrequencyHz)/actual.actualFrequencyHz<=0.05;
    const dutyOk=isFinite(result.duty) &&
      Math.abs(result.duty-actual.actualDutyPercent)<=5.0;
    const matches=result.agreement>=95;
    outcome=(fOk && dutyOk && matches && result.rises>=3 ?'PASS':'CHECK')+
      ' | measured '+(isFinite(result.frequency)?result.frequency.toFixed(1):'n/a')+
      ' Hz / '+(isFinite(result.duty)?result.duty.toFixed(1):'n/a')+
      '% | expected '+actual.actualFrequencyHz.toFixed(1)+' Hz / '+
      actual.actualDutyPercent.toFixed(1)+'% | pin match '+result.agreement.toFixed(1)+'%';
  }catch(e){
    Logic1.stop();
    outcome='CAPTURE ERROR: '+String(e);
  }finally{
    // An automated test always releases the Pattern output, even on failure.
    if(requireTest){
      const released=Patterns1.stop();
      outcome+=released?' / output released':' / DANGER: OUTPUT STOP NOT CONFIRMED: '+Patterns1.lastError;
    }
    ui.set('pgStatus',outcome);
    print('Pattern loopback: '+outcome);
  }
}
ui.on('pgStart','clicked',pgStart);
ui.on('pgStop','clicked',pgStop);
ui.on('pgCapture','clicked',function(){pgAcquire(false);});
ui.on('pgTest','clicked',function(){pgAcquire(true);});
ui.on('pgFreq','selectionChanged',pgInfo);
ui.on('pgDuty','selectionChanged',pgInfo);
pgInfo();
