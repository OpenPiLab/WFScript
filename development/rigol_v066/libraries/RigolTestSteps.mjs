/**
 * RigolTestSteps.mjs — Automated Rigol test procedures (v0.6.6).
 *
 * THIS is the implementation of the test. Open the Test Sequence Editor to
 * inspect step order and limits; open THIS library to understand the hardware
 * procedure for each step. The main Panel script contains UI event handlers
 * and a small bridge to the four already-connected instrument sessions.
 *
 * TestRunner owns scheduling, numeric limits, sequencing, cleanup and reports.
 * The temporary startStep/pollStep adapter is required by RC1.5.6's polling
 * contract; a future Promise-aware TestRunner can replace it without changing
 * the instrument procedures below.
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

// ───────────────────── Platform polling adapter ──────────────────────
// Most step authors should read the REAL procedures below, not this block.
function startStep(label, operation) {
    if (pending && !pending.done)
        throw Error('Previous test operation is still pending: ' + pending.label);
    var task = { label: label, generation: ++generation, done: false,
        value: undefined, error: null };
    pending = task;
    RigolTestHost.log( 'Begin ' + label);
    try {
        Promise.resolve(operation()).then(function (value) {
            if (pending !== task) return;
            task.value = value;
            task.done = true;
            RigolTestHost.log( 'Completed ' + label);
        }, function (e) {
            if (pending !== task) return;
            task.error = errorText(e);
            task.done = true;
            RigolTestHost.log( 'ERROR ' + label + ': ' + task.error);
            RigolTestHost.display('rigolTestSafety', 'TEST ERROR: ' + task.error + ' — CHECK BOTH OUTPUTS');
        });
    } catch (e) {
        task.error = errorText(e);
        task.done = true;
    }
}
function pollStep(numeric) {
    var task = pending;
    if (!task || !task.done) return undefined;
    if (task.error) throw Error(task.label + ': ' + task.error);
    if (!numeric) return true;
    var result = Number(task.value);
    if (!Number.isFinite(result)) throw Error(task.label + ' returned no finite measurement');
    return result;
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

// ───────────── Exports referenced by the Visual Test Sequence Editor ─────────────
// The current native v1 definition uses handler names (no step parameters yet).
// These tiny exports bind a human-readable procedure to each fixed test condition.
// A future parameterized runner can remove this repetitive binding section.
export function pollDone() { return pollStep(false); }
export function pollValue() { return pollStep(true); }
export function pollPreflight() {
    var result = pollStep(false);
    if (result === true) recordHardwareMetadata();
    return result;
}
function recordHardwareMetadata() {
    // Generic report.instrument.mode is still AD3-centric; these explicitly
    // describe the four real instruments that this sequence actually used.
    var fields = {rigolMode:'PHYSICAL_RIGOL_SCPI_TCP',
        rigolPsuEndpoint:RigolTestHost.endpoint('psu'),
        rigolMeterEndpoint:RigolTestHost.endpoint('meter'),
        rigolGenEndpoint:RigolTestHost.endpoint('gen'),
        rigolScopeEndpoint:RigolTestHost.endpoint('scope')};
    Object.keys(fields).forEach(function(key) {
        if(!TestRunner.setVariable(key,fields[key]))
            throw Error('Cannot record ' + key + ': ' + TestRunner.lastError);
    });
}
export function preflight() {
    lastWave = null;
    startStep('four-instrument preflight', verifyInitialState);
}
export function powerOff() {
    startStep('PSU CH1 verified OFF before waveform tests',function() {return safeRelease('psu');});
}
export function cleanupGen() {
    pending = null; // abandon interrupted step, let its active SCPI operation settle
    startStep('CLEANUP GEN CH1 OFF',function(){return safeRelease('gen');});
}
export function cleanupPsu() {
    pending = null;
    startStep('CLEANUP PSU CH1 OFF',function(){return safeRelease('psu');});
}
export function power500() { startStep('PSU CH1 5.00 V',function(){return measureSupply(5.00);}); }
export function dmm500() { startStep('DMM DCV at 5.00 V',function(){return measureDmm(5.00);}); }
export function power475() { startStep('PSU CH1 4.75 V',function(){return measureSupply(4.75);}); }
export function dmm475() { startStep('DMM DCV at 4.75 V',function(){return measureDmm(4.75);}); }
export function power450() { startStep('PSU CH1 4.50 V',function(){return measureSupply(4.50);}); }
export function dmm450() { startStep('DMM DCV at 4.50 V',function(){return measureDmm(4.50);}); }
export function power425() { startStep('PSU CH1 4.25 V',function(){return measureSupply(4.25);}); }
export function dmm425() { startStep('DMM DCV at 4.25 V',function(){return measureDmm(4.25);}); }
export function power400() { startStep('PSU CH1 4.00 V',function(){return measureSupply(4.00);}); }
export function dmm400() { startStep('DMM DCV at 4.00 V',function(){return measureDmm(4.00);}); }
export function power375() { startStep('PSU CH1 3.75 V',function(){return measureSupply(3.75);}); }
export function dmm375() { startStep('DMM DCV at 3.75 V',function(){return measureDmm(3.75);}); }
export function power350() { startStep('PSU CH1 3.50 V',function(){return measureSupply(3.50);}); }
export function dmm350() { startStep('DMM DCV at 3.50 V',function(){return measureDmm(3.50);}); }
export function power325() { startStep('PSU CH1 3.25 V',function(){return measureSupply(3.25);}); }
export function dmm325() { startStep('DMM DCV at 3.25 V',function(){return measureDmm(3.25);}); }
export function power300() { startStep('PSU CH1 3.00 V',function(){return measureSupply(3.00);}); }
export function dmm300() { startStep('DMM DCV at 3.00 V',function(){return measureDmm(3.00);}); }
export function power275() { startStep('PSU CH1 2.75 V',function(){return measureSupply(2.75);}); }
export function dmm275() { startStep('DMM DCV at 2.75 V',function(){return measureDmm(2.75);}); }
export function power250() { startStep('PSU CH1 2.50 V',function(){return measureSupply(2.50);}); }
export function dmm250() { startStep('DMM DCV at 2.50 V',function(){return measureDmm(2.50);}); }
export function power225() { startStep('PSU CH1 2.25 V',function(){return measureSupply(2.25);}); }
export function dmm225() { startStep('DMM DCV at 2.25 V',function(){return measureDmm(2.25);}); }
export function power200() { startStep('PSU CH1 2.00 V',function(){return measureSupply(2.00);}); }
export function dmm200() { startStep('DMM DCV at 2.00 V',function(){return measureDmm(2.00);}); }
export function capture_sine_1Vpp_1000Hz() { startStep('CH1 sine_1Vpp_1000Hz',function(){return captureWaveform('sine',1,1000,'sine_1Vpp_1000Hz');}); }
export function actual_vpp_sine_1Vpp_1000Hz() { return readMetric('sine_1Vpp_1000Hz','vpp'); }
export function actual_hz_sine_1Vpp_1000Hz() { return readMetric('sine_1Vpp_1000Hz','hz'); }
export function actual_shape_sine_1Vpp_1000Hz() { return readMetric('sine_1Vpp_1000Hz','shape'); }
export function capture_sine_1Vpp_5000Hz() { startStep('CH1 sine_1Vpp_5000Hz',function(){return captureWaveform('sine',1,5000,'sine_1Vpp_5000Hz');}); }
export function actual_vpp_sine_1Vpp_5000Hz() { return readMetric('sine_1Vpp_5000Hz','vpp'); }
export function actual_hz_sine_1Vpp_5000Hz() { return readMetric('sine_1Vpp_5000Hz','hz'); }
export function actual_shape_sine_1Vpp_5000Hz() { return readMetric('sine_1Vpp_5000Hz','shape'); }
export function capture_sine_2Vpp_1000Hz() { startStep('CH1 sine_2Vpp_1000Hz',function(){return captureWaveform('sine',2,1000,'sine_2Vpp_1000Hz');}); }
export function actual_vpp_sine_2Vpp_1000Hz() { return readMetric('sine_2Vpp_1000Hz','vpp'); }
export function actual_hz_sine_2Vpp_1000Hz() { return readMetric('sine_2Vpp_1000Hz','hz'); }
export function actual_shape_sine_2Vpp_1000Hz() { return readMetric('sine_2Vpp_1000Hz','shape'); }
export function capture_sine_2Vpp_5000Hz() { startStep('CH1 sine_2Vpp_5000Hz',function(){return captureWaveform('sine',2,5000,'sine_2Vpp_5000Hz');}); }
export function actual_vpp_sine_2Vpp_5000Hz() { return readMetric('sine_2Vpp_5000Hz','vpp'); }
export function actual_hz_sine_2Vpp_5000Hz() { return readMetric('sine_2Vpp_5000Hz','hz'); }
export function actual_shape_sine_2Vpp_5000Hz() { return readMetric('sine_2Vpp_5000Hz','shape'); }
export function capture_square_1Vpp_1000Hz() { startStep('CH1 square_1Vpp_1000Hz',function(){return captureWaveform('square',1,1000,'square_1Vpp_1000Hz');}); }
export function actual_vpp_square_1Vpp_1000Hz() { return readMetric('square_1Vpp_1000Hz','vpp'); }
export function actual_hz_square_1Vpp_1000Hz() { return readMetric('square_1Vpp_1000Hz','hz'); }
export function actual_shape_square_1Vpp_1000Hz() { return readMetric('square_1Vpp_1000Hz','shape'); }
export function capture_square_1Vpp_5000Hz() { startStep('CH1 square_1Vpp_5000Hz',function(){return captureWaveform('square',1,5000,'square_1Vpp_5000Hz');}); }
export function actual_vpp_square_1Vpp_5000Hz() { return readMetric('square_1Vpp_5000Hz','vpp'); }
export function actual_hz_square_1Vpp_5000Hz() { return readMetric('square_1Vpp_5000Hz','hz'); }
export function actual_shape_square_1Vpp_5000Hz() { return readMetric('square_1Vpp_5000Hz','shape'); }
export function capture_square_2Vpp_1000Hz() { startStep('CH1 square_2Vpp_1000Hz',function(){return captureWaveform('square',2,1000,'square_2Vpp_1000Hz');}); }
export function actual_vpp_square_2Vpp_1000Hz() { return readMetric('square_2Vpp_1000Hz','vpp'); }
export function actual_hz_square_2Vpp_1000Hz() { return readMetric('square_2Vpp_1000Hz','hz'); }
export function actual_shape_square_2Vpp_1000Hz() { return readMetric('square_2Vpp_1000Hz','shape'); }
export function capture_square_2Vpp_5000Hz() { startStep('CH1 square_2Vpp_5000Hz',function(){return captureWaveform('square',2,5000,'square_2Vpp_5000Hz');}); }
export function actual_vpp_square_2Vpp_5000Hz() { return readMetric('square_2Vpp_5000Hz','vpp'); }
export function actual_hz_square_2Vpp_5000Hz() { return readMetric('square_2Vpp_5000Hz','hz'); }
export function actual_shape_square_2Vpp_5000Hz() { return readMetric('square_2Vpp_5000Hz','shape'); }
