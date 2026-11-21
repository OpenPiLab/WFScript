/*
 * Rigol Test Station v0.6.5 — panel-local adapter for the native Test Executive.
 * This is appended to the existing v0.6.4 operator script; SCPI drivers stay unchanged.
 * The project-owned .wfpanel Test Sequence defines order, timeouts and limits.
 * The .mjs test library invokes only the methods on this global object.
 *
 * Output ownership is asserted BEFORE issuing enable; cleanup always attempts
 * both owned outputs independently and VERIFYs OFF. Network failure cannot
 * guarantee physical OFF: the operator must check the instrument front panels.
 */
var rigolTestRunning = false;
var rigolTestPending = null;
var rigolTestOwnership = { psu: false, gen: false };
var rigolTestWave = null;
var rigolTestGeneration = 0;
var rigolTestWaveSerial = 0;

function rigolTestCheckReady() {
    if (runtimeStopping) throw Error('Panel is stopping');
    if (!dev.psu || !dev.psu.connected || !dev.meter || !dev.meter.connected ||
        !dev.gen || !dev.gen.connected || !dev.scope || !dev.scope.connected)
        throw Error('Connect all four instruments on their normal Pages before running this test');
    ['psu','meter','gen','scope'].forEach(function (key) {
        if (operations[key] && operations[key].active)
            throw Error(DEVICE[key] + ' is processing another operation. Wait for completion.');
    });
    if (meterMonitorTimer !== null)
        throw Error('Stop the DMM monitoring timer before starting the Test Executive');
    var ack = ui.get('rigolTestWiring');
    if (!(ack === true || ack === 1 || ack === '1'))
        throw Error('Check the physical wiring, then enable WIRING CONFIRMED');
}
function rigolTestBegin(label, operation) {
    if (rigolTestPending && !rigolTestPending.done)
        throw Error('Previous test operation is still pending: ' + rigolTestPending.label);
    var task = { label: label, generation: ++rigolTestGeneration, done: false,
        value: undefined, error: null };
    rigolTestPending = task;
    log('TEST', 'Begin ' + label);
    try {
        Promise.resolve(operation()).then(function (value) {
            if (rigolTestPending !== task) return;
            task.value = value;
            task.done = true;
            log('TEST', 'Completed ' + label);
        }, function (e) {
            if (rigolTestPending !== task) return;
            task.error = textErr(e);
            task.done = true;
            log('TEST', 'ERROR ' + label + ': ' + task.error);
            put('rigolTestSafety', 'TEST ERROR: ' + task.error + ' — CHECK BOTH OUTPUTS');
        });
    } catch (e) {
        task.error = textErr(e);
        task.done = true;
    }
}
function rigolTestPoll(numeric) {
    var task = rigolTestPending;
    if (!task || !task.done) return undefined;
    if (task.error) throw Error(task.label + ': ' + task.error);
    if (!numeric) return true;
    var result = Number(task.value);
    if (!Number.isFinite(result)) throw Error(task.label + ' returned no finite measurement');
    return result;
}
function rigolTestRunP(which, label, task, timeoutMs) {
    return runP(which, 'TEST ' + label, task,
        { testOwned: true, quiet: true, timeoutMs: timeoutMs || 30000 });
}
function rigolTestPreflight() {
    var transaction=rigolTestGeneration;
    rigolTestCheckReady();
    // Read both physical outputs before configuring anything. NEVER turn off
    // an already-energized operator-owned output as part of preflight.
    return Promise.all([
        rigolTestRunP('psu', 'PSU CH1 initial OFF', function (d) {
            return callP(d.ch1, 'getOutput');
        }),
        rigolTestRunP('gen', 'GEN CH1 initial OFF', function (d) {
            return callP(d.ch1, 'getOutput');
        }),
        rigolTestRunP('scope', 'scope CH1 settings readback', function (d) {
            return callP(d.ch1, 'getSettings');
        }),
        rigolTestRunP('meter', 'DMM identity readback', function (d) {
            return callP(d.scpi, 'query', ['*IDN?']);
        })
    ]).then(function (r) {
        if(rigolTestGeneration!==transaction)throw Error('Preflight canceled');
        if (r[0] !== false) throw Error('DP932A CH1 must be physically OFF before RUN');
        if (r[1] !== false) throw Error('DG902 CH1 must be physically OFF before RUN');
        // Only clear prior ownership if both physical outputs are verified OFF.
        rigolTestOwnership={psu:false,gen:false};
        var s = r[2];
        if (!s || !Number.isFinite(Number(s.scale))) throw Error('MHO954 CH1 settings could not be verified');
        log('TEST', 'Preflight IDN=' + String(r[3]).slice(0,120));
        return rigolTestRunP('meter','set DMM DCV 10V / 5 PLC',function(d) {
            return callP(d.configure,'dcVoltage',[{range:10,nplc:5}]);
        });
    });
}
function rigolTestPsuVoltage(target) {
    var transaction=rigolTestGeneration;
    if (!Number.isFinite(target) || target < 2 || target > 5 || Math.round(target * 4) !== target * 4)
        throw Error('Invalid PSU sweep setpoint ' + target);
    return rigolTestRunP('psu','set/measure PSU CH1 ' + target.toFixed(2) + ' V',function (d,op) {
        var c = d.ch1;
        var prep;
        if (!rigolTestOwnership.psu) {
            rigolTestOwnership.psu = true; // Own the channel BEFORE any asynchronous setup/enable sequence.
            prep = callP(c,'setup',[{ voltage:5, current:0.20,
                ovp:5.50, ocp:0.25, enableOVP:true, enableOCP:true }])
                .then(function () {
                    op.ensure();
                    if(rigolTestGeneration!==transaction)throw Error('PSU step canceled before enable');
                    // OWNED before enabling, including lost replies and timeouts.
                    return callP(c,'enable');
                }).then(function () {op.ensure(); return callP(c,'getOutput');})
                .then(function (on) {if (on !== true) throw Error('PSU CH1 ON not verified');});
        } else {
            prep = callP(c,'getOutput').then(function(on) {
                if(on!==true)throw Error('PSU CH1 unexpectedly OFF');
                return callP(c,'setVoltage',[target]);
            });
        }
        return prep.then(function () {op.ensure();return op.delay(250);})
            .then(function () {op.ensure();return callP(c,'measureVoltage');})
            .then(function (voltage) {
                op.ensure();
                if(rigolTestGeneration!==transaction)throw Error('PSU measurement canceled');
                var n=Number(voltage);
                if(!Number.isFinite(n))throw Error('PSU CH1 measurement is not finite');
                ui.set('psuVRead1',n);
                put('psuLive1','CH1 Test Executive measured ' + n.toFixed(4) + ' V');
                return n;
            });
    },35000);
}
function rigolTestMeterVoltage(expected) {
    var transaction=rigolTestGeneration;
    return rigolTestRunP('meter','DMM DCV at ' + expected.toFixed(2) + ' V',function (d,op) {
        // Use READ? after the five-PLC configuration; MEAS? would reconfigure.
        return callP(d.acquisition,'read').then(function (reading) {
            op.ensure();
            if(rigolTestGeneration!==transaction)throw Error('DMM measurement canceled');
            if(Array.isArray(reading)) {
                if(reading.length!==1)throw Error('Expected one DC voltage reading');
                reading=reading[0];
            }
            if (!reading || reading.overload || reading.function!=='dcVoltage')
                throw Error('DM858 DCV overload, function mismatch, or missing reading');
            var value=Number(reading.value);
            if (!Number.isFinite(value))throw Error('Invalid DM858 DC voltage');
            ui.set('meterLarge',value);
            put('meterValue','TEST DCV ' + value.toFixed(5) + ' V · expected ' + expected.toFixed(2) + ' V');
            return value;
        });
    },30000);
}
function rigolTestPowerOff() {
    // Only release outputs which THIS run owned. If preflight failed on
    // already-on channels, do not silently change existing physical state.
    if (!rigolTestOwnership.psu) return Promise.resolve(true);
    return rigolTestRunP('psu','CH1 OFF + physical readback',function(d,op) {
        return callP(d.ch1,'disable').then(function() {op.ensure();return callP(d.ch1,'getOutput');})
            .then(function(on) {
                if(on!==false)throw Error('DP932A CH1 OFF unconfirmed');
                rigolTestOwnership.psu=false;
                preparedPsu[1]=false;
                put('psuLive1','CH1 OFF verified after Test Executive');
                return true;
            });
    },30000);
}
function rigolTestGeneratorOff() {
    if (!rigolTestOwnership.gen) return Promise.resolve(true);
    return rigolTestRunP('gen','CH1 OFF + physical readback',function(d,op) {
        return callP(d.ch1,'disable').then(function() {op.ensure();return callP(d.ch1,'getOutput');})
            .then(function(on) {
                if(on!==false)throw Error('DG902 CH1 OFF unconfirmed');
                rigolTestOwnership.gen=false;
                preparedGen[1]=false;
                put('genReadback','CH1 OFF verified after Test Executive');
                return true;
            });
    },30000);
}
function rigolTestAnalyze(frame, expected) {
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
    log('TEST','CH1 '+ expected.key + ' measured '+JSON.stringify(metrics));
    return metrics;
}
function rigolTestWaveCondition(shape,amplitude,frequency,key) {
    if(['sine','square'].indexOf(shape)<0 || [1,2].indexOf(amplitude)<0 ||
        [1000,5000].indexOf(frequency)<0)throw Error('Unsupported waveform test conditions');
    rigolTestWave=null;
    var transaction=rigolTestGeneration;
    var record={shape:shape,amplitude:amplitude,frequency:frequency,key:key};
    var hadOwnership=rigolTestOwnership.gen;
    rigolTestOwnership.gen=true; // Includes in-flight configure/enable on STOP.
    var ch=null;
    return rigolTestRunP('gen','configure CH1 '+key,function(d,op) {
        ch=d.ch1;
        // All configurations are only performed while physical CH1 is OFF.
        return rigolTestGeneratorOffWithinOperation(d,op,hadOwnership).then(function () {
            op.ensure();
            return callP(ch,'configure',[{shape:shape,frequency:frequency,
                amplitude:amplitude,offset:0,phase:0,load:'highZ',
                duty:shape==='square'?50:undefined}]);
        }).then(function (settings) {
            op.ensure();
            if(!settings)throw Error('Generator settings not returned');
            return callP(ch,'enable');
        }).then(function () {op.ensure();return callP(ch,'getOutput');})
            .then(function(on) {if(on!==true)throw Error('Generator CH1 ON unconfirmed');return true;});
    },50000).then(function() {
        if(rigolTestGeneration!==transaction)throw Error('Waveform capture canceled before scope setup');
        return rigolTestRunP('scope','capture CH1 '+key,function(d,op) {
            var scale=amplitude===1?0.20:0.50;
            return callP(d.ch1,'configure',[{enabled:true,impedance:1e6,probe:1,
                coupling:'DC',offset:0,scale:scale}])
                .then(function() {op.ensure();return callP(d.timebase,'configure',[{mode:'MAIN',scale:1/frequency}]);})
                .then(function() {op.ensure();return callP(d.trigger,'edge',[{channel:1,slope:'POS',level:0,sweep:'AUTO'}]);})
                .then(function() {op.ensure();return callP(d.acquisition,'run');})
                .then(function() {op.ensure();return op.delay(400);})
                .then(function() {op.ensure();return callP(d.acquisition,'stop');})
                .then(function() {op.ensure();return callP(d.acquisition,'status');})
                .then(function(status) {if(status!=='STOP')throw Error('MHO954 STOP not confirmed, got '+status);
                    op.ensure();return callP(d.waveform,'captureAll',[{channels:[1],mode:'NORMAL',
                        format:'BYTE',start:1,points:1000,chunkPoints:1000,freeze:false}]);})
                .then(function(frame) {
                    op.ensure();
                    if(rigolTestGeneration!==transaction)throw Error('Waveform capture canceled');
                    var metrics=rigolTestAnalyze(frame,record);
                    rigolTestWave=metrics;
                    var serial=++rigolTestWaveSerial;
                    renderScreenFrame(frame,[1],serial); // reuses existing coloured CH1 plot
                    put('rigolTestLatest','CH1 '+key+'  '+metrics.vpp.toFixed(3)+' Vpp  '+metrics.hz.toFixed(1)+' Hz  RMS/Vpp '+metrics.shape.toFixed(3));
                    return metrics;
                });
        },65000);
    });
}
function rigolTestGeneratorOffWithinOperation(d,op,hadOwnership) {
    // Called from the already-owned generator runP; do NOT nest a runP.
    return callP(d.ch1,'getOutput').then(function(on) {
        op.ensure();
        if(!hadOwnership && on===true)
            throw Error('Generator CH1 unexpectedly ON before first test');
        if(on!==true)return true;
        return callP(d.ch1,'disable').then(function(){op.ensure();return callP(d.ch1,'getOutput');})
            .then(function(after) {if(after!==false)throw Error('CH1 OFF not verified before configure');return true;});
    });
}
function rigolTestWaveMetric(key,metric) {
    if(!rigolTestWave || rigolTestWave.expected.key!==key)throw Error('No matching physical scope capture for '+key);
    var number=rigolTestWave[metric];
    if(!Number.isFinite(number))throw Error('Invalid '+metric+' for '+key);
    return number;
}
function rigolTestWaitIdle(which,limitMs) {
    var began=Date.now();
    return new Promise(function(resolve,reject) {
        function check() {
            if(!operations[which] || !operations[which].active)return resolve(true);
            if(Date.now()-began>=limitMs)
                return reject(Error(DEVICE[which]+' is still BUSY; hardware OFF cannot be safely confirmed'));
            setTimeout(check,100);
        }
        check();
    });
}
function rigolTestSafeRelease(which) {
    var owned=rigolTestOwnership[which];
    if(!owned)return Promise.resolve(true);
    if(!dev[which] || !dev[which].connected)
        return Promise.reject(Error(DEVICE[which]+' disconnected; CHECK physical CH1 output manually'));
    return rigolTestWaitIdle(which,12000).then(function() {
        return which==='psu'?rigolTestPowerOff():rigolTestGeneratorOff();
    });
}
var RigolTestStation = {
    recordHardwareMetadata: function() {
        // Report.instrument.mode is AD3-centric in the current runtime.
        // These explicit project test variables describe the four live SCPI endpoints.
        var fields={ rigolMode:'PHYSICAL_RIGOL_SCPI_TCP',
            rigolPsuEndpoint:endpoint('psu').host+':'+endpoint('psu').port,
            rigolMeterEndpoint:endpoint('meter').host+':'+endpoint('meter').port,
            rigolGenEndpoint:endpoint('gen').host+':'+endpoint('gen').port,
            rigolScopeEndpoint:endpoint('scope').host+':'+endpoint('scope').port };
        Object.keys(fields).forEach(function(key) {
            if(!TestRunner.setVariable(key,fields[key]))
                throw Error('Cannot record '+key+': '+TestRunner.lastError);
        });
    },
    beginPreflight: function() {
        rigolTestWave=null;
        rigolTestBegin('four-instrument preflight',rigolTestPreflight);
    },
    beginPower: function(volts) {
        rigolTestBegin('PSU CH1 '+volts.toFixed(2)+' V',function(){return rigolTestPsuVoltage(volts);});
    },
    beginDmm: function(volts) {
        rigolTestBegin('DM858 at '+volts.toFixed(2)+' V',function(){return rigolTestMeterVoltage(volts);});
    },
    beginPowerOff: function() {
        rigolTestBegin('PSU CH1 verified OFF before waveform tests',rigolTestSafeRelease.bind(null,'psu'));
    },
    beginWave: function(shape,amp,hz,key) {
        rigolTestBegin('CH1 '+key,function(){return rigolTestWaveCondition(shape,amp,hz,key);});
    },
    metric: rigolTestWaveMetric,
    pollAction: function(){return rigolTestPoll(false);},
    pollNumber: function(){return rigolTestPoll(true);},
    beginCleanup: function(which) {
        // Invalidates callbacks from the interrupted step; any in-flight
        // SCPI operation is allowed to settle before the OFF request.
        rigolTestPending=null;
        rigolTestBegin('CLEANUP '+which.toUpperCase()+' CH1 OFF',function(){return rigolTestSafeRelease(which);});
    }
};
function onRigolTestRun() {
    if(rigolTestRunning) {put('rigolTestSafety','Test already running');return;}
    try {rigolTestCheckReady();}
    catch(e) {put('rigolTestSafety','Cannot start: '+textErr(e));return;}
    rigolTestRunning=true;
    put('rigolTestSafety','RUNNING — controlled outputs may be live. DO NOT disconnect cables.');
    if(!TestRunner.run()) {
        rigolTestRunning=false;
        put('rigolTestSafety','RUN rejected: '+TestRunner.lastError);
    }
}
function onRigolTestStop() {
    if(!TestRunner.terminate())put('rigolTestSafety','STOP rejected: '+TestRunner.lastError);
    else put('rigolTestSafety','STOP requested — cleanup underway. Verify physical outputs OFF.');
}
function onRigolTestReset() {
    if(rigolTestRunning) {put('rigolTestSafety','Wait for cleanup to finish before RESET.');return;}
    if(!TestRunner.reset())put('rigolTestSafety','RESET rejected: '+TestRunner.lastError);
    else {put('rigolTestSafety','READY — check wiring and outputs before next RUN');rigolTestWave=null;}
}
function onRigolTestExportJson() {
    var path=TestRunner.exportReport('json');
    put('rigolTestReport',path?'JSON report: '+path:'Export error: '+TestRunner.lastError);
}
function onRigolTestExportCsv() {
    var path=TestRunner.exportReport('csv');
    put('rigolTestReport',path?'CSV report: '+path:'Export error: '+TestRunner.lastError);
}
TestRunner.onFinished(function(summary) {
    rigolTestRunning=false;
    rigolTestPending=null;
    var off=(rigolTestOwnership.psu?'PSU CH1 NOT VERIFIED OFF':'PSU CH1 released/untouched')+
        ' · '+(rigolTestOwnership.gen?'GEN CH1 NOT VERIFIED OFF':'GEN CH1 released/untouched');
    var archive=TestRunner.archiveInfo();
    put('rigolTestSafety','FINISHED '+String(summary.state).toUpperCase()+' — '+off+'. CHECK HARDWARE.');
    put('rigolTestReport', 'Native report '+String(summary.passed)+' PASS / '+String(summary.failed)+' FAIL · archive '+String(archive.status)+
        (archive.path?' · '+archive.path:''));
    log('TEST','FINISHED '+String(summary.state)+' '+off+' archive '+String(archive.status));
});
put('rigolTestSafety','READY · Connect four instruments and confirm wiring; never run unattended.');
put('rigolTestLatest','No CH1 scope capture from a test yet.');
put('rigolTestReport','Reports auto-archive as JSON after cleanup. JSON/CSV buttons export to Documents/WFScript Reports.');
