#!/usr/bin/env python3
"""Panel-only Rigol v0.6.5 native Test Executive update from validated RC1.5.5 source."""
from pathlib import Path
import json, copy, re
ROOT = Path(__file__).resolve().parent.parent / 'rigol_v064'
OUT = Path(__file__).resolve().parent
BASE = ROOT / 'Rigol_Four_Instrument_Operator_Bench_v0_6_4_Native_Charts.wfpanel'
panel=json.loads(BASE.read_text(encoding='utf8'))
source=panel['script']
source=source.replace('Rigol Four-Instrument Operator Bench v0.6.3', 'Rigol Four-Instrument Operator Bench v0.6.5', 1)
source=source.replace('Four front-panel-inspired Pages: MHO954, DG902 Pro, DP932A and DM858.', 'Five Pages: MHO954, DG902 Pro, DP932A, DM858 and Native Automated Tests.', 1)
source=source.replace("log('INIT', 'Rigol Operator Bench v0.6.3", "log('INIT', 'Rigol Operator Bench v0.6.5")
# Preserve all original code, except for a focused ownership guard at existing entry points.
needle="function connect(which) {\n    var tag = which.toUpperCase();"
assert source.count(needle)==1
source=source.replace(needle,"function connect(which) {\n    if (rigolTestRunning) return message(which.toUpperCase(), 'Test Executive running — connections locked');\n    var tag = which.toUpperCase();")
needle="function disconnect(which) {\n    cancelOperation(which, 'Operator disconnected ' + DEVICE[which], false);"
assert source.count(needle)==1
source=source.replace(needle,"function disconnect(which) {\n    if (rigolTestRunning) return message(which.toUpperCase(), 'Test Executive running — press STOP and verify safe OFF before disconnect');\n    cancelOperation(which, 'Operator disconnected ' + DEVICE[which], false);")
needle="function runP(which, label, task, options) {\n    options = options || {};"
assert source.count(needle)==1
source=source.replace(needle,"function runP(which, label, task, options) {\n    options = options || {};\n    if (rigolTestRunning && !options.testOwned)\n        return Promise.reject(new Error('Test Executive owns instrument commands during this run'));" )
bridge=(OUT/'RigolTestStationBridge.js').read_text(encoding='utf8')
source+='\n\n// === Native Test Executive — rigol_v065 panel-only addition ===\n'+bridge+'\n'
panel['script']=source
panel['name']='Rigol Four-Instrument Operator Bench v0.6.5 — Native Test Station'
panel['description']='Four original Rigol operator pages plus a fifth hardware Test Executive page: DP932A 5.00→2.00 V, DM858 DCV, DG902 CH1 sine/square, MHO954 CH1 scope validation, native reports.'
panel['scriptName']='RigolBench_4Instruments_v0_6_5.js'

def widget(id,type,text,x,y,width,height,events=None,properties=None,**kwargs):
    c={'id':id,'type':type,'text':text,'x':x,'y':y,'width':width,'height':height,
       'value':kwargs.get('value',0),'min':0,'max':100,'events':{}}
    if events: c['events']={key:{'handler':handler,'enabled':True} for key,handler in events.items()}
    if properties is not None: c['properties']=properties
    return c

def lbl(id,txt,x,y,w,h,rich=False):
    return widget(id,'label',txt,x,y,w,h,properties={ 'textFormat':'Rich Text' if rich else 'Plain Text',
        'wordWrap':True,'borderVisible':False,'cornerRadius':7})

def btn(id,txt,x,y,w,h,handler):
    return widget(id,'button',txt,x,y,w,h,events={'clicked':handler})

page={'id':'rigoltest','name':'05 Automated Tests','width':1260,'height':1060,'controls':[]}
C=page['controls']
C.append(lbl('rigolTestTitle','RIGOL  /  FOUR-INSTRUMENT AUTOMATED TEST STATION',20,12,1220,44))
C.append(lbl('rigolTestGuide',
    '<b style="color:#7dd3fc">PHYSICAL LOOPBACK</b>  DP932A CH1 (+/−) → DM858 voltage input (COM/V).  '
    'DG902 Pro CH1 → MHO954 CH1 (direct BNC / 1 MΩ).<br>'
    '<b style="color:#ffb98a">LIVE OUTPUTS:</b> PSU CH1 ramps 5.00 → 2.00 V at 0.25 V intervals; '
    'GEN CH1 runs sine and square at 1/2 Vpp, 1/5 kHz. Both verified OFF on cleanup. '
    'Never connect PSU output to generator output; verify wiring/grounding and instrument limits before RUN.',
    20,64,1220,85,True))
