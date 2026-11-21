/**
 * RigolTestSteps.mjs — Parameterized native callback test (v0.6.7).
 *
 * THIS is the implementation of the test. Open the Test Sequence Editor to
 * inspect step order and limits; open THIS library to understand the hardware
 * procedure for each step. The main Panel script contains UI event handlers
 * and a small bridge to the four already-connected instrument sessions.
 *
 * TestRunner owns scheduling, numeric limits, sequencing, cleanup and reports.
 * RC1.5.9 provides callback completion and JSON parameters. The instrument
 * service and physical safety procedures below are the v0.6.6 implementation.
 *
 * No async/await, Promise.finally, object spread, or duplicate TCP sessions.
 * Physical ON/OFF is established only through getOutput readbacks. On network
 * failure, no software implementation can guarantee an external output is OFF.
 */
import { invoke } from './WFScpiCore.mjs';

var pending = null;
var owned = { psu: false, gen: false };
var lastWave = null;
var generation = 0;
var waveSerial = 0;
function errorText(e) { return String(e && (e.message || e.stack) || e); }

/** Operator/connection preconditions; does not change any physical output. */
export function checkReady() {
    if (RigolTestHost.stopping()) throw Error('Panel is stopping');
    ['psu','meter','gen','scope'].forEach(function(which) {
        if (!RigolTestHost.connected(which))
            throw Error('Connect all four instruments on their normal Pages before RUN (' + which + ')');
        if (RigolTestHost.operationActive(which))
            throw Error(RigolTestHost.label(which) + ' is processing another operation.');
    });
    if (RigolTestHost.meterMonitoring())
        throw Error('Stop the DMM monitoring timer before starting the Test Executive');
    if (!RigolTestHost.wiringConfirmed())
        throw Error('Check the physical wiring, then enable WIRING CONFIRMED');
}

/** Optional status for the operator page; never acts as a test step itself. */
export function state() { return { psuOwned: owned.psu, genOwned: owned.gen,
    active: !!(pending && !pending.done), lastWave: lastWave }; }
export function finish() { pending = null; return state(); }
export function clearLastWave() { lastWave = null; }

// ───── Native C++ callback steps (no JS pollValue adapter) ─────
// One helper binds Promise-based SCPI operations to the native executive.
// It does NOT serialize devices or override existing physical output interlocks.
function executeNative(step,label,task,numeric) {
    if (pending && !pending.done)
        throw Error('Previous operation still pending: '+pending.label);
    var op={label:label,done:false};
    pending=op;
    ++generation; // invalidates continuations from a stopped/previous step
    RigolTestHost.log('Begin '+label);
    try {
        Promise.resolve(task()).then(function(result) {
            op.done=true;
            if(pending!==op || !TestRunner.isActive(step.token))return;
            if(numeric===true && !Number.isFinite(Number(result))) {
                TestRunner.fail(step.token,label+' returned a nonfinite measurement');return;
            }
            RigolTestHost.log('Completed '+label);
            TestRunner.complete(step.token,numeric==='structured'?result:(numeric?Number(result):true));
        },function(error) {
            op.done=true;
            if(pending!==op || !TestRunner.isActive(step.token))return;
            var detail=errorText(error);
            RigolTestHost.log('ERROR '+label+': '+detail);
            RigolTestHost.display('rigolTestSafety','TEST ERROR: '+detail+' — CHECK BOTH OUTPUTS');
            TestRunner.fail(step.token,detail);
        });
    } catch(error) {
        op.done=true;
        if(TestRunner.isActive(step.token))TestRunner.fail(step.token,errorText(error));
    }
}
function runInstrument(which, label, task, timeoutMs) {
    // Controlled use of the OPERATOR PANEL'S EXISTING serialized SCPI session.
    return RigolTestHost.run(which, label, task, timeoutMs || 30000);
}

