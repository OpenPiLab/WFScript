#include "CompletionTypeResolver.h"
#include "CompletionCatalog.h"
#include <QCoreApplication>
#include <QDebug>
#include <QString>

static int failures=0;
static void expect(const QString& label,const QString& code,const QString& typed,const QString& expected) {
    const QString got=CompletionTypeResolver::resolvePath(code,typed);
    if(got!=expected) { qCritical().noquote()<<"FAIL"<<label<<"got:"<<got<<"expected:"<<expected; ++failures; }
    else qInfo().noquote()<<"PASS"<<label;
}
static void member(const QString& label,const QString& code,const QString& typed,const QString& wanted) {
    const QString resolved=CompletionTypeResolver::resolvePath(code,typed);
    bool found=false;
    for (const auto& entry : CompletionCatalog::entries(resolved))
        if(entry.name==wanted) { found=true;break; }
    if(!found) { qCritical().noquote()<<"FAIL member"<<label<<"resolved:"<<resolved<<"wanted:"<<wanted; ++failures; }
    else qInfo().noquote()<<"PASS member"<<label;
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    const QString setup=QString::fromUtf8(R"JS(
function setup56(f1,amp1,f2,amp2){
 Scope.stop();Wavegen.stop();
 for(var i=1;i<=2;i++){
  var ch=i===1?Wavegen.Channel1:Wavegen.Channel2;
  ch.enable=true;ch.Mode.text="Simple";
  ch.Simple.Type.text="Sine";
  ch.Simple.Frequency.value=i===1?f1:f2;
  ch.Simple.Amplitude.value=i===1?amp1:amp2;
  ch.Modulation.FM.enable=false;ch.Modulation.AM.enable=false;
  ch.Sweep.Frequency.enable=false;ch.Sweep.Amplitude.enable=false;
  ch.Trigger.Source.text="None";ch.Trigger.Idle.text="Offset";
  
)JS");
    expect("original ternary variable",setup,QStringLiteral("ch"),QStringLiteral("Wavegen1.Channel1"));
    member("ch.",setup,QStringLiteral("ch"),QStringLiteral("Simple"));
    member("ch.Simple.",setup,QStringLiteral("ch.Simple"),QStringLiteral("Frequency"));
    member("ch.Modulation.FM.",setup,QStringLiteral("ch.Modulation.FM"),QStringLiteral("Depth"));
    member("ch.Trigger.Source.",setup,QStringLiteral("ch.Trigger.Source"),QStringLiteral("preset"));
    member("ch.Sweep.Frequency.",setup,QStringLiteral("ch.Sweep.Frequency"),QStringLiteral("Start"));
    expect("scope alias",QStringLiteral("const input = Scope.Channel2; input."),QStringLiteral("input"),QStringLiteral("Scope1.Channel1"));
    member("scope method",QStringLiteral("const input = Scope.Channel2; input."),QStringLiteral("input"),QStringLiteral("measure"));
    expect("parameter alias",QStringLiteral("let f=Wavegen.Channel1.Simple.Frequency; f."),QStringLiteral("f"),QStringLiteral("Wavegen1.Channel1.Simple.Frequency"));
    member("parameter.value",QStringLiteral("let f=Wavegen.Channel1.Simple.Frequency; f."),QStringLiteral("f"),QStringLiteral("value"));
    expect("nested alias",QStringLiteral("const a = Wavegen.Channel1; const b=a.Modulation.AM; b."),QStringLiteral("b"),QStringLiteral("Wavegen1.Channel1.Modulation.AM"));
    expect("parenthesized alias",QStringLiteral("const ch=(Wavegen.Channel2); ch."),QStringLiteral("ch"),QStringLiteral("Wavegen1.Channel1"));
    expect("ternary wrong types rejected",QStringLiteral("let ch=flag ? Scope.Channel1 : Wavegen.Channel2; ch."),QStringLiteral("ch"),QString());
    expect("conditional unknown branch rejected",QStringLiteral("let ch=flag ? Wavegen.Channel1 : someUnknown; ch."),QStringLiteral("ch"),QString());
    expect("reassignment invalidates",QStringLiteral("let ch=Wavegen.Channel1; ch=123; ch."),QStringLiteral("ch"),QString());
    expect("reassignment changes",QStringLiteral("let ch=Wavegen.Channel1; ch=Scope.Channel2; ch."),QStringLiteral("ch"),QStringLiteral("Scope1.Channel1"));
    expect("block let shadow",QStringLiteral("let ch=Scope.Channel1; {let ch=Wavegen.Channel2; ch."),QStringLiteral("ch"),QStringLiteral("Wavegen1.Channel1"));
    expect("block shadow removed",QStringLiteral("let ch=Scope.Channel1; {let ch=Wavegen.Channel2;} ch."),QStringLiteral("ch"),QStringLiteral("Scope1.Channel1"));
    expect("block var function scoped",QStringLiteral("function f(){ {var ch=Wavegen.Channel1;} ch."),QStringLiteral("ch"),QStringLiteral("Wavegen1.Channel1"));
    expect("function scope isolated",QStringLiteral("function f(){var ch=Wavegen.Channel1;} ch."),QStringLiteral("ch"),QStringLiteral("ch"));
    expect("parameter shadows global",QStringLiteral("function f(Wavegen){Wavegen.Channel1."),QStringLiteral("Wavegen.Channel1"),QString());
    expect("unknown local shadows catalog",QStringLiteral("const Scope=unknown; Scope.Channel1."),QStringLiteral("Scope.Channel1"),QString());
    expect("comment not declaration",QStringLiteral("// var ch=Wavegen.Channel1;\n ch."),QStringLiteral("ch"),QStringLiteral("ch"));
    expect("block comment not declaration",QStringLiteral("/*let ch=Wavegen.Channel1;*/ ch."),QStringLiteral("ch"),QStringLiteral("ch"));
    expect("string not declaration",QStringLiteral("const text='var ch=Wavegen.Channel1'; ch."),QStringLiteral("ch"),QStringLiteral("ch"));
    expect("regex not declaration",QStringLiteral("const r=/var ch=Wavegen.Channel1/; ch."),QStringLiteral("ch"),QStringLiteral("ch"));
    expect("template not declaration",QStringLiteral("const s=`var ch=Wavegen.Channel1`; ch."),QStringLiteral("ch"),QStringLiteral("ch"));
    expect("explicit compatibility",QString(),QStringLiteral("Wavegen.Channel1.Simple"),QStringLiteral("Wavegen.Channel1.Simple"));
    expect("Serial factory connection",QStringLiteral("const port=Serial.create({port:'COM5'}); port."),QStringLiteral("port"),QStringLiteral("SerialConnection"));
    member("Serial port.write",QStringLiteral("const port=Serial.create({port:'COM5'}); port."),QStringLiteral("port"),QStringLiteral("write"));
    expect("TCP client factory",QStringLiteral("const client=TCP.create({host:'127.0.0.1',port:1234}); client."),QStringLiteral("client"),QStringLiteral("TCPConnection"));
    member("TCP client.on",QStringLiteral("const client=TCP.create({host:'127.0.0.1',port:1234}); client."),QStringLiteral("client"),QStringLiteral("connect"));
    expect("TCP server factory",QStringLiteral("const server=TCP.createServer({port:0}); server."),QStringLiteral("server"),QStringLiteral("TCPServer"));
    member("TCP server.listen",QStringLiteral("const server=TCP.createServer({port:0}); server."),QStringLiteral("server"),QStringLiteral("listen"));
    expect("UDP factory",QStringLiteral("const udp=UDP.create({port:0}); udp."),QStringLiteral("udp"),QStringLiteral("UDPConnection"));
    member("UDP datagram send",QStringLiteral("const udp=UDP.create({port:0}); udp."),QStringLiteral("udp"),QStringLiteral("send"));
    expect("BinaryBuffer from factory",QStringLiteral("const b=Binary.create(16); b."),QStringLiteral("b"),QStringLiteral("BinaryBuffer"));
    member("Binary buffer write",QStringLiteral("const b=Binary.fromHex('0102'); b."),QStringLiteral("b"),QStringLiteral("writeU16LE"));
    expect("BinaryStream from factory",QStringLiteral("const rx=Binary.stream(4096); rx."),QStringLiteral("rx"),QStringLiteral("BinaryStream"));
    member("Binary stream frame",QStringLiteral("const rx=Binary.stream(4096); rx."),QStringLiteral("rx"),QStringLiteral("readIEEE"));
    expect("BinaryBuffer from stream",QStringLiteral("const rx=Binary.stream(4096); const frame=rx.readIEEE(); frame."),QStringLiteral("frame"),QStringLiteral("BinaryBuffer"));
    expect("BinarySamples inferred",QStringLiteral("const b=Binary.create(16); const data=b.decodeSamples('u16le',0,8,1,0); data."),QStringLiteral("data"),QStringLiteral("BinarySamples"));
    member("BinarySamples bins",QStringLiteral("const b=Binary.create(16); const data=b.decodeSamples('u16le',0,8,1,0); data."),QStringLiteral("data"),QStringLiteral("minMaxBins"));
    expect("FileIO readBuffer inferred",QStringLiteral("const data=FileIO.readBuffer('test.bin'); data."),QStringLiteral("data"),QStringLiteral("BinaryBuffer"));
    expect("Binary unknown assignment rejected",QStringLiteral("const b=Binary.create(16); b=5; b."),QStringLiteral("b"),QString());
    return failures ? 1 : 0;
}
