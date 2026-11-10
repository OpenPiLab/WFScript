/**
 * WFScript Example: Static Digital I/O
 * Set Z, LOW and HIGH per pin, read inputs and test a physical loopback.
 *
 * Device: Simulation by default. Choose Physical Device before Run Panel
 *         for real AD3 operations. No output is enabled merely by opening.
 * Wiring: For loopback, DIO0 → 1 kΩ → DIO1; no external driven circuits.
 *
 * The script belongs to ONE .wfpanel project. If it contains multiple Pages,
 * the Pages share this JavaScript runtime and globally unique widget IDs.
 * Read the SETTINGS / FUNCTIONS / EVENTS sections from top to bottom.
 * Edit this .js file in the source tree, then run:
 *      python tools/build_examples.py
 * to regenerate the packaged .wfpanel resource and keep JS in sync.
 */

// 12 DIGITAL I/O — WAVEFORMS-STYLE STATIC I/O AND SAFE LOOPBACK
// ============================================================================
// Physically driving a pin is explicit: selecting a Z/LOW/HIGH value invokes
// StaticIO.setPin(). Starting this Panel performs NO StaticIO output writes.
// WaveForms-compatible: StaticIO.Channel0.DIO0.value=1; StaticIO.config();
// 0=LOW, 1=HIGH, 2=Z. Channel0 holds DIO0..7, Channel1 DIO8..15.
const dioStates=['Z','LOW','HIGH'];
function dioGetCode(i) {
  const s=String(ui.get('dioSelect'+i));
  if(s==='Z')return 2;
  if(s==='LOW')return 0;
  if(s==='HIGH')return 1;
  throw Error('Invalid DIO'+i+' selection '+s);
}
function dioRefresh(msg) {
  const s=StaticIO.status();
  if(!s.valid) {
    ui.set('dioStatus','INPUT STATUS FAILED: '+s.lastError);
    print('StaticIO status error: '+s.lastError);
    return false;
  }
  for(let i=0;i<16;i++)ui.set('dioLevel'+i,(s.inputMask>>>i)&1);
  const driven=('0000'+(s.outputEnableMask&0xffff).toString(16).toUpperCase()).slice(-4);
  const level=('0000'+(s.inputMask&0xffff).toString(16).toUpperCase()).slice(-4);
  ui.set('dioStatus',(msg||'REFRESH')+' | '+(s.simulation?'SIMULATION (no external wiring model)':'AD3 HARDWARE')+
      ' | OE 0x'+driven+' | INPUT 0x'+level);
  return true;
}
function dioApplyPin(i) {
  try {
    const state=dioGetCode(i);
    if(!StaticIO.setPin(i,state))throw Error(StaticIO.lastError);
    dioRefresh('DIO'+i+' '+dioStates[state===2?0:state===0?1:2]+' APPLIED');
  } catch(e) {
    const stopOk=StaticIO.stop();
    for(let k=0;k<16;k++)ui.set('dioSelect'+k,'Z');
    if(stopOk)dioRefresh('ERROR / OUTPUTS RELEASED: '+String(e));
    else ui.set('dioStatus','DANGER: HIGH-Z NOT CONFIRMED: '+StaticIO.lastError+' / '+String(e));
    print('StaticIO DIO'+i+': '+String(e));
  }
}
function dioAllInputs() {
  const ok=StaticIO.stop();
  if(ok) {
    for(let i=0;i<16;i++)ui.set('dioSelect'+i,'Z');
    dioRefresh('ALL INPUTS / HIGH-Z');
  } else ui.set('dioStatus','DANGER: HIGH-Z NOT CONFIRMED — '+StaticIO.lastError);
}
function dioLoopback() {
  if(StaticIO.simulation) {
    ui.set('dioStatus','LOOPBACK requires Physical Device mode and DIO0→1 through 1 kΩ');
    return;
  }
  let message='';
  try {
    // First release everything, then drive DIO0. DIO1 remains high-Z.
    if(!StaticIO.stop())throw Error('Cannot establish safe input state: '+StaticIO.lastError);
    if(!StaticIO.setPin(0,1))throw Error(StaticIO.lastError);
    wait(0.020); // allow flywire and input level to settle
    const hi=StaticIO.readPin(1);
    if(!StaticIO.setPin(0,0))throw Error(StaticIO.lastError);
    wait(0.020);
    const lo=StaticIO.readPin(1);
    if(hi<0 || lo<0)throw Error('Read failed: '+StaticIO.lastError);
    message=(hi===1 && lo===0)?'PASS: DIO1 followed HIGH then LOW':'FAIL: DIO1 read HIGH='+hi+', LOW='+lo;
  } catch(e) {message='ERROR: '+String(e);}
  finally {
    const stopped=StaticIO.stop();
    if(stopped) {
      for(let i=0;i<16;i++)ui.set('dioSelect'+i,'Z');
      dioRefresh('LOOPBACK '+message+' — ALL HIGH-Z');
    } else ui.set('dioStatus','DANGER: LOOPBACK HIGH-Z NOT CONFIRMED: '+StaticIO.lastError+' | '+message);
  }
  print('StaticIO loopback: '+message);
}
for(let i=0;i<16;i++)
  ui.on('dioSelect'+i,'selectionChanged',function(){dioApplyPin(i);});
ui.on('dioRead','clicked',function(){dioRefresh('MANUAL READ');});
ui.on('dioAllZ','clicked',dioAllInputs);
ui.on('dioLoopback','clicked',dioLoopback);
// Passive read only; no output enable SDK command on startup.
try{dioRefresh('READY / NO OUTPUT COMMANDS SENT');}
catch(e){ui.set('dioStatus','Startup digital read unavailable: '+String(e));}


// ============================================================================
