#include "CompletionCatalog.h"
#include <QStringList>

namespace CompletionCatalog {
namespace {
using K = Kind;
void add(QVector<Item>& a, const QString& name, K k, const QString& detail) {
    a.append({name,k,detail});
}
void objects(QVector<Item>& a, const QStringList& names, const QString& prefix={}) {
    for (const QString& name : names) add(a,name,K::Object,prefix.isEmpty() ? QStringLiteral("Instrument or parameter object") : prefix);
}
void functions(QVector<Item>& a, const QStringList& names, const QString& description={}) {
    for (const QString& name:names) add(a,name,K::Function,description.isEmpty() ? QStringLiteral("Callable method") : description);
}
QString canonical(QString path) {
    const QStringList aliases={"Scope","Wavegen","Logic","Patterns","Protocol"};
    if(path=="Supplies1" || path.startsWith("Supplies1."))path.replace(0,9,"Supplies");
    for (const auto& alias:aliases) {
        if (path == alias || path.startsWith(alias + '.')) {
            path.replace(0,alias.size(),alias+QStringLiteral("1"));
            break;
        }
    }
    return path;
}
void numberProps(QVector<Item>& a) {
    add(a,"value",K::Number,"Requested setting; writable, usually in SI units");
    add(a,"real",K::Number,"Read-only effective/readback setting (may be unavailable before configuration)");
    add(a,"text",K::String,"Human-readable rendering of the numeric value");
    add(a,"preset",K::Array,"Available preset choices, if provided");
}
void enumProps(QVector<Item>& a) {
    add(a,"text",K::String,"Selected setting name; writable");
    add(a,"value",K::Number,"Selected choice index; writable");
    add(a,"real",K::Number,"Selected choice index");
    add(a,"preset",K::Array,"Available setting choices");
}
}
QVector<Item> entries(const QString& objectPath) {
    const QString p=canonical(objectPath);
    QVector<Item> a;
    if (p.isEmpty()) {
        objects(a,{"Scope1","Scope","Wavegen1","Wavegen","Logic1","Logic","Supplies","Supplies1","StaticIO","Patterns1","Patterns","Protocol1","Protocol","Device"},"WaveForms-compatible hardware controller");
        objects(a,{"ui"},"Panel designer interaction (panel scripts only)");
        objects(a,{"Data"},"Project-owned typed Panel Data shared across Pages and bound widgets");
        objects(a,{"Binary"},"Native binary buffers, streams and bulk numeric samples");
        objects(a,{"FileIO","Tool","Database"},"Panel file access, asynchronous dialogs and optional SQLite database");
        objects(a,{"Serial","TCP","UDP"},"Native opt-in PC serial and TCP/UDP communications; not AD3 digital pins");
        objects(a,{"TestRunner"},"Native Qt/C++ test executive; JavaScript-defined steps");
        objects(a,{"plots"},"Independent native Panel chart API (panel scripts only)");
        objects(a,{"console","Math","JSON","modules"},"JavaScript or library object");
        functions(a,{"include"},"Load a classic .js helper into the current script engine once");
        // Table-specific methods also appear under ui completion.
        functions(a,{"setTimeout","clearTimeout","setInterval","clearInterval"},
                  "Panel-only JavaScript timers backed by the Qt worker event loop; 10ms..24h, up to 256 active");
        functions(a,{"print","clear","wait","min","max","pow","round","abs","floor","ceil",
                     "sqrt","sin","cos","tan","asin","acos","atan","atan2","exp","log","log10"},
                  "WaveForms-style script helper");
        for (const QString word : {"var","let","const","function","if","else","for","while","return","throw","try","catch","new"})
            add(a,word,K::Keyword,"JavaScript keyword");
    } else if(p=="Binary") {
        functions(a,{"create","fromArray","fromHex","fromUtf8","fromBase64","concat",
                     "wrapIEEE","stream","pattern","wordPattern","clockUs"},
                  "Create native binary buffers; benchmark helpers pattern/wordPattern/clockUs");
        add(a,"lastError",K::String,"Most recent Binary factory error");
    } else if(p=="BinaryBuffer") {
        functions(a,{"slice","ieeePayload","readU8","readI8","readU16LE","readU16BE",
                     "readI16LE","readI16BE","readU32LE","readU32BE","readI32LE","readI32BE",
                     "readF32LE","readF32BE","readF64LE","readF64BE","readU64LE","readU64BE",
                     "readI64LE","readI64BE","writeU8","writeI8","writeU16LE","writeU16BE",
                     "writeI16LE","writeI16BE","writeU32LE","writeU32BE","writeI32LE","writeI32BE",
                     "writeF32LE","writeF32BE","writeF64LE","writeF64BE","writeU64LE","writeU64BE",
                     "writeI64LE","writeI64BE","fill","copyFrom","writeUtf8","toUtf8",
                     "toBase64","sum32","sum32Range","crc16","crc16Range","crc32","crc32Range","xor8","indexOf","equals",
                     "readBits","writeBits","readBits64","writeBits64","readSignal","writeSignal","statsU16LE","statsNumeric",
                     "decodeSamples","toHex","toArray","release"},
                  "Bounded native-buffer read/write, bit fields, checksums and bulk decoding");
        add(a,"length",K::Number,"Length of this byte buffer/view");
        add(a,"lastError",K::String,"Last native-buffer operation error");
    } else if(p=="BinaryStream") {
        functions(a,{"append","readLine","readIEEE","readFixed","readUntil","readLengthPrefixed","clear"},
                  "Native byte-stream accumulation and protocol framing");
        add(a,"pending",K::Number,"Buffered byte count");
        add(a,"lastError",K::String,"Most recent framing error; null can mean incomplete frame");
    } else if(p=="BinarySamples") {
        functions(a,{"stats","rangeStats","minMaxBins","toArray","at","release"},
                  "Native numeric sample storage, statistics and plot-ready min/max bins");
        add(a,"length",K::Number,"Native sample count");
        add(a,"lastError",K::String,"Last sample operation error");
    } else if(p=="Data") {
        functions(a,{"get","set","names","onChange","offChange","computed","isComputed"},"Shared typed values and change callbacks; subscribers run for Data.set and bound input edits");
    } else if(p=="Serial") {
        functions(a,{"ports","create"},"List PC serial ports or create an unopened QSerialPort connection");
    } else if(p=="TCP") {
        functions(a,{"create","createServer"},"Create a TCP client or local TCP listening server; explicit connect/listen");
    } else if(p=="UDP") {
        functions(a,{"create"},"Create an unbound UDP datagram socket; explicit bind");
    } else if(p=="SerialConnection" || p=="TCPConnection" || p=="TCPServer" || p=="UDPConnection") {
        functions(a,{"on","off","close","info"},"Node-style event handlers and connection lifecycle");
        if(p=="SerialConnection")functions(a,{"open","write"},"Open COM port; write UTF-8, byte[] or native BinaryBuffer");
        if(p=="TCPConnection")functions(a,{"connect","write"},"Connect asynchronously; write UTF-8, byte[] or native BinaryBuffer");
        if(p=="TCPServer")functions(a,{"listen"},"Listen for incoming TCP clients; on('connection',function(client){...})");
        if(p=="UDPConnection")functions(a,{"bind","send"},"Bind to local port / send a text or byte[] datagram");
    } else if(p=="FileIO") {
        functions(a,{"dataDirectory","dataPath","exists","readText","writeText","appendText","readJson","writeJson","readCsv","writeCsv","readBytes","writeBytes","readBuffer","writeBuffer","appendBuffer"},"Bounded file I/O; shared PanelData directory or operator-granted file");
    } else if(p=="Database") {
        functions(a,{"open","exec","query","close"},"Nonblocking Qt SQLite calls; database shared through the common PanelData folder");
    } else if(p=="Tool") {
        functions(a,{"question","getText","getNumber","getItem","getOpenFile","getSaveFile"},"Asynchronous Qt operator dialogs; callback receives {accepted,value,kind}");
    } else if(p=="Protocol1") {
        objects(a,{"UART","SPI","I2C","CAN","SWD"},"Native Digilent Chapter 13 digital protocol bridges");
        functions(a,{"stop","status"},"Stop and release digital resources / query active protocol");
        add(a,"lastError",K::String,"Last digital protocol shutdown error");
        add(a,"simulation",K::Boolean,"True when no hardware access is performed");
    } else if(p=="Protocol1.UART") {
        functions(a,{"configure","startRx","startAutoRx","stopAutoRx","onData","clearOnData","available","stats","write","writeText","read","Send","SendArray","Receiver","Receive","ReceiveArray"},
                  "UART TX/RX; byte arrays and UTF-8 text; no passive UART Spy decoder in SDK");
        add(a,"autoReceiving",K::Boolean,"True while native Qt timer drains the SDK into a bounded queue");
        add(a,"lastError",K::String,"Most recent UART failure");
        add(a,"configured",K::Boolean,"UART configured for this script session");
        add(a,"simulation",K::Boolean,"No hardware activity; empty receive data");
    } else if(p=="Protocol1.SPI") {
        functions(a,{"configure","transfer","command","Start","Stop","ReadWrite","Read","Read0","Write",
                     "WriteDual","ReadDual","WriteQuad","ReadQuad"},
                  "Native SPI master transactions: 1..32-bit words, SISO/standard/dual/quad, optional command/dummy");
        add(a,"lastError",K::String,"Most recent SPI failure");
        add(a,"configured",K::Boolean,"SPI configured for this script session");
        add(a,"simulation",K::Boolean,"No hardware activity; simulated RX is zero-filled and marked");
    } else if(p=="Protocol1.I2C") {
        functions(a,{"configure","writeRead","write","read","Clear","Receiver","spyPoll","Receive",
                     "Write","Read","ReadSub"},
                  "I2C master with 7-bit addresses and native I2C Spy; no native slave emulation");
        add(a,"lastError",K::String,"Most recent I2C failure or NACK details");
        add(a,"configured",K::Boolean,"I2C configured for this script session");
        add(a,"simulation",K::Boolean,"No hardware activity; ACK is not simulated");
    } else if(p=="Protocol1.CAN") {
        functions(a,{"configure","startRx","send","receive","Send","Receiver","Receive"},
                  "Classical CAN 11/29-bit identifiers; no native CAN FD support");
        add(a,"lastError",K::String,"Most recent classical CAN SDK error");
        add(a,"configured",K::Boolean,"CAN configured for this script session");
        add(a,"simulation",K::Boolean,"No hardware activity; no simulated frames");
    } else if(p=="Protocol1.SWD") {
        functions(a,{"configure","clear","readRegister","writeRegister"},
                  "SWD debug-port AP/DP operations with ACK and parity status");
        add(a,"lastError",K::String,"Most recent SWD SDK error");
        add(a,"configured",K::Boolean,"SWD configured for this script session");
        add(a,"simulation",K::Boolean,"No hardware activity");
    } else if(p=="Device") {
        functions(a,{"isConnected"},"Whether a hardware or simulated device is available");
        add(a,"name",K::String,"Connected device name");add(a,"SN",K::String,"Connected device serial number");
    } else if(p=="Scope1") {
        functions(a,{"single","run","config","wait","stop","capabilities","hardwareStatus","captureInfo","saveReference","referenceData","mathData"},"Acquisition lifecycle; run rearms after each wait");
        objects(a,{"Time","Trigger","Channel1","Channel2"});
        add(a,"lastError",K::String,"Last SDK or acquisition error");
        add(a,"configurationCount",K::Number,"WFScript: number of committed hardware Scope setups");
        add(a,"triggerConfigurationCount",K::Number,"WFScript: number of committed hardware trigger setups");
        add(a,"acquisitionCount",K::Number,"WFScript: number of successful hardware acquisition arm commands");
        add(a,"recordLostSamples",K::Number,"Lost Record FIFO samples; invalidates capture");
        add(a,"recordCorruptSamples",K::Number,"Corrupted Record FIFO samples; invalidates capture");
        add(a,"lastSettleMs",K::Number,"WFScript: wait on most recent Scope arm, in ms; zero for unchanged settings");
    } else if(p=="Scope1.Time") {
        objects(a,{"Rate","Samples","TriggerPositionPercent","Mode","RecordLength"});
        add(a,"taken",K::String,"Host UTC time after transfer; not device trigger time");
        add(a,"takenEpochMs",K::Number,"Host timestamp in milliseconds after transfer");
        add(a,"timestampSource",K::String,"HostAfterTransfer; device trigger ticks in Scope.captureInfo()");
    }
    else if(p=="Scope1.Time.Rate" || p=="Scope1.Time.Samples" ||
            p=="Scope1.Time.RecordLength" || p=="Scope1.Trigger.Length" ||
            p=="Scope1.Trigger.LCondition" || p=="Scope1.Trigger.HoldOff" ||
            p=="Scope1.Trigger.Level" || p=="Scope1.Trigger.Hysteresis" ||
            p=="Scope1.Channel1.Offset" || p=="Scope1.Channel2.Offset" ||
            p=="Scope1.Channel1.Attenuation" || p=="Scope1.Channel2.Attenuation" ||
            p=="Scope1.Time.TriggerPositionPercent") numberProps(a);
    else if(p=="Scope1.Channel1.Range" || p=="Scope1.Channel2.Range") {
        numberProps(a);
        add(a,"steps",K::Array,"SDK-reported selectable Vpp ranges; empty in simulation or when unsupported");
    }
    else if(p=="Scope1.Time.Mode" || p=="Scope1.Channel1.SampleMode" ||
            p=="Scope1.Channel2.SampleMode" || p=="Scope1.Trigger.Filter") enumProps(a);
    else if(p=="Scope1.Trigger") objects(a,{"Trigger","Type","Source","Condition","Level","Hysteresis","LCondition","Length","HoldOff","Filter"});
    else if(p=="Scope1.Trigger.Trigger" || p=="Scope1.Trigger.Type" ||
            p=="Scope1.Trigger.Source" || p=="Scope1.Trigger.Condition") enumProps(a);
    else if(p=="Scope1.Channel1" || p=="Scope1.Channel2") {
        objects(a,{"Range","Offset","Attenuation","SampleMode"});
        add(a,"data",K::Array,"Last acquired voltage samples");
        functions(a,{"getData","getVisibleData","measure","measureCapture","measureWindow","statistics","spectrum","sample","timeOfIndex"},
                  "Software measurements; measure(name,startSeconds,endSeconds) uses a time window; spectrum(N,window) provides FFT volts.");
    } else if(p=="Wavegen1") {
        functions(a,{"run","start","stop","config","capabilities","status","play"},"Configure and control enabled generator channels");
        objects(a,{"Channel1","Channel2"});
        add(a,"lastError",K::String,"Last SDK error");
    } else if(p=="Wavegen1.Channel1" || p=="Wavegen1.Channel2") {
        add(a,"enable",K::Boolean,"Enable this output channel");
        objects(a,{"Mode","Simple","Custom","Burst","Modulation","Sweep","Trigger","Play"});
    } else if(p.endsWith(".Mode") && p.startsWith("Wavegen1.Channel")) enumProps(a);
    else if(p.endsWith(".Simple") && p.startsWith("Wavegen1.Channel")) objects(a,{"Type","Frequency","Period","Amplitude","Offset","Symmetry","Phase"});
    else if(p.endsWith(".Custom") && p.startsWith("Wavegen1.Channel")) {
        functions(a,{"set"},"WFScript extension: upload normalized [-1,1] samples for Simple.Type=Custom");
        add(a,"length",K::Number,"Current arbitrary waveform sample count");
    } else if(p.endsWith(".Modulation") && p.startsWith("Wavegen1.Channel")) objects(a,{"FM","AM"});
    else if((p.endsWith(".Modulation.FM")||p.endsWith(".Modulation.AM")) && p.startsWith("Wavegen1.Channel")) {
        add(a,"enable",K::Boolean,"Enable optional hardware modulation node");objects(a,{"Type","Frequency","Depth"});
    } else if(p.endsWith(".Sweep") && p.startsWith("Wavegen1.Channel")) objects(a,{"Frequency","Amplitude"});
    else if((p.endsWith(".Sweep.Frequency")||p.endsWith(".Sweep.Amplitude"))&&p.startsWith("Wavegen1.Channel")) {
        add(a,"enable",K::Boolean,"Enable linear sweep axis");objects(a,{"Start","Stop","Time"});
    } else if(p.endsWith(".Trigger") && p.startsWith("Wavegen1.Channel")) {
        objects(a,{"Source","Idle"});add(a,"RepeatTrigger",K::Boolean,"Rearm trigger before each burst repeat");
    } else if(p.endsWith(".Play")&&p.startsWith("Wavegen1.Channel")) {
        objects(a,{"Rate"});add(a,"length",K::Number,"Number of normalized samples in streaming buffer");
        functions(a,{"set"},"Upload finite normalized samples for Wavegen.play(channel,timeout)");
    } else if((p.endsWith(".Modulation.FM.Type") || p.endsWith(".Modulation.AM.Type")) &&
              p.startsWith("Wavegen1.Channel")) enumProps(a);
    else if(p.contains(".Modulation.") && p.startsWith("Wavegen1.Channel")) numberProps(a);
    else if(p.contains(".Sweep.") && p.startsWith("Wavegen1.Channel")) numberProps(a);
    else if(p.endsWith(".Trigger.Source")||p.endsWith(".Trigger.Idle")) enumProps(a);
    else if(p.endsWith(".Play.Rate")&&p.startsWith("Wavegen1.Channel")) numberProps(a);
    else if(p.endsWith(".Burst") && p.startsWith("Wavegen1.Channel"))
        objects(a,{"Run","Wait","Repeat","Master"});
    else if(p.contains(".Burst.") && p.startsWith("Wavegen1.Channel")) numberProps(a);
    else if(p.endsWith(".Simple.Type") && p.startsWith("Wavegen1.Channel")) enumProps(a);
    else if(p.contains(".Simple.") && p.startsWith("Wavegen1.Channel")) numberProps(a);
    else if(p=="Supplies" || p=="Supplies1") {
        functions(a,{"run","config","stop","status"},"AD3 V+/V- control; commit changes explicitly; simulation supported");
        objects(a,{"MasterEnable","Output"});
        add(a,"lastError",K::String,"Last supply validation or SDK error");
        add(a,"running",K::Boolean,"True when master is on and at least one output is enabled");
    } else if(p=="Supplies.MasterEnable" || p=="Supplies1.MasterEnable") {
        add(a,"checked",K::Boolean,"Master ON/OFF; call Supplies.config() to apply staged state");
    } else if(p=="Supplies.Output" || p=="Supplies1.Output") {
        objects(a,{"PositiveSupply","NegativeSupply"});
    } else if(p=="Supplies.Output.PositiveSupply" || p=="Supplies.Output.NegativeSupply" ||
              p=="Supplies1.Output.PositiveSupply" || p=="Supplies1.Output.NegativeSupply") {
        objects(a,{"Enable","Voltage"});
    } else if((p.startsWith("Supplies.Output.") || p.startsWith("Supplies1.Output.")) && p.endsWith(".Enable")) {
        add(a,"value",K::Number,"0 = OFF, 1 = ON; commit with Supplies.config()/run()");
    } else if((p.startsWith("Supplies.Output.") || p.startsWith("Supplies1.Output.")) && p.endsWith(".Voltage")) {
        add(a,"value",K::Number,"AD3 V+ 0.5..5 V; V- -5..-0.5 V");
        add(a,"real",K::Number,"Most recent voltage status; simulation is an ideal model");
    } else if(p=="StaticIO") {
        objects(a,{"Channel0","Channel1"},"Eight DIO pins per WaveForms Static I/O group");
        functions(a,{"config","setPin","readPin","readGroup","status","capabilities","stop",
                     "onChange","clearOnChange","clearAllOnChange","startMonitor","stopMonitor","monitorStats"},
                  "Static digital I/O: explicitly started read-only input monitoring; output control remains separate");
        add(a,"monitoring",K::Boolean,"True only after explicit startMonitor(), false after stopMonitor()");
        add(a,"lastError",K::String,"Last StaticIO SDK or validation error");
        add(a,"simulation",K::Boolean,"True when running a software-only ideal echo model");
    } else if(p=="StaticIO.Channel0" || p=="StaticIO.Channel1") {
        const int first = p.endsWith("Channel0") ? 0 : 8;
        for(int i=first;i<first+8;++i)
            add(a,QStringLiteral("DIO%1").arg(i),K::Object,QStringLiteral("DIO output and input signal"));
        objects(a,{"Input","Mode"},"Group input byte, and fixed I/Os display mode");
    } else if(p=="StaticIO.Channel0.Mode" || p=="StaticIO.Channel1.Mode") {
        add(a,"text",K::String,"Read-only I/Os group mode (Slider/Progress not yet implemented)");
    } else if(p=="StaticIO.Channel0.Input" || p=="StaticIO.Channel1.Input") {
        add(a,"value",K::Number,"Fresh read-only 8-bit input mask; -1 indicates SDK read error");
    } else if(p.startsWith("StaticIO.Channel") && p.contains(".DIO")) {
        if(p.endsWith(".Input")) {
            add(a,"value",K::Number,"Actual pin logic level 0/1; -1 on SDK read failure");
        } else if(p.count(QLatin1Char('.'))==2) {
            add(a,"value",K::Number,"0=LOW, 1=HIGH, 2=Z. Staged until config() is called");
            add(a,"text",K::String,"'0', '1', or 'Z' output command");
            objects(a,{"Input"},"Input level read from fresh SDK snapshot");
        }
    } else if(p=="Patterns1") {
        functions(a,{"run","config","stop","status","runEncoder","configEncoder","runSequence","configSequence"},
                  "Hardware-timed Clock/PWM, 3-channel encoder, and repeating multi-channel custom bit sequences");
        objects(a,{"Channels"},"WaveForms-style Pattern Generator channel group");
        add(a,"running",K::Boolean,"True if this WFScript bridge started the pattern");
        add(a,"simulation",K::Boolean,"True in software-only mode; no real digital output");
        add(a,"clockHz",K::Number,"Native SDK DigitalOut base clock; zero before configuration");
        add(a,"lastError",K::String,"Last DigitalOut SDK or settings error");
    } else if(p=="Patterns1.Channels") {
        objects(a,{"DIO0","DIO1","DIO2"},"Pattern channel property objects; runEncoder() configures three synchronized outputs");
    } else if(p=="Patterns1.Channels.DIO0" || p=="Patterns1.Channels.DIO1" || p=="Patterns1.Channels.DIO2") {
        objects(a,{"Pin","DIO","Type","Output","Idle","Frequency","Duty"},
                "WaveForms-style properties; values staged until config() or run()");
    } else if(p=="Patterns1.Channels.DIO0.Pin" || p=="Patterns1.Channels.DIO0.DIO" || p=="Patterns1.Channels.DIO1.Pin" || p=="Patterns1.Channels.DIO1.DIO" || p=="Patterns1.Channels.DIO2.Pin" || p=="Patterns1.Channels.DIO2.DIO") {
        add(a,"value",K::Number,"Physical output pin index (integer 0..15)");
    } else if(p=="Patterns1.Channels.DIO0.Type" || p=="Patterns1.Channels.DIO1.Type" || p=="Patterns1.Channels.DIO2.Type") {
        add(a,"text",K::String,"Clock for run(), Custom readback after runEncoder()");
    } else if(p=="Patterns1.Channels.DIO0.Output" || p=="Patterns1.Channels.DIO1.Output" || p=="Patterns1.Channels.DIO2.Output") {
        add(a,"text",K::String,"PP push-pull only in v5.6.48");
    } else if(p=="Patterns1.Channels.DIO0.Idle" || p=="Patterns1.Channels.DIO1.Idle" || p=="Patterns1.Channels.DIO2.Idle") {
        add(a,"text",K::String,"Z (high impedance) only in v5.6.48");
    } else if((p.startsWith("Patterns1.Channels.DIO0.") || p.startsWith("Patterns1.Channels.DIO1.") || p.startsWith("Patterns1.Channels.DIO2.")) && (p.endsWith(".Frequency") || p.endsWith(".Duty"))) {
        add(a,"value",K::Number,"Requested Hz or percent; configurable before run()");
        add(a,"real",K::Number,"Achieved SDK-counter value after run()/config()");
    } else if(p=="Logic1") {
        functions(a,{"single","wait","stop","samples","captureDiagnostics"},"16-bit DigitalIn capture; single arms, wait transfers completed samples");
        objects(a,{"Time"},"DigitalIn acquisition settings");
        add(a,"sampleRateHz",K::Number,"Read-only actual DigitalIn sampling rate");
        add(a,"lastError",K::String,"Last SDK error");
    } else if(p=="Logic1.Time") {
        objects(a,{"Samples","Rate"},"DigitalIn requested sample count and sample rate");
    } else if(p=="Logic1.Time.Samples" || p=="Logic1.Time.Rate") {
        numberProps(a);
    } else if(p=="TestRunner") {
        functions(a,{"configure","add","onFinished","clear","reset","run","next","abort","terminate","report","measure",
                     "summary","results","state","setRunInfo","runInfo","pendingRunInfo","setVariable","variable","variables",
                     "saveReport","exportReport","archiveInfo","loadDefinition","loadDefinitionFile","saveDefinition","definition",
                     "complete","fail","isActive"},
                  "Qt/C++ sequencer owns step execution, measurements, limits and widget updates");
        add(a,"lastError",K::String,"Last TestRunner configuration or step exception");
    } else if(p=="ui") {
        functions(a,{"tableRowCount","tableSlice","tableRow","tableSelection",
                     "tableSetTitle","tableSetStatus","tableFollow",
                     "tableImportCsv","tableExportCsv","tableCopySelection"},
                  "Experiment DataTable: bounded reads, selection, header controls and CSV actions");
        functions(a,{"on","get","set"},"Panel widget state, structured Test Sequence / Measurement Table / Event Log updates, and callbacks");
    } else if(p=="plots") {
        functions(a,{"setSeries","setNativeSamples","nativeSeriesInfo",
                     "getView","setView","onViewChanged",
                     "setSamples","fromScope","append","appendPoint","styleSeries",
                     "configure","removeSeries","clear"},
                  "Panel-only native plots: explicit data delivery to a plot widget ID");
    } else if(p=="modules") {
        functions(a,{"load"},"Import a native .mjs module; exports are returned in a namespace");
        functions(a,{"userFolder"},"Location of the writable shared WFScript libraries folder");
    } else if(p=="console") functions(a,{"log","warn","error"});
    else if(p=="Math") functions(a,{"sin","cos","tan","asin","acos","atan","atan2","sqrt","pow","abs","min","max","round","floor","ceil","log","log10","exp"});
    return a;
}
}