C.append(widget('rigolTestWiring','toggle','WIRING CONFIRMED',20,163,242,50,value=0,
    properties={'accent':'amber'}))
C.append(lbl('rigolTestStartNote','Connect all 4 instruments on their normal pages. Stop the DMM monitor. CH1 outputs must start OFF.',278,159,962,54))
C.append(widget('rigolTestInfo','runInformation','Run Information',20,226,485,200))
C.append(btn('rigolTestRun','RUN AUTOMATED TEST',526,229,220,51,'onRigolTestRun'))
C.append(btn('rigolTestStop','STOP + CLEANUP',758,229,216,51,'onRigolTestStop'))
C.append(btn('rigolTestReset','RESET',986,229,254,51,'onRigolTestReset'))
C.append(lbl('rigolTestSafety','READY · Connect four instruments and confirm wiring; never run unattended.',525,285,715,64))
C.append(widget('rigolTestSequence','test_sequence','Native C++ Test Sequence',20,440,594,258,
    properties={'defaultSteps':'Preflight|PSU 5V|DMM 5V|Sweep|Generator|Cleanup',
        'maxSteps':100,'showProgress':True,'showElapsed':True,'showNumbers':True,
        'autoScroll':True,'compactMode':True}))
C.append(widget('rigolTestTable','measurement_table','Native Test Measurements',626,440,614,258,
    properties={'decimals':4,'maxRows':100,'headers':'Measurement|Measured|Limits|Result'}))
C.append(lbl('rigolTestLatest','No CH1 scope capture from a test yet.',20,710,1220,40))
C.append(widget('rigolTestLog','event_log','Test Executive Event Log',20,756,1220,184,
    properties={'maxEntries':500,'autoScroll':True,'showTimestamp':True,'showSource':True}))
C.append(btn('rigolTestJson','EXPORT JSON',20,954,199,46,'onRigolTestExportJson'))
C.append(btn('rigolTestCsv','EXPORT CSV',231,954,194,46,'onRigolTestExportCsv'))
C.append(lbl('rigolTestReport','Reports auto-archive as JSON after cleanup. JSON/CSV buttons export to Documents/WFScript Reports.',447,814,793,57))
C.append(lbl('rigolTestFooter','Limits are inclusive. The Test Sequence Editor owns test order, limits and timeouts. STOP is not a hardware interlock.',20,877,1220,35))
panel['pages'].append(page)
panel['runInformation']={'fields':[
    {'id':'uutId','label':'Fixture / UUT ID','type':'text','required':False,'default':'Rigol Loopback'},
    {'id':'operatorId','label':'Operator ID','type':'text','required':False,'default':''},
    {'id':'testNotes','label':'Test notes','type':'text','required':False,'default':''}
]}
panel['reportStorage']={'autoSave':True,'directory':''}
steps=[]
module=['''/** Rigol v0.6.5 — native Test Executive module.
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
''']
def action(id,name,handler,group='main',timeout=50000,description='',poll='pollDone'):
    steps.append({'id':id,'name':name,'description':description,'group':group,'type':'action',
        'mode':'poll','intervalMs':80,'timeoutMs':timeout,
        'handlers':{'start':handler,'poll':poll}})
def numeric(id,name,limits,unit,handler=None, start=None, group='main',timeout=35000,description=''):
    obj={'id':id,'name':name,'description':description,'group':group,'type':'numeric',
         'limits':{'low':round(limits[0],6),'high':round(limits[1],6)},'unit':unit}
    if handler:
        obj.update({'mode':'once','handler':handler})
    else:
        obj.update({'mode':'poll','intervalMs':80,'timeoutMs':timeout,
                    'handlers':{'start':start,'poll':'pollValue'}})
    steps.append(obj)
action('preflight','SETUP · verify wiring, four connections and outputs OFF','preflight','setup',40000,
       'No output is changed. Requires operator wiring acknowledgement and reads physical CH1 OFF states.',poll='pollPreflight')
