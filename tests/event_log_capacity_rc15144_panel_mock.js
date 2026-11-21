// Hardware-free execution of the embedded Panel JavaScript with controlled timer callbacks.
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const path=require('path');
const panel=JSON.parse(fs.readFileSync(path.join(__dirname,'../development/eventlog_rc15144/WFScript_EventLog_50k_Rollover_Lab.wfpanel'),'utf8'));
const timers=new Map(); let next=1;
const entries=[]; let status='';
const sandbox={
    Date,
    ui:{set(id,payload){
        if(id==='stationLog'){
            if(payload.clear===true){entries.length=0;return;}
            assert(payload.append && ['INFO','PASS'].includes(payload.append.level));
            entries.push(payload.append);
        } else if(id==='timerStatus')status=payload;
        else throw new Error('Unknown ID '+id);
    }},
    setInterval(fn,interval){const id=next++;timers.set(id,{fn,interval});return id;},
    clearInterval(id){timers.delete(id);}
};
vm.createContext(sandbox);
vm.runInContext(panel.script,sandbox,{filename:panel.scriptName});
sandbox.onButton1Clicked();assert.strictEqual(timers.size,4);
sandbox.onButton1Clicked();assert.strictEqual(timers.size,4,'double-start must not duplicate intervals');
for(const t of Array.from(timers.values())) if([10,20,30].includes(t.interval))for(let k=0;k<3;k++)t.fn();
assert.strictEqual(entries.length,18);
for(let i=0;i<entries.length;i+=2){
 assert.strictEqual(entries[i].level,'INFO');
 assert.strictEqual(entries[i+1].level,'PASS');
 assert(entries[i].message.includes('callbackEpochMs='));
 assert(entries[i+1].message.includes('seq='));
}
sandbox.onButton2Clicked();assert.strictEqual(timers.size,0);
assert(status.startsWith('STOPPED'));
sandbox.onButton3Clicked();assert.strictEqual(entries.length,0);
sandbox.onButton1Clicked();assert.strictEqual(timers.size,4);
sandbox.onShutdown();assert.strictEqual(timers.size,0);
console.log('PASS Event Log RC1.5.14.4 Panel timer callbacks, paired sequences, start/stop/clear/shutdown');