// ───────────────────── Named test procedures ──────────────────────
// Setup: verify wiring and physical output state; configure meter.
function verifyInitialState() {
    var transaction=generation;
    checkReady();
    // Read both physical outputs before configuring anything. NEVER turn off
    // an already-energized operator-owned output as part of preflight.
    return Promise.all([
        runInstrument('psu', 'PSU CH1 initial OFF', function (d) {
            return invoke(d.ch1, 'getOutput');
        }),
        runInstrument('gen', 'GEN CH1 initial OFF', function (d) {
            return invoke(d.ch1, 'getOutput');
        }),
        runInstrument('scope', 'scope CH1 settings readback', function (d) {
            return invoke(d.ch1, 'getSettings');
        }),
        runInstrument('meter', 'DMM identity readback', function (d) {
            return invoke(d.scpi, 'query', ['*IDN?']);
        })
    ]).then(function (r) {
        if(generation!==transaction)throw Error('Preflight canceled');
        if (r[0] !== false) throw Error('DP932A CH1 must be physically OFF before RUN');
        if (r[1] !== false) throw Error('DG902 CH1 must be physically OFF before RUN');
        // Only clear prior ownership if both physical outputs are verified OFF.
        owned={psu:false,gen:false};
        var s = r[2];
        if (!s || !Number.isFinite(Number(s.scale))) throw Error('MHO954 CH1 settings could not be verified');
        RigolTestHost.log( 'Preflight IDN=' + String(r[3]).slice(0,120));
        return runInstrument('meter','set DMM DCV 10V / 5 PLC',function(d) {
            return invoke(d.configure,'dcVoltage',[{range:10,nplc:5}]);
        });
    });
}
/** DC SWEEP: configure/read PSU CH1; 0.20 A current, OVP/OCP enabled. */
function measureSupply(target) {
    var transaction=generation;
    if (!Number.isFinite(target) || target < 2 || target > 5 || Math.round(target * 4) !== target * 4)
        throw Error('Invalid PSU sweep setpoint ' + target);
    return runInstrument('psu','set/measure PSU CH1 ' + target.toFixed(2) + ' V',function (d,op) {
        var c = d.ch1;
        var prep;
        if (!owned.psu) {
            owned.psu = true; // Own the channel BEFORE any asynchronous setup/enable sequence.
            prep = invoke(c,'setup',[{ voltage:5, current:0.20,
                ovp:5.50, ocp:0.25, enableOVP:true, enableOCP:true }])
                .then(function () {
                    op.ensure();
                    if(generation!==transaction)throw Error('PSU step canceled before enable');
                    // OWNED before enabling, including lost replies and timeouts.
                    return invoke(c,'enable');
                }).then(function () {op.ensure(); return invoke(c,'getOutput');})
                .then(function (on) {if (on !== true) throw Error('PSU CH1 ON not verified');});
        } else {
            prep = invoke(c,'getOutput').then(function(on) {
                if(on!==true)throw Error('PSU CH1 unexpectedly OFF');
                return invoke(c,'setVoltage',[target]);
            });
        }
        return prep.then(function () {op.ensure();return op.delay(250);})
            .then(function () {op.ensure();return invoke(c,'measureVoltage');})
            .then(function (voltage) {
                op.ensure();
                if(generation!==transaction)throw Error('PSU measurement canceled');
                var n=Number(voltage);
                if(!Number.isFinite(n))throw Error('PSU CH1 measurement is not finite');
                RigolTestHost.setValue('psuVRead1',n);
                RigolTestHost.display('psuLive1','CH1 Test Executive measured ' + n.toFixed(4) + ' V');
                return n;
            });
    },35000);
}
/** DC SWEEP: independent DM858 DC voltage reading at the current setpoint. */
function measureDmm(expected) {
    var transaction=generation;
    return runInstrument('meter','DMM DCV at ' + expected.toFixed(2) + ' V',function (d,op) {
        // Use READ? after the five-PLC configuration; MEAS? would reconfigure.
        return invoke(d.acquisition,'read').then(function (reading) {
            op.ensure();
            if(generation!==transaction)throw Error('DMM measurement canceled');
            if(Array.isArray(reading)) {
                if(reading.length!==1)throw Error('Expected one DC voltage reading');
                reading=reading[0];
            }
            if (!reading || reading.overload || reading.function!=='dcVoltage')
                throw Error('DM858 DCV overload, function mismatch, or missing reading');
            var value=Number(reading.value);
            if (!Number.isFinite(value))throw Error('Invalid DM858 DC voltage');
            RigolTestHost.setValue('meterLarge',value);
            RigolTestHost.display('meterValue','TEST DCV ' + value.toFixed(5) + ' V · expected ' + expected.toFixed(2) + ' V');
            return value;
        });
    },30000);
}
function supplyOff() {
    // Only release outputs which THIS run owned. If preflight failed on
    // already-on channels, do not silently change existing physical state.
    if (!owned.psu) return Promise.resolve(true);
    return runInstrument('psu','CH1 OFF + physical readback',function(d,op) {
        return invoke(d.ch1,'disable').then(function() {op.ensure();return invoke(d.ch1,'getOutput');})
            .then(function(on) {
                if(on!==false)throw Error('DP932A CH1 OFF unconfirmed');
                owned.psu=false;
                RigolTestHost.markUnprepared('psu');
                RigolTestHost.display('psuLive1','CH1 OFF verified after Test Executive');
                return true;
            });
    },30000);
}
function generatorOff() {
    if (!owned.gen) return Promise.resolve(true);
    return runInstrument('gen','CH1 OFF + physical readback',function(d,op) {
        return invoke(d.ch1,'disable').then(function() {op.ensure();return invoke(d.ch1,'getOutput');})
            .then(function(on) {
                if(on!==false)throw Error('DG902 CH1 OFF unconfirmed');
                owned.gen=false;
                RigolTestHost.markUnprepared('gen');
                RigolTestHost.display('genReadback','CH1 OFF verified after Test Executive');
                return true;
            });
    },30000);
}
function analyzeWaveform(frame, expected) {
    if (!frame || !frame.ch1 || !frame.ch1.voltage || frame.ch1.voltage.length!==1000 ||
        !frame.time || frame.time.length!==1000)
        throw Error('Scope did not return a complete CH1 1000-point NORMAL capture');
    var samples=frame.ch1.voltage, times=frame.time;
    var lo=Infinity,hi=-Infinity,sum=0;
    for(var i=0;i<1000;i++) {
        var v=Number(samples[i]);
        if(!Number.isFinite(v) || !Number.isFinite(Number(times[i])))throw Error('Invalid CH1 data');
        lo=Math.min(lo,v); hi=Math.max(hi,v); sum+=v;
        if(i && !(Number(times[i])>Number(times[i-1])))throw Error('Non-monotonic scope timestamps');
    }
    var span=hi-lo,mean=sum/1000;
    if(!(span>0.08))throw Error('Scope waveform is flat or unavailable');
    var variance=0;
    for(var j=0;j<1000;j++)variance+=(samples[j]-mean)*(samples[j]-mean);
    var rms=Math.sqrt(variance/1000);
    // Hysteretic rising-midpoint detection with linear crossing interpolation.
    // The hysteresis prevents noise/ringing from producing multiple cycles.
    var mid=(hi+lo)/2, lower=mid-span*.15, upper=mid+span*.15;
    var armed=false, crossings=[];
    for(var k=1;k<1000;k++) {
        if (samples[k]<=lower) armed=true;
        if(armed && samples[k-1]<mid && samples[k]>=mid && samples[k]>=samples[k-1]) {
            var part=(mid-samples[k-1])/(samples[k]-samples[k-1]);
            crossings.push(Number(times[k-1])+part*(Number(times[k])-Number(times[k-1])));
            armed=false;
        }
    }
    var periods=[];
    for(var x=1;x<crossings.length;x++) {
        var interval=crossings[x]-crossings[x-1];
        if(interval>0)periods.push(interval);
    }
    if(periods.length<3)throw Error('Insufficient stable CH1 cycles (' + periods.length + ')');
    periods.sort(function(a,b){return a-b;});
    var median=periods[Math.floor(periods.length/2)];
    var frequency=1/median;
    if(!Number.isFinite(frequency))throw Error('Invalid CH1 frequency estimate');
    var metrics={vpp:span,hz:frequency,shape:rms/span,cycles:periods.length,
        rms:rms, expected:expected};
    RigolTestHost.log('CH1 '+ expected.key + ' measured '+JSON.stringify(metrics));
    return metrics;
}
/** WAVEFORM TEST: configure DG902, acquire MHO954 CH1, analyze 1000 samples. */
function captureWaveform(shape,amplitude,frequency,key) {
    if(['sine','square'].indexOf(shape)<0 || [1,2].indexOf(amplitude)<0 ||
        [1000,5000].indexOf(frequency)<0)throw Error('Unsupported waveform test conditions');
    lastWave=null;
    var transaction=generation;
    var record={shape:shape,amplitude:amplitude,frequency:frequency,key:key};
    var hadOwnership=owned.gen;
    var ch=null;
    // Defensive interlock: independently verify the PSU before any generator setup,
    // even if a user has edited the visual sequence and removed its PSU-OFF step.
    if (owned.psu) throw Error('PSU CH1 remains owned: execute PSU OFF before waveforms');
    return runInstrument('psu', 'verify PSU CH1 OFF before ' + key, function(d) {
        return invoke(d.ch1, 'getOutput').then(function(on) {
            if (on !== false) throw Error('PSU CH1 is physically ON; waveform test refused');
            return true;
        });
    }, 15000).then(function() {
        if (generation !== transaction) throw Error('Waveform setup canceled');
        return runInstrument('gen','configure CH1 '+key,function(d,op) {
        ch=d.ch1;
        // Verify operator-owned state before taking ownership. Once confirmed,
        // own the channel BEFORE the first asynchronous configuration or enable.
        return ensureGeneratorOffInOperation(d,op,hadOwnership).then(function () {
            op.ensure();
            return invoke(ch,'configure',[{shape:shape,frequency:frequency,
                amplitude:amplitude,offset:0,phase:0,load:'highZ',
                duty:shape==='square'?50:undefined}]);
        }).then(function (settings) {
            op.ensure();
            if(!settings)throw Error('Generator settings not returned');
            return invoke(ch,'enable');
        }).then(function () {op.ensure();return invoke(ch,'getOutput');})
            .then(function(on) {if(on!==true)throw Error('Generator CH1 ON unconfirmed');return true;});
        },50000);
    }).then(function() {
        if(generation!==transaction)throw Error('Waveform capture canceled before scope setup');
        return runInstrument('scope','capture CH1 '+key,function(d,op) {
            var scale=amplitude===1?0.20:0.50;
            return invoke(d.ch1,'configure',[{enabled:true,impedance:1e6,probe:1,
                coupling:'DC',offset:0,scale:scale}])
                .then(function() {op.ensure();return invoke(d.timebase,'configure',[{mode:'MAIN',scale:1/frequency}]);})
                .then(function() {op.ensure();return invoke(d.trigger,'edge',[{channel:1,slope:'POS',level:0,sweep:'AUTO'}]);})
                .then(function() {op.ensure();return invoke(d.acquisition,'run');})
                .then(function() {op.ensure();return op.delay(400);})
                .then(function() {op.ensure();return invoke(d.acquisition,'stop');})
                .then(function() {op.ensure();return invoke(d.acquisition,'status');})
                .then(function(status) {if(status!=='STOP')throw Error('MHO954 STOP not confirmed, got '+status);
                    op.ensure();return invoke(d.waveform,'captureAll',[{channels:[1],mode:'NORMAL',
                        format:'BYTE',start:1,points:1000,chunkPoints:1000,freeze:false}]);})
                .then(function(frame) {
                    op.ensure();
                    if(generation!==transaction)throw Error('Waveform capture canceled');
                    var metrics=analyzeWaveform(frame,record);
                    lastWave=metrics;
                    var serial=++waveSerial;
                    RigolTestHost.renderScopeFrame(frame,serial); // reuses existing coloured CH1 plot
                    RigolTestHost.display('rigolTestLatest','CH1 '+key+'  '+metrics.vpp.toFixed(3)+' Vpp  '+metrics.hz.toFixed(1)+' Hz  RMS/Vpp '+metrics.shape.toFixed(3));
                    return metrics;
                });
        },65000);
    });
}
function ensureGeneratorOffInOperation(d,op,hadOwnership) {
    // Called inside a serialized generator operation; do NOT nest another runInstrument.
    return invoke(d.ch1,'getOutput').then(function(on) {
        op.ensure();
        if(!hadOwnership && on===true)
            throw Error('Generator CH1 unexpectedly ON before first test');
        owned.gen = true; // Before any asynchronous configure/enable; no gap after check.
        if(on!==true)return true;
        return invoke(d.ch1,'disable').then(function(){op.ensure();return invoke(d.ch1,'getOutput');})
            .then(function(after) {if(after!==false)throw Error('CH1 OFF not verified before configure');return true;});
    });
}
function readMetric(key,metric) {
    if(!lastWave || lastWave.expected.key!==key)throw Error('No matching physical scope capture for '+key);
    var number=lastWave[metric];
    if(!Number.isFinite(number))throw Error('Invalid '+metric+' for '+key);
    return number;
}
function waitUntilIdle(which,limitMs) {
    var began=Date.now();
    return new Promise(function(resolve,reject) {
        function check() {
            if(!RigolTestHost.operationActive(which))return resolve(true);
            if(Date.now()-began>=limitMs)
                return reject(Error(RigolTestHost.label(which)+' is still BUSY; hardware OFF cannot be safely confirmed'));
            setTimeout(check,100);
        }
        check();
    });
}
/** CLEANUP: wait for pending instrument operation and verify physical OFF. */
function safeRelease(which) {
    var doesOwn=owned[which];
    if(!doesOwn)return Promise.resolve(true);
    if(!RigolTestHost.connected(which))
        return Promise.reject(Error(RigolTestHost.label(which)+' disconnected; CHECK physical CH1 output manually'));
    return waitUntilIdle(which,12000).then(function() {
        return which==='psu'?supplyOff():generatorOff();
    });
}