for idx in range(13):
    v=round(5.0-0.25*idx,2)
    key=f'{int(round(v*100)):03d}'
    psu=f'power{key}'
    dmm=f'dmm{key}'
    module.append(f'export function {psu}() {{ RigolTestStation.beginPower({v:.2f}); }}\n')
    module.append(f'export function {dmm}() {{ RigolTestStation.beginDmm({v:.2f}); }}\n')
    # Inclusive tolerances: PSU +/- 0.10 V (2% full-scale); DMM +/- 0.075 V (75 mV).
    numeric(f'psu_{key}',f'DP932A CH1 {v:.2f} V readback',(v-.10,v+.10),'V',start=psu,
            description='Actual DP932A voltage readback (not merely the programmed setpoint).')
    numeric(f'dmm_{key}',f'DM858 DCV at {v:.2f} V',(v-.075,v+.075),'V',start=dmm,
            description='Independent five-PLC DMM voltage read, 10 V range, after PSU settling.')
action('psu_off','DP932A CH1 OFF (readback verified)','powerOff',timeout=32000,
       description='Remove PSU CH1 output before starting generator/scope tests.')
for shape in ('sine','square'):
 for amp in (1,2):
  for hz in (1000,5000):
    key=f'{shape}_{amp}Vpp_{hz}Hz'
    short=f'{shape.capitalize()} {amp} Vpp @ {hz} Hz'
    begin='capture_'+key
    module.append(f'export function {begin}() {{ RigolTestStation.beginWave("{shape}",{amp},{hz},"{key}"); }}\n')
    for metric in ('vpp','hz','shape'):
        func=f'actual_{metric}_{key}'
        module.append(f'export function {func}() {{ return RigolTestStation.metric("{key}","{metric}"); }}\n')
    action('capture_'+key,short+' · configure / capture CH1',begin,timeout=120000,
        description='Enable DG902 CH1, configure MHO954 CH1, acquire fresh STOPped 1000-point capture.')
    numeric('amplitude_'+key,short+' · peak-to-peak',(amp*.85,amp*1.15),'Vpp',handler='actual_vpp_'+key,
        description='Scope-derived extrema; ±15% voltage acceptance.')
    numeric('frequency_'+key,short+' · frequency',(hz*.97,hz*1.03),'Hz',handler='actual_hz_'+key,
        description='Scope waveform rising-edge hysteretic period estimate; ±3%.')
    numeric('shape_'+key,short+' · AC RMS / Vpp',(.32,.40) if shape=='sine' else (.43,.55),'ratio',handler='actual_shape_'+key,
        description='Waveform sanity check: sine ≈0.354, square ≈0.500 (no DC).')
action('cleanup_gen','CLEANUP · DG902 CH1 OFF + verify','cleanupGen','cleanup',32000,
       'Always attempt this on completed, failed or gracefully stopped runs.')
action('cleanup_psu','CLEANUP · DP932A CH1 OFF + verify','cleanupPsu','cleanup',32000,
       'Attempted independently even if generator cleanup fails.')
assert len(steps)<=100
panel['testDefinition']={'schemaVersion':1,'id':'rigol-loopback-065',
     'title':'Rigol Four-Instrument Voltage and Waveform Verification',
     'config':{'sequenceId':'rigolTestSequence','tableId':'rigolTestTable',
         'logId':'rigolTestLog','summaryId':'rigolTestSafety',
         'stopOnFailure':False,'totalTimeoutMs':480000},
     'steps':steps}
# WARNING: native summaryId uses ui.set on each step; it will overwrite safety warning.
# Leave summaryId unset, so separate safety message remains fully controlled by operator script.
panel['testDefinition']['config'].pop('summaryId')
module_source=''.join(module)
panel['libraries'].append({'name':'RigolTestSteps.mjs','source':module_source})
panel['testLibrary']='RigolTestSteps.mjs'
output=OUT/'Rigol_Four_Instrument_Operator_Bench_v0_6_5_Automated_Tests.wfpanel'
output.write_text(json.dumps(panel,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
(OUT/'RigolBench_4Instruments_v0_6_5.js').write_text(source,encoding='utf8')
(OUT/'RigolTestSteps.mjs').write_text(module_source,encoding='utf8')
(OUT/'RigolTestDefinition_v0_6_5.wftest.json').write_text(json.dumps(panel['testDefinition'],ensure_ascii=False,indent=2)+'\n')
print('Built',output,'steps',len(steps),'numeric',sum(s['type']=='numeric' for s in steps),
      'pages',len(panel['pages']),'module exports',len(re.findall(r'export function',module_source)),
      'source bytes',len(source.encode('utf8')))
