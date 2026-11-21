// WFScript v0.2-dev Binary Lab 1 — reproducible native-vs-JavaScript benchmarks.
// This Panel never connects to an external instrument, enables outputs, or reads
// arbitrary files. The optional TCP test only uses 127.0.0.1 and an ephemeral port.
// All test data is deterministic. Native and JS results must agree to report PASS.
// Timing is wall-clock in the worker thread via Binary.clockUs() (monotonic Qt timer).
// It is NOT a measurement of the MHO954's network throughput.
var lastScpi = 'Not run', lastPacket = 'Not run', lastTcp = 'Not run';
var tcpBusy = false;
var metricRows = [], activeSuite = 'SCPI', activePoints = 0, activeBytes = 0, runSerial=0, activeRun=0;
function logLine(s) { print('[BINARY LAB] ' + s); }
function status(s) { ui.set('benchStatus',s); ui.set('benchStatus2',s); logLine(s); }
function fail(s,e) { status(s+' ERROR: '+(e&&e.message?e.message:e)); }
function check(ok,label) { if(!ok) throw Error('CORRECTNESS FAILURE: '+label); }
function bench(label,fn) {
    var start=Binary.clockUs(), value=fn(), us=Binary.clockUs()-start;
    return {value:value,ms:us/1000};
}
function fmt(n) {return Number(n).toFixed(3);}
function row(name,nativeMs,jsMs,verdict) {
    metricRows.push({run:activeRun,suite:activeSuite,task:name,points:activePoints,bytes:activeBytes,nativeMs:nativeMs,jsMs:jsMs,verdict:verdict});
    var speed= nativeMs>0 ? fmt(jsMs/nativeMs)+'x' : 'n/a';
    return name+' | Native '+fmt(nativeMs)+' ms | JS '+fmt(jsMs)+' ms | '+speed+' | '+verdict;
}
function selectedPoints() {
    var value=String(ui.get('pointSize'));
    return value==='1,000,000' ? 1000000 : 100000;
}
function selectedRepeat() {
    var value=Number(ui.get('repetitions'));
    if(!isFinite(value) || value<1 || value>5) return 1;
    return Math.floor(value);
}
function jsWordBytes(count) {
    var b=new Array(count*2);
    for(var i=0;i<count;i++){
        var value=((i*73+1519)&65535);
        b[2*i]=value&255;b[2*i+1]=value>>>8;
    }
    return b;
}
function jsStatsU16LE(b,count) {
    var lo=65535,hi=0,sum=0,sum2=0;
    for(var i=0;i<count;i++){
        var v=b[2*i]+256*b[2*i+1];
        if(v<lo)lo=v;if(v>hi)hi=v;
        sum+=v;sum2+=v*v;
    }
    return {count:count,min:lo,max:hi,sum:sum,mean:sum/count,rms:Math.sqrt(sum2/count)};
}
function jsFrameOf(b) {
    var digits=String(b.length), prefix='#'+String(digits.length)+digits;
    var result=new Array(prefix.length+b.length+1);
    for(var j=0;j<prefix.length;j++)result[j]=prefix.charCodeAt(j);
    for(var i=0;i<b.length;i++)result[prefix.length+i]=b[i];
    result[result.length-1]=10;
    return result;
}
function jsPayloadOf(frame) {
    check(frame[0]===35,'JS frame marker');
    var digits=frame[1]-48, len=0;
    check(digits>0&&digits<=9,'JS frame digit count');
    for(var i=2;i<2+digits;i++)len=len*10+(frame[i]-48);
    check(frame.length===2+digits+len+1,'JS full frame length');
    return frame.slice(2+digits,2+digits+len);
}
function closeEnough(a,b){return Math.abs(a-b)<0.000001*Math.max(1,Math.abs(a),Math.abs(b));}
function runScpi() {
    try {
        activeSuite='SCPI'; activeRun=++runSerial;activePoints=selectedPoints(); activeBytes=activePoints*2;
        var points=selectedPoints(), lines=[], generationNative=bench('native generation',function(){return Binary.wordPattern(points);});
        var generationJS=bench('JS generation',function(){return jsWordBytes(points);});
        var b=generationNative.value, a=generationJS.value;
        check(b && b.length===2*points,'native WORD allocation');
        check(b.readU16LE(0)===a[0]+256*a[1],'first WORD');
        check(b.readU16LE(2*(points-1))===a[2*points-2]+256*a[2*points-1],'last WORD');
        lines.push(row('Build '+points+' WORD points',generationNative.ms,generationJS.ms,'PASS'));
        var nativeFrame=bench('native IEEE frame',function(){return Binary.wrapIEEE(b);});
        var jsFrame=bench('JS IEEE frame',function(){return jsFrameOf(a);});
        check(nativeFrame.value && nativeFrame.value.length===jsFrame.value.length,'IEEE framed lengths');
        lines.push(row('Pack IEEE488.2 frame',nativeFrame.ms,jsFrame.ms,'PASS'));
        var nativeExtract=bench('native header parse',function(){return nativeFrame.value.ieeePayload();});
        var jsExtract=bench('JS header parse+slice',function(){return jsPayloadOf(jsFrame.value);});
        check(nativeExtract.value && nativeExtract.value.length===jsExtract.value.length,'IEEE payload extraction');
        check(nativeExtract.value.readU16LE(0)===a[0]+a[1]*256,'IEEE payload data');
        lines.push(row('Parse IEEE + extract payload',nativeExtract.ms,jsExtract.ms,'PASS'));
        var nativeStats=bench('native WORD stats',function(){return b.statsU16LE(0,points);});
        var jsStats=bench('JS WORD stats',function(){return jsStatsU16LE(a,points);});
        var ns=nativeStats.value,js=jsStats.value;
        check(ns.count===js.count&&ns.min===js.min&&ns.max===js.max,'WORD stats count/min/max');
        check(closeEnough(ns.sum,js.sum)&&closeEnough(ns.rms,js.rms),'WORD sum and RMS');
        lines.push(row('Decode + min/max/mean/RMS',nativeStats.ms,jsStats.ms,'PASS'));
        // Simulate SCPI recv events: byte-by-byte JS accumulation vs native buffer append.
        // Frame building above is excluded from this measured phase.
        var nativeStream=Binary.stream(Math.max(300000,nativeFrame.value.length+256));
        var nativeAssembly=bench('native stream assembly',function(){
            for(var off=0;off<nativeFrame.value.length;off+=16384){
                var part=nativeFrame.value.slice(off,Math.min(16384,nativeFrame.value.length-off));
                check(nativeStream.append(part),'native stream append');
            }
            return nativeStream.readIEEE();
        });
        var jsAssembly=bench('JS stream assembly',function(){
            var rx=[];
            for(var off=0;off<jsFrame.value.length;off+=16384){
                var end=Math.min(jsFrame.value.length,off+16384);
                for(var k=off;k<end;k++)rx.push(jsFrame.value[k]);
            }
            return jsPayloadOf(rx);
        });
        check(nativeAssembly.value && nativeAssembly.value.length===jsAssembly.value.length,'assembled payload');
        lines.push(row('16 KiB chunks → complete SCPI frame',nativeAssembly.ms,jsAssembly.ms,'PASS'));
        var result='SCPI WORD / '+points+' points / '+(2*points)+' bytes\n'+lines.join('\n');
        result+='\nResult: '+ns.count+' samples, min='+ns.min+' max='+ns.max+' RMS codes='+fmt(ns.rms);
        result+='\nNative payload storage: '+(2*points)+' bytes (plus wrappers); JS payload array: '+(2*points)+' numeric elements.';
        result+='\nMethods compare equivalent results, not identical internal storage. No external scope used.';
        ui.set('scpiResults',result); lastScpi=result;
        status('SCPI synthetic test PASS — '+points+' WORD samples; 5 comparisons');
        logLine(result.split('\n').join(' | '));
        // Explicitly release large native views; JavaScript arrays are local.
        nativeAssembly.value.release();nativeExtract.value.release();nativeFrame.value.release();b.release();nativeStream.clear();
    } catch(e) {fail('SCPI benchmark',e);ui.set('scpiResults','FAILED: '+e.message);}
}
function jsPattern(count){var b=new Array(count);for(var i=0;i<count;i++)b[i]=(i*73+19)&255;return b;}
function jsSum32(bytes){var x=0;for(var i=0;i<bytes.length;i++)x=(x+bytes[i])>>>0;return x;}
function jsCRC(bytes){
    var crc=65535;
    for(var i=0;i<bytes.length;i++){
        crc^=bytes[i];for(var bit=0;bit<8;bit++)crc=(crc&1)?((crc>>>1)^0xA001):(crc>>>1);
    }
    return crc;
}
function runProtocols() {
    try {
        activeSuite='PACKET';activeRun=++runSerial;activePoints=0;activeBytes=1048576;
        var lines=[], n=1048576, generationN=bench('native pattern',function(){return Binary.pattern(n);});
        var generationJ=bench('JS pattern',function(){return jsPattern(n);});
        var data=generationN.value, js=generationJ.value;
        check(data&&data.length===n&&data.readU8(0)===js[0]&&data.readU8(n-1)===js[n-1],'file-like bytes');
        lines.push(row('Build synthetic 1 MiB file payload',generationN.ms,generationJ.ms,'PASS'));
        var nativeSum=bench('native checksum',function(){return data.sum32();});
        var jsSum=bench('JS checksum',function(){return jsSum32(js);});
        check(nativeSum.value===jsSum.value,'file-like sum32');
        lines.push(row('File-style 1 MiB byte checksum',nativeSum.ms,jsSum.ms,'PASS'));
        activeBytes=4096;
        var packetN=Binary.pattern(4096), packetJS=jsPattern(4096);
        var nativeCrc=bench('native CRC',function(){return packetN.crc16();});
        var jsCrc=bench('JS CRC',function(){return jsCRC(packetJS);});
        check(nativeCrc.value===jsCrc.value,'CRC16/Modbus');
        lines.push(row('Protocol CRC16 / 4096 bytes',nativeCrc.ms,jsCrc.ms,'PASS'));
        activeBytes=8;
        var fields=Binary.fromHex('12 34 78 56 00 00 80 3f');
        check(fields && fields.readU16BE(0)===0x1234,'U16 BE');
        check(fields.readU16LE(2)===0x5678,'U16 LE');
        check(closeEnough(fields.readF32LE(4),1),'float32 LE');
        check(fields.toHex(0,8)==='123478560000803f','hex');
        var nativeFields=bench('native fields',function(){
            var sum=0;for(var i=0;i<10000;i++)sum+=fields.readU16LE(2)+fields.readF32LE(4);
            return sum;
        });
        var jsFields=bench('JS fields',function(){
            var sum=0;var f=[18,52,120,86,0,0,128,63];
            for(var i=0;i<10000;i++)sum+=(f[2]+256*f[3])+1;
            return sum;
        });
        check(nativeFields.value===jsFields.value,'small field access');
        lines.push(row('10k small field reads',nativeFields.ms,jsFields.ms,'PASS'));
        var view=data.slice(100,256);
        check(view&&view.length===256&&view.readU8(0)===js[100],'sliced view');
        view.release();
        var out='FILE / PACKET / FIELDS (synthetic, no disk I/O)\n'+lines.join('\n');
        out+='\nCRC16='+nativeCrc.value+'  sum32='+nativeSum.value+'  endian and F32 checks PASS';
        out+='\nSmall scalar reads may be faster in JavaScript: native method-call overhead is real.';
        ui.set('packetResults',out);lastPacket=out;status('Protocol and file-style tests PASS — 4 comparisons');
        logLine(out.split('\n').join(' | '));
        fields.release();packetN.release();data.release();
    } catch(e){fail('Protocol benchmark',e);ui.set('packetResults','FAILED: '+e.message);}
}
function clearResults() {
    ui.set('scpiResults','Not run');ui.set('packetResults','Not run');ui.set('tcpResults','Not run');
    metricRows=[];runSerial=0;
    status('Cleared. All tests are synthetic or localhost-only; no external instruments.');
}
function saveMetrics(){
    try {
        check(metricRows.length>0,'Run at least one test before exporting');
        var csv='run,suite,task,points,bytes,native_ms,javascript_ms,ratio_js_over_native,verdict\n';
        for(var i=0;i<metricRows.length;i++){
            var r=metricRows[i];
            var columns=[r.run,r.suite,r.task,r.points,r.bytes,fmt(r.nativeMs),fmt(r.jsMs),
                         r.nativeMs>0?fmt(r.jsMs/r.nativeMs):'',r.verdict];
            csv+=columns.map(function(v){return '"'+String(v).replace(/"/g,'""')+'"';}).join(',')+'\n';
        }
        var dest=FileIO.dataPath('binary_lab_metrics.csv');
        FileIO.writeText(dest,csv);
        status('Exported '+metricRows.length+' benchmark rows to '+dest);
    }catch(e){fail('CSV export',e);}
}
// Network benchmark exercises the *real* QTcpSocket and Qt→QJSEngine boundary.
// Both cases echo identical 200,009-byte SCPI-style frames over localhost.
// Each elapsed time INCLUDES sending, receiving, and frame assembly; differences
// are not a pure receive-conversion microbenchmark.
function runTcpLoopback(){
    if(tcpBusy)return status('Local TCP benchmark already running');
    tcpBusy=true;activeRun=++runSerial;
    var server=null,client=null,peers=[],results=[],frameN=null;
    var finished=false,bytes=200000;
    // The server-side QTcpSocket can report RemoteHostClosedError after the
    // client intentionally closes a completed test. That is not a failed
    // transfer. Keep peer state so premature closes are still failures.
    function cleanup(){
        var c=client;client=null;
        if(c){try{c.close();}catch(_){}}
        for(var p=0;p<peers.length;p++){
            try{if(peers[p].info().valid)peers[p].close();}catch(_){}}
        peers=[];
        if(server){try{server.close();}catch(_){} server=null;}
        if(frameN){try{frameN.release();}catch(_){} frameN=null;}
        tcpBusy=false;
    }
    function abort(err){
        if(finished)return;
        finished=true;
        var message=String(err&&err.message?err.message:err);
        cleanup();
        fail('Local TCP test',message);
        ui.set('tcpResults','FAILED: '+message);
    }
    try{
        var payload=Binary.wordPattern(100000);frameN=Binary.wrapIEEE(payload);payload.release();
        check(frameN && frameN.length===200009,'framed TCP payload length');
        var jsPayload=jsWordBytes(100000),jsFrame=jsFrameOf(jsPayload);
        check(jsFrame.length===frameN.length,'loopback equal data');
        server=TCP.createServer({host:'127.0.0.1',port:0});
        server.on('error',function(e){abort('Listener: '+e);});
        server.on('connection',function(peer){
            if(finished){try{peer.close();}catch(_){}return;}
            peers.push(peer);
            var echoed=0,expected=jsFrame.length;
            peer.on('binary',function(chunk){
                if(finished)return;
                try{
                    echoed+=chunk.length;
                    if(echoed>expected)return abort('Server received excess data: '+echoed+'/'+expected);
                    var sent=peer.write(chunk);
                    if(sent!==chunk.length)return abort('Server echo write incomplete: '+sent+'/'+chunk.length);
                }catch(e){abort('Server echo: '+e);}
            });
            peer.on('error',function(e){
                if(finished)return;
                var message=String(e&&e.message?e.message:e);
                // QTcpSocket reports this normal disconnect as an error on
                // Windows. Accept it ONLY after the entire input was echoed.
                if(echoed===expected && /remote host closed the connection/i.test(message))return;
                abort('Server peer: '+message+' (echoed '+echoed+'/'+expected+' bytes)');
            });
            peer.on('close',function(){
                if(finished)return;
                if(echoed!==expected)abort('Server peer closed before complete echo: '+echoed+'/'+expected);
            });
        });
        function trial(mode){
            if(finished)return;
            var received=0,rx=[],stream=Binary.stream(300000),start=0,complete=false;
            var trialClient=TCP.create({host:'127.0.0.1',port:server.info().localPort});
            client=trialClient;
            trialClient.on('error',function(e){
                if(!finished && !complete)abort(mode+' client: '+e);
            });
            trialClient.on('close',function(){
                if(!finished && !complete)abort(mode+' client closed before complete response ('+received+'/'+jsFrame.length+')');
            });
            function finish(){
                if(complete || finished)return;
                complete=true;
                var elapsed=(Binary.clockUs()-start)/1000,info=trialClient.info();
                results.push(mode+': '+fmt(elapsed)+' ms  bytes='+received+
                    '  callbacks='+(mode==='legacy'?info.legacyEvents:info.nativeEvents));
                // Close may notify the *server* peer of RemoteHostClosedError.
                // The peer's echoed-byte counter distinguishes expected close
                // from lost data. Complete is set before closing the client.
                trialClient.close();
                if(client===trialClient)client=null;
                stream.clear();
                if(mode==='legacy')trial('native');
                else {
                    var summary='LOCALHOST TCP / 100k WORD / 200,009 bytes incl. IEEE frame\n'+results.join('\n');
                    summary+='\nBoth paths verified full payload. Includes TX, RX and frame processing.';
                    summary+='\nLegacy TCP.on(data) remains unchanged; native uses TCP.on(binary).';
                    var a=parseFloat(results[0].split(' ')[1]),b=parseFloat(results[1].split(' ')[1]);
                    metricRows.push({run:activeRun,suite:'TCP',task:'localhost 200009 byte round-trip',points:100000,bytes:200009,nativeMs:b,jsMs:a,verdict:'PASS'});
                    finished=true;
                    cleanup();
                    ui.set('tcpResults',summary);lastTcp=summary;
                    status('Local TCP loopback PASS — both receive paths');
                    logLine(summary.split('\n').join(' | '));
                }
            }
            if(mode==='legacy')trialClient.on('data',function(chunk){
                if(complete||finished)return;
                try{
                    for(var i=0;i<chunk.length;i++)rx.push(chunk[i]);
                    received=rx.length;
                    if(received===jsFrame.length){
                        var result=jsPayloadOf(rx);
                        check(result.length===bytes && result[0]===jsPayload[0] &&
                            result[result.length-1]===jsPayload[jsPayload.length-1],'legacy loopback data');
                        finish();
                    }else if(received>jsFrame.length)abort('Legacy frame overflow');
                }catch(e){abort('Legacy verification: '+e);}
            });
            else trialClient.on('binary',function(chunk){
                if(complete||finished)return;
                try{
                    received+=chunk.length;
                    if(received>jsFrame.length)return abort('Native frame overflow');
                    if(!stream.append(chunk))return abort(stream.lastError);
                    var decoded=stream.readIEEE();
                    if(decoded){
                        check(decoded.length===bytes && decoded.readU16LE(0)===1519,'native loopback data');
                        check(decoded.readU16LE(199998)===((99999*73+1519)&65535),'last WORD');
                        decoded.release();
                        finish();
                    }else if(stream.lastError)abort(stream.lastError);
                }catch(e){abort('Native verification: '+e);}
            });
            trialClient.on('connect',function(){
                if(finished||complete)return;
                try{
                    start=Binary.clockUs();
                    for(var offset=0;offset<jsFrame.length;offset+=50000){
                        var n=Math.min(50000,jsFrame.length-offset);
                        var written=mode==='legacy'
                            ? trialClient.write(jsFrame.slice(offset,offset+n))
                            : trialClient.write(frameN.slice(offset,n));
                        check(written===n,mode+' TCP write length');
                    }
                }catch(e){abort(mode+' request: '+e);}
            });
            trialClient.connect();
        }
        server.on('listening',function(){if(!finished)trial('legacy');});
        server.listen();
        status('Local TCP loopback running — 127.0.0.1 only; two sequential paths');
    }catch(e){abort(e);}
}

// RC1.3 API exercises: native read+write, FileIO round-trip, framed packets,
// DBC style Intel/Motorola signals, bulk numeric samples and plot-ready bins.
function runCodec(){
    try {
        var lines=[],b=Binary.create(96);
        check(b&&b.length===96,'create 96 bytes');
        check(b.writeU8(0,254)&&b.readU8(0)===254,'U8');
        check(b.writeI8(1,-12)&&b.readI8(1)===-12,'I8');
        check(b.writeU16LE(2,0x1234)&&b.toHex(2,2)==='3412','U16LE wire');
        check(b.writeU16BE(4,0x5678)&&b.readU16BE(4)===0x5678,'U16BE');
        check(b.writeI16LE(6,-1234)&&b.readI16LE(6)===-1234,'I16LE');
        check(b.writeI16BE(8,-2345)&&b.readI16BE(8)===-2345,'I16BE');
        check(b.writeU32LE(10,0xFEDCBA98)&&b.readU32LE(10)===0xFEDCBA98,'U32LE');
        check(b.writeU32BE(14,0x87654321)&&b.readU32BE(14)===0x87654321,'U32BE');
        check(b.writeI32LE(18,-12345678)&&b.readI32LE(18)===-12345678,'I32LE');
        check(b.writeI32BE(22,-2147483647)&&b.readI32BE(22)===-2147483647,'I32BE');
        check(b.writeF32LE(26,3.125)&&b.readF32LE(26)===3.125,'F32LE');
        check(b.writeF32BE(30,-1.5)&&b.readF32BE(30)===-1.5,'F32BE');
        check(b.writeF64LE(34,Math.PI)&&Math.abs(b.readF64LE(34)-Math.PI)<1e-15,'F64LE');
        check(b.writeF64BE(42,-Math.E)&&Math.abs(b.readF64BE(42)+Math.E)<1e-15,'F64BE');
        check(b.writeU64LE(50,'18446744073709551615')&&
              b.readU64LE(50)==='18446744073709551615','full 64-bit unsigned');
        check(b.writeI64BE(58,'-9223372036854775808')&&
              b.readI64BE(58)==='-9223372036854775808','full 64-bit signed');
        check(b.writeUtf8(66,'WFScript')&&b.toUtf8(66,8)==='WFScript','UTF8');
        var b64=b.toBase64(), back=Binary.fromBase64(b64);
        check(back&&back.toHex(0,96)===b.toHex(0,96),'Base64 exact round trip');
        var view=b.slice(2,4), before=b.readU8(2);
        check(view.writeU8(0,77),'slice modification');
        check(b.readU8(2)===before&&view.readU8(0)===77,'copy-on-write view');
        var merged=Binary.concat([Binary.fromHex('AA55'),view]);
        check(merged&&merged.length===6&&merged.toHex(0,2)==='aa55','concat');
        check(b.writeU16BE(95,7)===false&&b.lastError.length>0,'write bounds rejection');
        check(b.equals(back),'native equality');
        check(b.indexOf(Binary.fromHex('3412'),0)===2,'native search');
        var crcVector=Binary.fromUtf8('123456789');
        check(crcVector.crc32()===0xCBF43926,'CRC32 known reference vector');
        lines.push('PASS: integer U8/I8/U16/I16/U32/I32 (LE + BE)');
        lines.push('PASS: IEEE F32/F64 (LE + BE), exact U64/I64 decimal strings');
        lines.push('PASS: UTF-8, Base64, concat, bounds, shared views, CRC32, search');
        lines.push('CRC16=' + b.crc16() + '  sum32=' + b.sum32());
        ui.set('codecResults',lines.join('\n'));status('RC1.3 codec round trips PASS');
        crcVector.release();view.release();back.release();merged.release();b.release();
    }catch(e){fail('RC1.3 codec',e);ui.set('codecResults','FAIL: '+e.message);}
}
function runFileBuffer(){
    try{
        var path=FileIO.dataPath('binary_rc13_roundtrip.bin');
        check(!!path,'valid PanelData path');
        var a=Binary.pattern(1024), tail=Binary.fromHex('0123456789abcdef');
        check(FileIO.writeBuffer(path,a),'native binary file write');
        check(FileIO.appendBuffer(path,tail),'native binary file append');
        var read=FileIO.readBuffer(path);
        check(read&&read.length===1032,'native binary file length');
        check(read.sum32Range(0,1024)===a.sum32(),'disk read checksum');
        check(read.toHex(1024,8)==='0123456789abcdef','disk append bytes');
        ui.set('fileBufferResults','PASS: actual disk FileIO native read/write/append\n'+
              'Location: PanelData/binary_rc13_roundtrip.bin\n'+
              'Bytes: '+read.length+'; CRC16='+read.crc16()+'; round-trip equality PASS\n'+
              'Permission check unchanged; output is intentionally kept for inspection.');
        status('RC1.3 native FileIO disk round-trip PASS');
        read.release();tail.release();a.release();
    }catch(e){fail('RC1.3 FileIO',e);ui.set('fileBufferResults','FAIL: '+e.message);}
}
function runFrames(){
    try{
        var rx=Binary.stream(1024), text=Binary.fromUtf8('STOP\r\n'), payload=Binary.fromHex('0102aabb');
        check(rx.append(text),'append text');check(rx.readLine()==='STOP','text line');
        var ieee=Binary.wrapIEEE(payload);
        var one=ieee.slice(0,3), two=ieee.slice(3,ieee.length-3);
        check(rx.append(one)&&rx.readIEEE()===null,'partial IEEE header');
        check(rx.append(two),'IEEE remainder');
        var decoded=rx.readIEEE();
        check(decoded&&decoded.toHex(0,4)==='0102aabb','complete IEEE');
        // 0x0004 little-endian length prefix + four payload bytes.
        var lp=Binary.fromHex('04000102aabb');
        check(rx.append(lp.slice(0,1))&&rx.readLengthPrefixed(2,'little',false)===null,'partial length prefix');
        check(rx.append(lp.slice(1,4))&&rx.readLengthPrefixed(2,'little',false)===null,'partial payload');
        check(rx.append(lp.slice(5,1)),'last byte');
        var body=rx.readLengthPrefixed(2,'little',false);
        check(body&&body.toHex(0,4)==='0102aabb','length-prefixed payload');
        check(rx.append(Binary.fromHex('1020304050')),'fixed frame append');
        var fixed=rx.readFixed(3);
        check(fixed&&fixed.toHex(0,3)==='102030'&&rx.pending===2,'fixed frame preserves excess');
        var delim=Binary.stream(16);
        check(delim.append(Binary.fromHex('4142430044')),'delimited append');
        var stop=delim.readUntil(Binary.fromHex('00'),false);
        check(stop && stop.toUtf8(0,3)==='ABC' && delim.pending===1,'native delimiter frame');
        stop.release();delim.clear();
        ui.set('frameResults','PASS: mixed text SCPI line + IEEE #definite block\n'+
          'PASS: fragmented 16-bit length prefix and payload\n'+
          'PASS: fixed/delimited frames, partial headers and byte handling');
        status('RC1.3 framing round trips PASS');
        [one,two,decoded,body,fixed,ieee,payload,text,lp].forEach(function(b){b.release();});rx.clear();
    }catch(e){fail('RC1.3 framing',e);ui.set('frameResults','FAIL: '+e.message);}
}
function jsWriteIntel(bytes,start,width,value){
    for(var k=0;k<width;k++){
        var bit=start+k,oct=bit>>3, mask=1<<(bit&7);
        bytes[oct]=(bytes[oct]&~mask)|(((value>>k)&1)?mask:0);
    }
}
function jsReadIntel(bytes,start,width){
    var value=0;
    for(var k=0;k<width;k++)value+=((bytes[(start+k)>>3]>>((start+k)&7))&1)*Math.pow(2,k);
    return value;
}
function runCan(){
    try {
        var f=Binary.create(8);
        check(f.writeBits(0,12,'intel',false,0xABC),'Intel write');
        check(f.toHex(0,2)==='bc0a'&&f.readBits(0,12,'intel',false)===0xABC,'Intel expected wire');
        f.fill(0,0,8);
        check(f.writeBits(7,12,'motorola',false,0xABC),'Motorola write');
        check(f.toHex(0,2)==='abc0'&&f.readBits(7,12,'motorola',false)===0xABC,'Motorola sawtooth wire');
        f.fill(0,0,8);
        check(f.writeSignal(0,16,'intel',true,0.1,-40,23.5),'scaled signed write');
        check(Math.abs(f.readSignal(0,16,'intel',true,0.1,-40)-23.5)<1e-8,'scaled signed read');
        check(f.writeBits(32,12,'intel',true,-100),'signed CAN write');
        check(f.readBits(32,12,'intel',true)===-100,'signed CAN read');
        f.fill(0,0,8);
        check(f.writeBits64(0,64,'intel',false,'18446744073709551615'),'full uint64 DBC');
        check(f.readBits64(0,64,'intel',false)==='18446744073709551615','exact uint64 DBC');
        f.fill(0,0,8);
        check(f.writeBits64(7,64,'motorola',true,'-123456789012345'),'signed motorola 64bit');
        check(f.readBits64(7,64,'motorola',true)==='-123456789012345','exact Motorola 64bit');
        var native=bench('native DBC 1000 writes',function(){
            var b=Binary.create(8);for(var i=0;i<1000;i++)b.writeBits(0,12,'intel',false,i&4095);return b;
        });
        var pure=bench('JS DBC 1000 writes',function(){
            var b=[0,0,0,0,0,0,0,0];for(var i=0;i<1000;i++)jsWriteIntel(b,0,12,i&4095);return b;
        });
        check(native.value.readBits(0,12,'intel',false)===jsReadIntel(pure.value,0,12),'DBC benchmark equal');
        var result='PASS: CAN Intel 12-bit bytes bc 0a; Motorola 12-bit bytes ab c0\n'+
          'PASS: signed/scaled DBC fields and lossless 64-bit Intel/Motorola\n'+
          '1000 scalar write calls Native '+fmt(native.ms)+' ms / JS '+fmt(pure.ms)+
          ' ms (crossing cost is expected; do not use scalar calls for DSP loops)\n'+
          'These are DBC bit-field primitives, NOT a DBC-file parser or CAN bus driver.';
        ui.set('canResults',result);status('RC1.3 CAN/DBC bit-field round trips PASS');
        native.value.release();f.release();
    }catch(e){fail('RC1.3 CAN/DBC',e);ui.set('canResults','FAIL: '+e.message);}
}
function runSampleSeries(){
    try{
        var count=100000, raw=Binary.wordPattern(count);
        var native=bench('native samples + bins',function(){
            var x=raw.decodeSamples('u16le',0,count,0.001,-2.0);
            var stats=x.stats(),bins=x.minMaxBins(400);
            return {series:x,stats:stats,bins:bins};
        });
        var js=bench('JS samples + bins',function(){
            var numbers=new Array(count),lo=Infinity,hi=-Infinity,sum=0,sum2=0;
            for(var i=0;i<count;i++){
                var v=((i*73+1519)&65535)*0.001-2;
                numbers[i]=v;if(v<lo)lo=v;if(v>hi)hi=v;sum+=v;sum2+=v*v;
            }
            var bins=[];
            for(var b=0;b<400;b++){
                var start=Math.floor(b*count/400),end=Math.floor((b+1)*count/400),mn=numbers[start],mx=mn;
                for(var k=start+1;k<end;k++){if(numbers[k]<mn)mn=numbers[k];if(numbers[k]>mx)mx=numbers[k];}
                bins.push({start:start,end:end,min:mn,max:mx});
            }
            return {stats:{count:count,min:lo,max:hi,sum:sum,rms:Math.sqrt(sum2/count)},bins:bins};
        });
        check(native.value.stats.count===js.value.stats.count,'sample count');
        check(closeEnough(native.value.stats.min,js.value.stats.min)&&
            closeEnough(native.value.stats.max,js.value.stats.max)&&
            closeEnough(native.value.stats.rms,js.value.stats.rms),'numeric stats');
        check(native.value.bins.length===400,'native plot-ready bins');
        for(var i=0;i<400;i++)check(closeEnough(native.value.bins[i].min,js.value.bins[i].min)&&
            closeEnough(native.value.bins[i].max,js.value.bins[i].max),'bin '+i);
        ui.set('sampleResults','PASS: 100,000 scaled u16 samples, stats + 400 min/max plot bins\n'+
            'Native '+fmt(native.ms)+' ms   /   JavaScript '+fmt(js.ms)+' ms\n'+
            'Native series length '+native.value.series.length+', min '+fmt(native.value.stats.min)+
            ', max '+fmt(native.value.stats.max)+', RMS '+fmt(native.value.stats.rms)+'\n'+
            'Plot-ready 400 bins: small JS result; direct native plot binding is future work.');
        status('RC1.3 native sample/bins comparison PASS');
        native.value.series.release();raw.release();
    }catch(e){fail('RC1.3 sample series',e);ui.set('sampleResults','FAIL: '+e.message);}
}

function onInitialize(){
    status('READY — run synthetic benchmarks first. Binary is native; JS is baseline. No external hardware.');
    ui.set('scpiResults',lastScpi);ui.set('packetResults',lastPacket);ui.set('tcpResults',lastTcp);
    ui.set('codecResults','Not run');ui.set('fileBufferResults','Not run');ui.set('frameResults','Not run');
    ui.set('canResults','Not run');ui.set('sampleResults','Not run');
}
function onShutdown(){tcpBusy=false;}