// ───────────── Parameterized native callbacks: the step library's public API ─────────────
// Step params are defined in the Visual Test Sequence Editor, not here.
function recordHardwareMetadata() {
    var fields={rigolMode:'PHYSICAL_RIGOL_SCPI_TCP',
        rigolPsuEndpoint:RigolTestHost.endpoint('psu'),
        rigolMeterEndpoint:RigolTestHost.endpoint('meter'),
        rigolGenEndpoint:RigolTestHost.endpoint('gen'),
        rigolScopeEndpoint:RigolTestHost.endpoint('scope')};
    Object.keys(fields).forEach(function(key){
        if(!TestRunner.setVariable(key,fields[key]))
            throw Error('Cannot record '+key+': '+TestRunner.lastError);
    });
}
export function testPreflight(step) {
    lastWave=null;
    recordHardwareMetadata();
    executeNative(step,'Four-instrument preflight',verifyInitialState,false);
}
export function setAndReadPsu(step) {
    var voltage=Number(step.params.voltage);
    executeNative(step,'PSU CH1 '+voltage.toFixed(2)+' V',function(){
        return measureSupply(voltage);
    },true);
}
export function readDmmAt(step) {
    var expected=Number(step.params.voltage);
    executeNative(step,'DMM at '+expected.toFixed(2)+' V',function(){
        return measureDmm(expected);
    },true);
}
export function switchPsuOff(step) {
    executeNative(step,'PSU CH1 verified OFF',function(){return safeRelease('psu');},false);
}
export function generateAndCapture(step) {
    var p=step.params;
    executeNative(step,'CH1 '+p.key,function(){
        return captureWaveform(p.shape,p.amplitude,p.frequency,p.key);
    },false);
}
export function readCaptureMetric(step) {
    return readMetric(step.params.key,step.params.metric);
}
export function releaseGenerator(step) {
    pending=null; // abandon any interrupted operation, wait for device idle
    executeNative(step,'CLEANUP GEN CH1 OFF',function(){return safeRelease('gen');},false);
}
export function releaseSupply(step) {
    pending=null;
    executeNative(step,'CLEANUP PSU CH1 OFF',function(){return safeRelease('psu');},false);
}

// RC1.5.10: one waveform capture yields 3 independently evaluated results.
// Retain existing operation ownership, safe shutdown and SCPI serialization.
export function generateAndVerifyWaveform(step) {
    var p=step.params;
    var key=p.key || (p.shape+'_'+p.amplitude+'Vpp_'+p.frequency+'Hz');
    executeNative(step,'CH1 '+key,function(){
        return captureWaveform(p.shape,p.amplitude,p.frequency,key).then(function(metrics){
            return {vpp:metrics.vpp,hz:metrics.hz,shape:metrics.shape,
                    rms:metrics.rms,cycles:metrics.cycles,source:'PHYSICAL_RIGOL_SCPI_TCP'};
        });
    },'structured');
}
