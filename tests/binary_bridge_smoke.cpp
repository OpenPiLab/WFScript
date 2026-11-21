// Native Binary Lab smoke. No external instruments or internet access.
#include "BinaryBridge.h"
#include "CommunicationBridge.h"
#include <QCoreApplication>
#include <QJsonArray> // includes satisfy repository conservative native-test audit
#include <QJSEngine>
#include <QJSValue>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>
#include <QDebug>
#include <cmath>

static int failures=0;
static void verify(bool condition,const char* name){
    if(condition)qInfo()<<"PASS"<<name;
    else{qCritical()<<"FAIL"<<name;++failures;}
}
static bool js(QJSEngine& engine,const QString& source){
    QJSValue result=engine.evaluate(source,QStringLiteral("<native-binary-test>"));
    if(result.isError()){
        qCritical()<<"JavaScript error"<<result.toString()<<result.property("stack").toString();return false;
    }
    return true;
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    QJSEngine engine;
    BinaryBridge binary(&engine);
    QJSEngine::setObjectOwnership(&binary,QJSEngine::CppOwnership);
    engine.globalObject().setProperty(QStringLiteral("Binary"),engine.newQObject(&binary));
    verify(js(engine,QStringLiteral(R"JS(
        var b=Binary.wordPattern(100000);
        if(!b || b.length!==200000)throw Error('wordPattern bytes');
        if(b.readU16LE(0)!==1519)throw Error('first word');
        if(b.readU16LE(199998)!==((99999*73+1519)&65535))throw Error('last word');
        var s=b.statsU16LE(0,100000);
        var expectSum=0,expectMin=65535,expectMax=0;
        for(var i=0;i<100000;i++){
            var x=(i*73+1519)&65535;
            expectSum+=x;expectMin=Math.min(expectMin,x);expectMax=Math.max(expectMax,x);
        }
        if(s.count!==100000 || s.sum!==expectSum || s.min!==expectMin || s.max!==expectMax)
            throw Error('statistics');
        var f=Binary.wrapIEEE(b);
        if(!f || f.length!==200009)throw Error('frame length');
        var payload=f.ieeePayload();
        if(!payload || payload.length!==b.length || payload.readU16LE(0)!==1519)
            throw Error('IEEE view');
        var accum=Binary.stream(300000);
        for(var i=0;i<f.length;i+=1001){
            var p=f.slice(i,Math.min(1001,f.length-i));
            if(!accum.append(p))throw Error(accum.lastError);
        }
        var ret=accum.readIEEE();
        if(!ret || ret.length!==200000 || ret.readU16LE(199998)!==b.readU16LE(199998))
            throw Error('stream result');
        if(accum.pending!==0)throw Error('SCPI newline not consumed');
        var fields=Binary.fromHex('123478560000803f');
        if(!fields || fields.readU16BE(0)!==0x1234 || fields.readU16LE(2)!==0x5678)
            throw Error('endian');
        if(Math.abs(fields.readF32LE(4)-1)>1e-9)throw Error('float');
        if(fields.toHex(0,8)!=='123478560000803f')throw Error('hex');
        var arr=fields.toArray(0,8);
        if(!Array.isArray(arr)||arr.length!==8||arr[3]!==0x56)throw Error('bounded array export');
        var bad=Binary.fromHex('GG');if(bad!==null || !Binary.lastError)throw Error('invalid hex');
        if(fields.readU16LE(8)!==-1 || !fields.lastError)throw Error('bounds');
        var p=Binary.pattern(4096), bytes=[];
        for(var i=0;i<4096;i++)bytes.push((i*73+19)&255);
        var sum=0,crc=65535;
        for(var i=0;i<bytes.length;i++){
            sum=(sum+bytes[i])>>>0;crc^=bytes[i];
            for(var k=0;k<8;k++)crc=(crc&1)?((crc>>>1)^0xa001):(crc>>>1);
        }
        if(p.sum32()!==sum || p.crc16()!==crc)throw Error('checksum');
        var line=Binary.stream(32);
        if(!line.append(Binary.fromHex('53544f500d0a')))throw Error('line append');
        if(line.readLine()!=='STOP'||line.readLine()!==null)throw Error('line parse');
        var loose=Binary.stream(16);
        if(!loose.append(Binary.fromHex('2333393939393939393939')))throw Error('malformed append');
        if(loose.readIEEE()!==null||!loose.lastError)throw Error('oversize frame reject');
        var split=Binary.stream(32);
        if(!split.append(Binary.fromHex('23313141')))throw Error('split append'); // #11A
        var letter=split.readIEEE();if(!letter||letter.length!==1||letter.readU8(0)!==65)throw Error('split frame');
        if(!split.append(Binary.fromHex('0d0a4f4b0a')))throw Error('split suffix');
        if(split.readLine()!=='OK')throw Error('split suffix skip');
        b.release();f.release();payload.release();ret.release();p.release();fields.release();
    )JS")),"native buffer and SCPI/packet tests");
    verify(js(engine,QStringLiteral(R"JS(
        var rw=Binary.create(96);
        if(!rw||rw.length!==96||rw.readU32LE(0)!==0)throw Error('zeroed buffer');
        function ok(result,name){if(!result)throw Error(name+': '+rw.lastError);}
        ok(rw.writeU8(0,254),'u8');if(rw.readU8(0)!==254)throw Error('u8 roundtrip');
        ok(rw.writeI8(1,-7),'i8');if(rw.readI8(1)!==-7)throw Error('i8 roundtrip');
        ok(rw.writeU16LE(2,0x1234),'u16le');if(rw.toHex(2,2)!=='3412')throw Error('u16le wire');
        ok(rw.writeU16BE(4,0x5678),'u16be');if(rw.readU16BE(4)!==0x5678)throw Error('u16be');
        ok(rw.writeI16LE(6,-2345),'i16le');if(rw.readI16LE(6)!==-2345)throw Error('i16le');
        ok(rw.writeI16BE(8,-1234),'i16be');if(rw.readI16BE(8)!==-1234)throw Error('i16be');
        ok(rw.writeU32LE(10,0xFEDCBA98),'u32le');if(rw.readU32LE(10)!==0xFEDCBA98)throw Error('u32le');
        ok(rw.writeU32BE(14,0xFEDCBA98),'u32be');if(rw.readU32BE(14)!==0xFEDCBA98)throw Error('u32be');
        ok(rw.writeI32LE(18,-123456789),'i32le');if(rw.readI32LE(18)!==-123456789)throw Error('i32le');
        ok(rw.writeI32BE(22,-123456789),'i32be');if(rw.readI32BE(22)!==-123456789)throw Error('i32be');
        ok(rw.writeF32LE(26,3.125),'f32le');if(rw.readF32LE(26)!==3.125)throw Error('f32le');
        ok(rw.writeF32BE(30,3.125),'f32be');if(rw.readF32BE(30)!==3.125)throw Error('f32be');
        ok(rw.writeF64LE(34,Math.PI),'f64le');if(Math.abs(rw.readF64LE(34)-Math.PI)>1e-15)throw Error('f64le');
        ok(rw.writeF64BE(42,-Math.E),'f64be');if(Math.abs(rw.readF64BE(42)+Math.E)>1e-15)throw Error('f64be');
        ok(rw.writeU64LE(50,'18446744073709551615'),'u64le');
        if(rw.readU64LE(50)!=='18446744073709551615')throw Error('u64le full precision');
        ok(rw.writeI64BE(58,'-9223372036854775808'),'i64be');
        if(rw.readI64BE(58)!=='-9223372036854775808')throw Error('i64be full precision');
        ok(rw.writeUtf8(66,'WFScript'),'utf8');if(rw.toUtf8(66,8)!=='WFScript')throw Error('utf8');
        var b64=rw.toBase64(),again=Binary.fromBase64(b64);
        if(!again||again.toHex(0,96)!==rw.toHex(0,96))throw Error('base64 roundtrip');
        var part=rw.slice(2,4);var orig=rw.readU8(2);
        ok(part.writeU8(0,99),'slice write');
        if(rw.readU8(2)!==orig || part.readU8(0)!==99)throw Error('copy-on-write');
        var combined=Binary.concat([part,Binary.fromHex('0102')]);
        if(!combined||combined.length!==6||combined.readU16BE(4)!==0x0102)throw Error('concat');
        if(rw.writeU16BE(95,1)||!rw.lastError)throw Error('bounds reject');
        if(rw.writeU8(0,-1)||!rw.lastError)throw Error('negative reject');
        if(rw.writeU64LE(0,'18446744073709551616')||!rw.lastError)throw Error('u64 reject overflow');
        if(Binary.fromBase64('!?')!==null || !Binary.lastError)throw Error('strict base64');
        if(Binary.fromUtf8('123456789').crc32()!==0xCBF43926)throw Error('CRC32 standard vector');
        var flag=Binary.fromHex('1234aabb1234');
        if(flag.indexOf(Binary.fromHex('aabb'),0)!==2 || !flag.equals(Binary.fromHex('1234aabb1234')))throw Error('byte search/equality');
        var can=Binary.create(8);
        ok(can.writeBits(0,12,'intel',false,0xABC),'intel write');
        if(can.toHex(0,2)!=='bc0a'||can.readBits(0,12,'intel',false)!==0xABC)throw Error('intel wire');
        can.fill(0,0,8);ok(can.writeBits(7,12,'motorola',false,0xABC),'motorola');
        if(can.toHex(0,2)!=='abc0'||can.readBits(7,12,'motorola',false)!==0xABC)throw Error('motorola wire');
        can.fill(0,0,8);ok(can.writeSignal(0,16,'intel',true,0.1,-40,23.5),'DBC scaled write');
        if(Math.abs(can.readSignal(0,16,'intel',true,0.1,-40)-23.5)>1e-8)throw Error('DBC scaled read');
        ok(can.writeBits(32,12,'intel',true,-100),'DBC signed write');
        if(can.readBits(32,12,'intel',true)!==-100)throw Error('DBC signed read');
        if(can.writeBits(63,12,'motorola',false,100)||!can.lastError)throw Error('DBC reject OOB');
        can.fill(0,0,8);
        if(!can.writeBits64(0,64,'intel',false,'18446744073709551615') ||
           can.readBits64(0,64,'intel',false)!=='18446744073709551615')throw Error('64-bit DBC exact');
        can.fill(0,0,8);
        if(!can.writeBits64(7,64,'motorola',true,'-123456789012345') ||
           can.readBits64(7,64,'motorola',true)!=='-123456789012345')throw Error('64-bit Motorola signed');
        var chunk=Binary.fromHex('04000102aabb'),stream=Binary.stream(512);
        if(!stream.append(chunk.slice(0,1)) || stream.readLengthPrefixed(2,'little',false)!==null)throw Error('partial length prefix');
        if(!stream.append(chunk.slice(1,4)) || stream.readLengthPrefixed(2,'little',false)!==null)throw Error('partial payload');
        if(!stream.append(chunk.slice(5,1)))throw Error('finish prefix');
        var payload=stream.readLengthPrefixed(2,'little',false);
        if(!payload||payload.toHex(0,4)!=='0102aabb')throw Error('prefix payload');
        if(!stream.append(Binary.fromHex('deadbeef01')))throw Error('fixed append');
        var fixed=stream.readFixed(4);
        if(!fixed||fixed.toHex(0,4)!=='deadbeef'||stream.pending!==1)throw Error('fixed remainder');
        var delim=Binary.stream(16);
        delim.append(Binary.fromHex('4142430044'));
        var txt=delim.readUntil(Binary.fromHex('00'),false);
        if(!txt||txt.toUtf8(0,3)!=='ABC'||delim.pending!==1)throw Error('readUntil delimiter');
        var samples=Binary.wordPattern(100000);
        var decoded=samples.decodeSamples('u16le',0,100000,0.001,-2);
        if(!decoded||decoded.length!==100000)throw Error('native samples');
        var stats=decoded.stats(), bins=decoded.minMaxBins(400);
        if(stats.count!==100000 || bins.length!==400)throw Error('sample stats/bin count');
        if(Math.abs(decoded.at(0)-(1519*0.001-2))>1e-8)throw Error('sample first');
        var stats2=samples.statsNumeric('u16le',0,100000,0.001,-2);
        if(Math.abs(stats2.rms-stats.rms)>1e-6)throw Error('statsNumeric compare');
        decoded.release();samples.release();rw.release();part.release();again.release();combined.release();can.release();
    )JS")),"RC1.3 binary read/write, DBC, file-style frames, samples and bounds");
    CommunicationBridge bridge(&engine,&binary);
    QJSEngine::setObjectOwnership(&bridge,QJSEngine::CppOwnership);
    engine.globalObject().setProperty(QStringLiteral("__commHost"),engine.newQObject(&bridge));
    verify(js(engine,CommunicationBridge::bootstrapSource()),"communications wrappers");
    verify(js(engine,QStringLiteral(R"JS(
        var resultOK=false,failures=[],server=null;
        var frame=Binary.wrapIEEE(Binary.wordPattern(100000));
        server=TCP.createServer({host:'127.0.0.1',port:0});
        server.on('error',function(e){failures.push(String(e));});
        server.on('connection',function(peer){
            peer.on('binary',function(chunk){peer.write(chunk);});
        });
        server.on('listening',function(){
            var client=TCP.create({host:'127.0.0.1',port:server.info().localPort});
            var stream=Binary.stream(300000);
            client.on('error',function(e){failures.push(String(e));});
            client.on('binary',function(chunk){
                if(!stream.append(chunk)){failures.push(stream.lastError);return;}
                var data=stream.readIEEE();
                if(data){
                    if(data.length!==200000 || data.readU16LE(0)!==1519 ||
                        data.readU16LE(199998)!==((99999*73+1519)&65535))
                        failures.push('bad TCP native bytes');
                    else resultOK=true;
                    data.release();client.close();server.close();
                }else if(stream.lastError)failures.push(stream.lastError);
            });
            client.on('connect',function(){
                for(var p=0;p<frame.length;p+=50000){
                    client.write(frame.slice(p,Math.min(50000,frame.length-p)));
                }
            });
            client.connect();
        });
        server.listen();
    )JS")),"localhost native TCP scenario launched");
    QElapsedTimer timer;timer.start();
    while(timer.elapsed()<6000 && !engine.globalObject().property(QStringLiteral("resultOK")).toBool() &&
            engine.globalObject().property(QStringLiteral("failures")).property(QStringLiteral("length")).toInt()==0){
        QCoreApplication::processEvents(QEventLoop::AllEvents,25);
        QThread::msleep(1);
    }
    verify(engine.globalObject().property(QStringLiteral("resultOK")).toBool(),"binary TCP 200k bytes localhost framed echo");
    verify(engine.globalObject().property(QStringLiteral("failures")).property(QStringLiteral("length")).toInt()==0,"no native TCP callback errors");
    bridge.shutdown();binary.shutdown();
    return failures?1:0;
}
