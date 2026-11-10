#include "ApiKnowledge.h"
#include "CompletionCatalog.h"
#include <QRegularExpression>
#include <QStringList>
#include <QHash>

namespace ApiKnowledge {
namespace {
struct Detail {
    const char* path;
    const char* signature;
    const char* parameters;
    const char* unit;
    const char* example;
    const char* notes;
};
// Curated detail is SUPPLEMENTAL. Symbol availability and the short summary
// always come from CompletionCatalog. Adding an API must update that inventory;
// the automated tests fail if a curated path no longer exists.
static const Detail kDetails[] = {
    {"Patterns1.runSequence", "runSequence(pins, bitStrings, bitRateHz) : boolean", "1..3 distinct DIO pins; 2..1024 bits/channel; 10..1,000,000 logical bits/s", "bits/s", "if(!Patterns1.runSequence([0,1,2],[\"01010101\",\"00110011\",\"00001111\"],1000)) throw Error(Patterns1.lastError);", "WFScript-specific extension, not a native WaveForms script method. The first character in each string is generated first, all strings must contain only 0/1 and be equal length. One shared DigitalOut start and divider keep channels aligned. Repeats continuously until stop(); SDK range and buffer limits are discovered at runtime. All output pins return to high-Z on stop. For tests connect DIO0->DIO3, DIO1->DIO4, DIO2->DIO5 through separate 1k resistors."},
    {"Patterns1.configSequence", "configSequence(pins, bitStrings, bitRateHz) : boolean", "Same arguments as runSequence", "bits/s", "Patterns1.configSequence([0,1],[\"0101\",\"0011\"],1000);", "Configure repeating multi-pin custom DigitalOut buffers but do not start them. Call runSequence to start. Stop releases owned outputs to high impedance."},
    {"Patterns1.runEncoder", "runEncoder(aPin, bPin, indexPin, ppr, rpm, reverse) : boolean", "Three distinct DIO pins 0..15; 1..1024 PPR; 1..10000 RPM; reverse boolean", "PPR, RPM", "if(!Patterns1.runEncoder(0,1,2,16,600,false)) throw Error(Patterns1.lastError);", "WFScript-specific convenience extension: generates coherent Gray-code quadrature A/B and one index pulse per revolution using one common DigitalOut start. Hardware custom buffer must hold 4*PPR bits on ALL three channels. Direction changes require stop/restart. Call stop() to release all three outputs to Z. Use a 3-resistor DIO0->3, 1->4, 2->5 loopback for physical verification."},
    {"Patterns1.configEncoder", "configEncoder(aPin, bPin, indexPin, ppr, rpm, reverse) : boolean", "Same arguments as runEncoder", "PPR, RPM", "Patterns1.configEncoder(0,1,2,16,600,false);", "Load synchronized encoder data without starting the output. Explicit runEncoder() starts. Not a native WaveForms scripting method: WFScript extension."},
    {"Patterns1.Channels.DIO1.DIO.value", "DIO.value : integer", "Physical output pin, 0..15", "pin", "print(Patterns1.Channels.DIO1.DIO.value);", "Second logical channel for an encoder B phase; last configured physical pin readback."},
    {"Patterns1.Channels.DIO2.DIO.value", "DIO.value : integer", "Physical output pin, 0..15", "pin", "print(Patterns1.Channels.DIO2.DIO.value);", "Third logical channel for index output; last configured physical pin readback."},
    {"Patterns1.run", "run() : boolean", "No arguments", "", "Patterns1.Channels.DIO0.Frequency.value=1000;\nif(!Patterns1.run()) throw Error(Patterns1.lastError);", "Starts one native DigitalOut Clock pattern. Explicit start only; no output when the Panel loads. Return output to high-Z with stop()."},
    {"Patterns1.config", "config() : boolean", "No arguments", "", "if(!Patterns1.config()) throw Error(Patterns1.lastError);", "Stages and commits DigitalOut without starting generation. Output idle state is Z."},
    {"Patterns1.stop", "stop() : boolean", "No arguments", "", "if(!Patterns1.stop()) print(Patterns1.lastError);", "Stops the pattern generator and disables the owned channel output. The runtime attempts stop at script/Panel exit."},
    {"Patterns1.status", "status() : object", "No arguments", "Hz and percent", "var p=Patterns1.status(); print(p.actualFrequencyHz,p.actualDutyPercent,p.simulation);", "Returns cached running/mode plus the SDK-read sdkStateName, sdkStateCode, sdkRunning, sdkRepeatCount (0=infinite), sdkStatusAvailable, sdkRepeatAvailable and achieved encoder RPM or sequence bit rate. Sequence status includes sequencePins, sequenceBitStrings, sequenceLength, sequenceActualBitRateHz and sequenceRepeatFactor. sdkRepeatCount is diagnostic only and may be nonzero despite continuous Run=0. Native fields are optional and distinguish actual DigitalOut from the cached workflow flag."},
    {"Patterns1.Channels.DIO0.DIO.value", "DIO.value : integer", "Physical AD3 pin index 0..15", "pin", "Patterns1.Channels.DIO0.DIO.value=0;", "WaveForms-compatible physical pin selector. Synonym for WFScript Pin.value; one clock/PWM channel in this revision."},
    {"Patterns1.Channels.DIO0.Pin.value", "Pin.value : integer", "Integer pin index 0..15", "pin", "Patterns1.Channels.DIO0.Pin.value=0;", "One logical WaveForms-style Pattern channel selects a physical pin. For custom 1..3 channel bit sequences use Patterns1.runSequence()."},
    {"Patterns1.Channels.DIO0.Type.text", "Type.text : string", "Clock", "", "Patterns1.Channels.DIO0.Type.text='Clock';", "Clock uses run(); Custom is readback after runEncoder() or runSequence(). The array-based multi-channel sequencer is a WFScript extension."},
    {"Patterns1.Channels.DIO0.Output.text", "Output.text : string", "PP", "", "Patterns1.Channels.DIO0.Output.text='PP';", "Push-pull only; requires 3.3-V-safe hardware and no external drivers on the selected pin."},
    {"Patterns1.Channels.DIO0.Idle.text", "Idle.text : string", "Z", "", "Patterns1.Channels.DIO0.Idle.text='Z';", "High-impedance idle state. Explicit stop also disables the Pattern channel output."},
    {"Patterns1.Channels.DIO0.Frequency.value", "Frequency.value : number", "1 to 1,000,000", "Hz", "Patterns1.Channels.DIO0.Frequency.value=1000;", "Clock frequency. Achieved frequency is returned by Frequency.real after configuring."},
    {"Patterns1.Channels.DIO0.Frequency.real", "Frequency.real : number", "Read only", "Hz", "print(Patterns1.Channels.DIO0.Frequency.real);", "Achieved DigitalOut counter/divider frequency from the SDK-reported base clock, not a measured signal."},
    {"Patterns1.Channels.DIO0.Duty.value", "Duty.value : number", "1 to 99", "%", "Patterns1.Channels.DIO0.Duty.value=50;", "Duty percentage; achieved value may differ after integer counter rounding."},
    {"Patterns1.Channels.DIO0.Duty.real", "Duty.real : number", "Read only", "%", "print(Patterns1.Channels.DIO0.Duty.real);", "Achieved DigitalOut counter setting; physical duty is measured separately through Logic1."},
    {"Logic1.single", "single() : boolean", "No arguments", "", "Logic1.Time.Rate.value=20000;\nLogic1.Time.Samples.value=4096;\nif(!Logic1.single()) throw Error(Logic1.lastError);", "Starts an untriggered 16-bit acquisition. This function returns after arming; call wait() to finish and transfer samples. No digital output is driven."},
    {"Logic1.wait", "wait(timeoutSeconds = 5) : boolean", "Positive timeout seconds", "s", "if(!Logic1.wait(3)) throw Error(Logic1.lastError);", "Waits until acquisition Done and loads the 16-bit sample words. Returns false on timeout or stop."},
    {"Logic1.captureDiagnostics", "captureDiagnostics() : object", "No arguments; last completed capture", "words / bytes / transitions", "var d=Logic1.captureDiagnostics();\nprint(d.bytesRequested, d.firstHalfChanges, d.secondHalfChanges);", "WFScript extension: reports requested/returned 16-bit word count, requested BYTES, optional SDK valid-sample count and state, and first/second-half DIO0..5 activity. Zero activity is valid for an idle input. The SDK data API accepts bytes, not sample count; this is fixed in 5.6.49.2."},
    {"Logic1.samples", "samples() : integer[]", "No arguments", "16-bit masks", "var data=Logic1.samples();\nvar dio1=data.map(w=>(w>>1)&1);", "Returns complete sample words after a successful wait(). Each bit corresponds to one DIO pin (0..15)."},
    {"Logic1.Time.Rate.value", "Rate.value : number", "Sample rate in Hz", "Hz", "Logic1.Time.Rate.value=20000;", "SDK reads the native DigitalIn clock and rounds to a positive integer divider. Rate.real and Logic1.sampleRateHz show effective sampling rate after single()."},
    {"Logic1.Time.Rate.real", "Rate.real : number", "Read only", "Hz", "print(Logic1.Time.Rate.real);", "Effective DigitalIn sample frequency from device clock divided by configured integer divider."},
    {"Logic1.Time.Samples.value", "Samples.value : number", "16..32768 samples, subject to device limit", "samples", "Logic1.Time.Samples.value=4096;", "v5.6.47 bounded single capture; requests larger than hardware DigitalIn buffer are rejected."},
    {"Logic1.sampleRateHz", "sampleRateHz : number", "Read only", "Hz", "print(Logic1.sampleRateHz);", "Actual digital acquisition sample rate after Logic1.single(). Use this value for plot timebase."},

    {"Device.isConnected", "isConnected() : boolean", "No arguments", "", "if (!Device.isConnected()) throw new Error(\"No device\");", "A connected AD3 or the selected simulation can satisfy this query."},
    {"StaticIO.config", "config() : boolean", "No arguments", "", "StaticIO.Channel0.DIO0.value=1;\nif(!StaticIO.config()) throw Error(StaticIO.lastError);", "WFScript explicitly commits staged pin values; unlike the WaveForms Static I/O GUI, property assignment alone does not drive the pin. Disabled pins default to Z."},
    {"StaticIO.setPin", "setPin(index, state) : boolean", "index 0..15, state 0 low / 1 high / 2 high-Z", "", "if(!StaticIO.setPin(0,1)) throw Error(StaticIO.lastError);", "WFScript convenience method that updates and commits one pin; changes hardware outputs in Physical Device mode."},
    {"StaticIO.readPin", "readPin(index) : integer", "index 0..15", "0/1/-1", "print(StaticIO.readPin(1));", "Performs a new DigitalIOStatus snapshot before reading the input bit. -1 means failure; check StaticIO.lastError."},
    {"StaticIO.readGroup", "readGroup(start) : integer", "start=0 or 8", "0..255/-1", "print(StaticIO.readGroup(0));", "Reads all 8 pins in the group in one snapshot; -1 means an SDK error."},
    {"StaticIO.status", "status() : object", "No arguments", "16-bit masks", "var s=StaticIO.status();\nprint(s.valid, s.inputMask, s.outputEnableMask);", "Read-only DigitalIO snapshot. inputMask is actual hardware digital levels; a simulation returns ideal output echo with undriven inputs LOW."},
    {"StaticIO.capabilities", "capabilities() : object", "No arguments", "16-bit masks", "var c=StaticIO.capabilities();\nprint(c.outputMask.toString(16));", "Queries SDK capabilities without changing directions. 0xffff is the ideal simulation mask. availability is device-dependent."},
    {"StaticIO.stop", "stop() : boolean", "No arguments", "", "if(!StaticIO.stop()) print(StaticIO.lastError);", "Releases the StaticIO output-enable mask to 0 (all high impedance). Runtime performs cleanup if StaticIO was touched; failure to confirm high-Z is warned."},
    {"StaticIO.Channel0.DIO0.value", "value : integer", "0=LOW, 1=HIGH, 2=Z", "", "StaticIO.Channel0.DIO0.value=1;\nStaticIO.config();", "WaveForms-compatible property; this WFScript subset deliberately stages assignments until config(). DIO8..15 live under Channel1."},
    {"StaticIO.Channel0.DIO0.text", "text : string", "'0', '1', or 'Z'", "", "StaticIO.Channel0.DIO0.text='Z';\nStaticIO.config();", "Invalid text is rejected on commit. Staged output state is separate from physical input state."},
    {"StaticIO.Channel0.DIO0.Input.value", "value : integer", "Read-only", "0/1/-1", "print(StaticIO.Channel0.DIO0.Input.value);", "Fresh SDK snapshot of the pin's actual logic level. Returns -1 for SDK error, not a fabricated LOW."},
    {"StaticIO.Channel1.DIO8.value", "value : integer", "0=LOW, 1=HIGH, 2=Z", "", "StaticIO.Channel1.DIO8.value=2;\nStaticIO.config();", "Channel1 contains DIO8..15. Outputs default to high impedance."},
    {"StaticIO.Channel1.DIO8.Input.value", "value : integer", "Read-only", "0/1/-1", "print(StaticIO.Channel1.DIO8.Input.value);", "Fresh input level for DIO8. Use StaticIO.status() to read all pins efficiently."},
    {"StaticIO.Channel0.Input.value", "value : integer", "Read-only", "0..255/-1", "print(StaticIO.Channel0.Input.value);", "Eight-bit input mask for DIO0..7. SDK read error returns -1."},
    {"StaticIO.Channel1.Input.value", "value : integer", "Read-only", "0..255/-1", "print(StaticIO.Channel1.Input.value);", "Eight-bit input mask for DIO8..15. SDK read error returns -1."},
    {"Supplies.run", "run() : boolean", "No arguments", "", "Supplies.Output.PositiveSupply.Voltage.value = 3.3;\nSupplies.Output.PositiveSupply.Enable.value = 1;\nif (!Supplies.run()) throw Error(Supplies.lastError);", "Explicitly turns master on and commits both output channels. Power is OFF at Panel/script completion. No output is energized in simulation."},
    {"Supplies.config", "config() : boolean", "No arguments", "", "Supplies.MasterEnable.checked = true;\nif (!Supplies.config()) throw Error(Supplies.lastError);", "Commits the staged WaveForms-style settings; unlike WaveForms' UI-backed properties, WFScript requires an explicit commit. Invalid settings fail closed."},
    {"Supplies.stop", "stop() : boolean", "No arguments", "", "if (!Supplies.stop()) print(Supplies.lastError);", "Disables AD3 supply master; the script and Panel runtime also call stop on cleanup if supplies were used."},
    {"Supplies.status", "status() : object", "No arguments", "V", "var s = Supplies.status();\nprint(s.measuredAvailable, s.positiveVoltage, s.negativeVoltage);", "Reads AD3 AnalogIO status. measuredAvailable=false means no physical voltage readback; simulation values are ideal and not measurements."},
    {"Supplies.MasterEnable.checked", "checked : boolean", "true/false", "", "Supplies.MasterEnable.checked = true;\nSupplies.config();", "WaveForms-compatible master enable property. Staged until config(); run() also turns on master."},
    {"Supplies.Output.PositiveSupply.Enable.value", "value : integer", "0 or 1", "", "Supplies.Output.PositiveSupply.Enable.value = 1;", "V+ channel enabled only if master is also on. Changes require config()/run()."},
    {"Supplies.Output.NegativeSupply.Enable.value", "value : integer", "0 or 1", "", "Supplies.Output.NegativeSupply.Enable.value = 1;", "V- channel enabled only if master is also on. Changes require config()/run()."},
    {"Supplies.Output.PositiveSupply.Voltage.value", "value : number", "0.5 to 5.0", "V", "Supplies.Output.PositiveSupply.Voltage.value = 3.3;", "AD3 voltage command, bounded; changes are staged until config()/run()."},
    {"Supplies.Output.NegativeSupply.Voltage.value", "value : number", "-5.0 to -0.5", "V", "Supplies.Output.NegativeSupply.Voltage.value = -3.3;", "Negative rail signed voltage. Both setpoints must remain in range, even when output disabled."},
    {"Supplies.Output.PositiveSupply.Voltage.real", "real : number", "No arguments", "V", "Supplies.status(); print(Supplies.Output.PositiveSupply.Voltage.real);", "Most recent SDK voltage status, not guaranteed accurate if readback unavailable. Check status().measuredAvailable."},
    {"Supplies.Output.NegativeSupply.Voltage.real", "real : number", "No arguments", "V", "Supplies.status(); print(Supplies.Output.NegativeSupply.Voltage.real);", "Most recent SDK voltage status; check status().measuredAvailable. Simulation reports an ideal model."},
    {"Wavegen1.run", "run() : boolean", "No arguments", "", "if (!Wavegen.run()) throw new Error(Wavegen.lastError);", "Configures and starts enabled waveform generators. Changes physical outputs in Hardware mode."},
    {"Wavegen1.stop", "stop() : boolean", "No arguments", "", "Wavegen.stop();", "Stops the generator."},
    {"Wavegen1.Channel1.enable", "enable : boolean", "true or false", "", "Wavegen.Channel1.enable = true;", "Selects whether a generator channel is enabled."},
    {"Wavegen1.Channel1.Simple.Frequency.value", "Frequency.value : number", "Frequency in hertz", "Hz", "Wavegen.Channel1.Simple.Frequency.value = 1000;", "Effective output range depends on the device configuration."},
    {"Wavegen1.Channel1.Simple.Amplitude.value", "Amplitude.value : number", "Amplitude in volts (peak)", "V peak", "Wavegen.Channel1.Simple.Amplitude.value = 1;", "The generated sine wave has approximately 0.707 V RMS at 1 V peak."},
    {"Wavegen1.Channel1.Simple.Offset.value", "Offset.value : number", "DC offset", "V", "Wavegen.Channel1.Simple.Offset.value = 0;", "Output voltage limits depend on range and offset combined."},
    {"Wavegen1.Channel1.Simple.Type.text", "Type.text : string", "Supported waveform name", "", "Wavegen.Channel1.Simple.Type.text = \"Sine\";", "Use autocomplete inside quotes for available choices; selected hardware may restrict them."},
    {"Wavegen1.Channel1.Mode.text", "Mode.text : string", "Generator operating mode", "", "Wavegen.Channel1.Mode.text = \"Simple\";", "Use autocomplete inside quotes for supported modes."},
    {"Scope1.single", "single() : boolean", "No arguments", "", "if (!Scope.single()) throw new Error(Scope.lastError);", "Arms an acquisition. Call Scope.wait() to obtain completed data."},
    {"Scope1.wait", "wait(timeoutSeconds = 5) : boolean", "Optional timeout in seconds", "s", "if (!Scope.wait(5)) throw new Error(Scope.lastError);", "Checks acquisition completion. Do not assume data is ready before the wait succeeds."},
    {"Scope1.Time.Rate.value", "Rate.value : number", "Requested acquisition rate", "samples/s", "Scope.Time.Rate.value = 1000000;", "Actual supported rate depends on AD3 configuration."},
    {"Scope1.Time.Samples.value", "Samples.value : number", "Requested acquisition buffer size", "samples", "Scope.Time.Samples.value = 16384;", "For larger captures, use Scope Record mode if supported."},
    {"Scope1.Channel1.Range.value", "Range.value : number", "Requested oscilloscope range in peak-to-peak volts", "Vpp", "Scope1.Channel1.Range.value = 10;", "Existing WaveForms-style property. Hardware may round or map requested Vpp to an available gain/range. Call Scope1.config() or single() before checking Range.real."},
    {"Scope1.Channel1.Range.real", "Range.real : number", "Read-only effective range, following successful config/single", "Vpp", "Scope1.Channel1.Range.value = 10;\nif (!Scope1.config()) throw Error(Scope1.lastError);\nprint(Scope1.Channel1.Range.real);", "Reads FDwfAnalogInChannelRangeGet for actual hardware configuration. In simulation the readback equals the requested range. Do not interpret as an extra physical gain stage."},
    {"Scope1.Channel1.Range.steps", "Range.steps : number[]", "Read-only SDK-advertised selectable ranges", "Vpp", "var ranges = Scope1.Channel1.Range.steps;\nprint(ranges);", "Read from FDwfAnalogInChannelRangeSteps when the device is opened. [] means no reported values, including in simulation; see Scope1.capabilities().rangeStepsAvailable. These are selectable settings, not separate hardware amplifier gains."},
    {"Scope1.capabilities", "capabilities() : object", "No arguments", "Vpp and other units", "var c=Scope1.capabilities();\nprint(c.rangeStepsAvailable, c.rangeSteps, c.rangeMinVpp, c.rangeMaxVpp);", "Includes device-advertised rangeSteps, rangeStepsAvailable, rangeMinVpp, rangeMaxVpp and rangeStepCount where the SDK exposes them. Range discovery is non-mutating and optional."},
    {"Scope1.Channel1.measure", "measure(name [, startSeconds, endSeconds]) : number", "Measurement name, optional start/end seconds", "Depends on measurement", "var rms = Scope.Channel1.measure(\"RMS\");", "Computes from the last completed capture. Ctrl+Space after the opening quote suggests measurement names."},
    {"Scope1.Channel1.measureWindow", "measureWindow(name, startSeconds, endSeconds) : number", "Measurement name, start/end times (seconds)", "Depends on measurement", "var rms = Scope.Channel1.measureWindow(\"RMS\", 0, 0.05);", "Time bounds must be ordered and fit the capture."},
    {"Scope1.Channel1.measureCapture", "measureCapture(name [, startSeconds, endSeconds]) : number", "Measurement name, optional time bounds", "Depends on measurement", "var hz = Scope.Channel1.measureCapture(\"Frequency\");", "Uses captured sample data, not a live hardware query."},
    {"Scope1.Channel1.spectrum", "spectrum(fftSize, window) : object", "Power-of-two FFT size, window: Hann, Rectangular, Blackman", "Hz, V", "var fft = Scope.Channel1.spectrum(16384, \"Hann\");\nprint(fft.peakFrequencyHz);", "Returns numeric spectrum data. Frequency and amplitude depend on window and sampling conditions."},
    {"Scope1.Channel1.statistics", "statistics([startSeconds, endSeconds]) : object", "Optional start and end times", "mixed", "var stats = Scope.Channel1.statistics();\nprint(stats.rms, stats.min, stats.max);", "Computes measurements in native C++ from the last capture."},
    {"Scope1.captureInfo", "captureInfo() : object", "No arguments", "", "var info = Scope.captureInfo();\nprint(info.sampleCount, info.sampleRateHz);", "Host-after-transfer timestamp differs from the DWF trigger timestamp."},
    {"Scope1.saveReference", "saveReference(channelNumber) : boolean", "Channel index 1 or 2", "", "Scope.saveReference(1);", "Copies the last completed channel buffer; reference persists across captures."},
    {"Scope1.mathData", "mathData(operation) : array", "CH1-CH2, CH1+CH2, CH1*CH2, CH1-REF1, CH2-REF2", "Varies", "var diff = Scope.mathData(\"CH1-REF1\");", "Requires matching sample counts; reference math also requires matching sampling rates."},
    {"Scope1.Time.Mode.text", "Mode.text : string", "Repeated, Shift, Record, or hardware-supported mode", "", "Scope.Time.Mode.text = \"Record\";", "Some modes are unavailable on certain hardware; check Scope.capabilities()."},
    {"Wavegen1.Channel1.Modulation.FM.enable", "FM.enable : boolean", "true or false", "", "Wavegen.Channel1.Modulation.FM.enable = false;", "Enabling FM changes generator output; check supported hardware modes."},
    {"Wavegen1.play", "play(channel, timeoutSeconds) : boolean", "Channel 1/2, timeout seconds", "s", "if (!Wavegen.play(1, 2)) throw new Error(Wavegen.lastError);", "For finite streaming playback after filling the Play buffer."},
    {"include", "include(path) : void", "Safe relative .js library name", "", "include(\"TextTools.js\");", "Runs a classic script once per engine in global scope. Use modules.load() for native .mjs exports."},
    {"modules.load", "load(path) : module", "Safe relative .mjs library name", "", "var math = modules.load(\"SharedMath.mjs\");", "Uses Qt native ECMAScript modules; same engine, with cached exports. Embedded panel libraries take priority over shared files."},
    {"modules.userFolder", "userFolder() : string", "No arguments", "", "print(modules.userFolder());", "Shows the writable per-user library folder. Manage modules via Script > Manage Script Libraries."},
    {"print", "print(value, ...) : void", "One or more output values", "", "print(\"Voltage:\", Scope.Channel1.measure(\"RMS\"));", "Prints to the Script output console."},
    {"wait", "wait(seconds)", "Delay in seconds", "s", "wait(0.100);", "This is the script delay helper, not Scope.wait()."},
    {"TestRunner.configure", "configure(options) : boolean", "{title?,sequenceId?,tableId?,logId?,summaryId?,stopOnFailure?,totalTimeoutMs?}", "ms", "TestRunner.configure({sequenceId:'stationSequence',totalTimeoutMs:5000,stopOnFailure:true});", "Native C++ executive. totalTimeoutMs is a monotonic total-run deadline in milliseconds; 0 disables it. UI targets are optional. Timed execution uses one worker-thread Qt event loop, not a blocking wait."},
    {"TestRunner.loadDefinition", "loadDefinition(document, libraryExports) : boolean", "{schemaVersion:1,id,title,config,steps[]} plus imported .mjs exports", "", "const seq=modules.load('TestStationSequence.mjs');\nconst impl=modules.load('TestStationSteps.mjs');\nif(!TestRunner.loadDefinition(seq.definition,impl)) throw Error(TestRunner.lastError);", "Manual-mode only for projects without a project-owned definition. Project-managed tests load automatically; do not call from operator script. Transactional C++ loader validates every step and resolves its handler/handlers names against callable exports. Replaces current steps/config only when all entries pass. Manual TestRunner.add() remains supported. Setup/Main/Cleanup ordering remains native; no JS eval or second engine."},
    {"TestRunner.loadDefinitionFile", "loadDefinitionFile(filePath, libraryExports) : boolean", "Existing file ending .wftest.json (max 128 KiB), imported .mjs module", "", "const impl=modules.load('TestStationSteps.mjs');\nTestRunner.loadDefinitionFile('C:/Tests/ControllerFunctionalTest.wftest.json',impl);", "Read and validate a versioned standalone JSON sequence file. Only the data file is loaded from disk; functions must already be exported by a separately loaded module. No file writes and no test execution occur on import. Invalid files preserve the previous executable sequence."},
    {"TestRunner.saveDefinition", "saveDefinition(filePath) : boolean", "Existing parent folder, destination ending .wftest.json", "UTF-8 JSON", "TestRunner.saveDefinition('C:/Tests/ControllerCopy.wftest.json');", "Explicitly export the most recently imported declarative definition atomically using QSaveFile. Rejects active executions, unsupported filename extensions and sequences assembled with manual add() calls. Saves configuration and named handlers, never source code or runtime results."},
    {"TestRunner.definition", "definition() : object", "No arguments", "Versioned JSON-like sequence", "const doc=TestRunner.definition();\nprint(doc.id,doc.steps.length);", "Read-only snapshot of the imported data-only definition; empty for sequences assembled exclusively with TestRunner.add(). Does not contain callable functions. Changes to runtime options through configure() do not rewrite the source definition."},
    {"TestRunner.add", "add(definition, callbackOrHandlers?) : boolean", "{id,name,group?,type?,description?,mode?,intervalMs?,timeoutMs?,durationMs?}, function OR {start?,poll?,finish?}", "ms", "TestRunner.add({id:'settle',name:'Settle',mode:'delay',durationMs:150,timeoutMs:500},null);", "Manual-mode only when no native project testDefinition is loaded. For normal project tests use the Visual Editor. v5.6.59: group=setup/main/cleanup (default main); C++ always executes Cleanup after failed/error Main tests. type=test (default, successful PASS) or action (successful DONE). Modes: once calls the function once; delay waits durationMs without calling JS; poll calls {poll} on a best-effort Qt timer (intervalMs >= 10) until it returns true (PASS) or false (FAIL). Poll return undefined/null = pending. {start} runs once; {finish}(status,elapsedMs) runs at termination, including normal abort/reset when the engine is available. Callbacks may come directly from modules.load('...mjs') exports or included .js global functions. Synchronous hardware/JS calls cannot be force-preempted by a Qt timer."},
    {"TestRunner.onFinished", "onFinished(callback) : boolean", "function(summary) or null", "", "TestRunner.onFinished(function(r){print(r.state,r.elapsedMs+'ms');});", "Native completion notification after the last step, failure, timeout, or abort. The callback runs on the Panel worker thread and may read summary()/results(), but must not start another run synchronously. Needed because timed run() returns before the final result."},
    {"TestRunner.run", "run() : boolean", "No arguments", "", "if(!TestRunner.run()) print(TestRunner.lastError);", "Starts/continues native scheduling. For all-once sequences it retains synchronous behavior; for delay/poll steps true means accepted/running, NOT final PASS. Inspect state() later or register onFinished(). No promise support and no nested event loop."},
    {"TestRunner.next", "next() : boolean", "No arguments", "", "TestRunner.next();", "Begins one step only. A poll/delay step remains RUNNING and completes on a Qt timer; next() is rejected while a step is active. Subsequent steps require another next() or run()."},
    {"TestRunner.measure", "measure(name,value,low,high,unit?,resultWidget?) : boolean", "Measurement name, finite value and inclusive bounds", "Depends on unit", "return TestRunner.measure('V+',3.301,3.25,3.35,'V','resultPositive');", "Native inclusive limit evaluation, bounded results and automatic updates to table, result card and event log. Only callable during an executing step callback; do not call repeatedly during every pending poll unless intentional."},
    {"TestRunner.reset", "reset() : boolean", "No arguments", "", "TestRunner.reset();", "Clears results and restores PENDING steps. Can cancel a pending timed step; its finish handler is called with aborted before results are cleared. Cannot reset from inside an active callback."},
    {"TestRunner.clear", "clear() : boolean", "No arguments", "", "TestRunner.clear();", "Discard registered steps as well as results, only while idle/not in a session."},
    {"TestRunner.abort", "abort() : boolean", "No arguments", "", "TestRunner.abort();", "Emergency abort between scheduled callbacks. The active step's finish handler runs when safe, but the sequence Cleanup group is skipped. Prefer terminate() for normal operator stops. Native Panel Stop interrupts the engine and cannot forcibly cancel a blocking driver call."},
    {"TestRunner.terminate", "terminate() : boolean", "No arguments", "", "TestRunner.terminate();", "Cooperative stop: after the active callback returns, skip remaining Main steps and run the Cleanup group; the overall state is aborted. This is the preferred operator Stop for hardware test stations. Different from abort(), which stops immediately and skips sequence-level Cleanup."},
    {"TestRunner.setRunInfo", "setRunInfo(details) : boolean", "Legacy/dynamic tests: up to 16 primitive fields. Project-defined Run Information uses the native UUT widget instead.", "", "TestRunner.setRunInfo({uutId:'SIM-UUT-001',stationId:'Demo',operatorId:'DEMO'});", "C++ owns a bounded snapshot of this run's UUT/operator/station metadata. Values must be primitive strings, booleans or finite numbers. Fields are included in summary().runInfo and report().runInfo, not persisted to .wfpanel. Cannot change during execution. reset()/clear() remove previous run metadata; start a new run with setRunInfo() again."},
    {"TestRunner.runInfo", "runInfo() : object", "No arguments", "Run metadata", "print(TestRunner.runInfo().uutId);", "Frozen native C++ snapshot of current or last execution; not changed when editing next UUT."},
    {"TestRunner.pendingRunInfo", "pendingRunInfo() : object", "No arguments", "Pending UUT inputs", "const pending=TestRunner.pendingRunInfo(); print(pending.uutId);", "Live pending operator values from the one project-owned model shared by all Run Information widgets. Includes hidden Pages; no active widget. Numeric entries remain text until validated at run start. Does not overwrite report().runInfo for an already completed execution."},
    {"TestRunner.setVariable", "setVariable(name,value) : boolean", "Name string and primitive value", "", "TestRunner.setVariable('lastVoltage',3.301);", "Share a bounded primitive value between imported library step callbacks within the active C++ execution. Only valid inside a running step handler. Values are cleared on each run and reset; no JavaScript objects, functions or pointers are stored."},
    {"TestRunner.variable", "variable(name) : value", "Shared variable name", "", "const value=TestRunner.variable('lastVoltage');", "Returns the native value recorded by setVariable() or undefined/invalid if absent; use for cross-step communication without module globals."},
    {"TestRunner.variables", "variables() : object", "No arguments", "Shared variable snapshot", "print(JSON.stringify(TestRunner.variables()));", "Returns all named C++-owned values shared between test steps in this execution. Included in report().variables."},
    {"TestRunner.saveReport", "saveReport(filePath, format='json') : boolean", "Absolute or relative existing folder and .json/.csv file name; format json or csv", "UTF-8", "if (!TestRunner.saveReport('C:/TestReports/UUT-001.json')) print(TestRunner.lastError);", "Explicit export only after a completed run (PASS/FAIL/ERROR/ABORTED). JSON preserves the entire structured native report. CSV writes one STEP row per step and one MEASUREMENT row per measurement with linked stepId. Uses QSaveFile atomic replacement; parent directory must already exist. Does not run automatically on Panel load."},
    {"TestRunner.exportReport", "exportReport(format='json') : string", "Optional 'json' or 'csv'", "Path", "const path=TestRunner.exportReport('json'); if(path) print('Saved:',path); else print(TestRunner.lastError);", "Creates Documents/WFScript Reports when explicitly invoked for a terminal run. File name contains a sanitized UUT ID plus the unique execution ID; returns saved path or an empty string on error. JSON is the complete record; CSV is a tabular step/measurement export. Never writes on Panel load or while a test is still RUNNING."},
    {"TestRunner.archiveInfo", "archiveInfo() : object", "No arguments; native automatic report storage", "Status / path", "const a=TestRunner.archiveInfo(); print(a.status,a.path,a.error);", "Read-only status of the most recent automatic archive: idle, pending, disabled, saved or error. The project Test Sequence Editor > Report Storage owns auto-save and folder selection. A report is archived after all Cleanup finishes, even for failed/error/aborted executions. File failures do not change the test verdict. The onFinished handler can read archiveInfo(). No archive is written on Panel startup."},
    {"TestRunner.report", "report() : object", "No arguments", "JSON-compatible object", "const r=TestRunner.report(); print(JSON.stringify(r));", "C++ owned run record: schemaVersion, executionId, title, state, startedAtUtc, finishedAtUtc, elapsedMs, summary, steps[] (group,type,status,timing), measurements[] (stepId, value, limits, status), runInfo, and per-run variables. Automatic archiving is configured in Test Sequence > Report Storage; use archiveInfo() to get the result path. Manual saveReport() and exportReport() remain available. Also includes a definition SHA-256 and execution mode when available."},
    {"TestRunner.summary", "summary() : object", "No arguments", "counts and ms", "const s=TestRunner.summary();print(s.state,s.elapsedMs,s.stepElapsedMs,s.currentStepId);", "Native snapshot includes total and active step elapsed ms, pollCount, currentStepId, totalTimeoutMs, completion counts, failure counts and lastError. Timed steps remain RUNNING between callbacks."},
    {"TestRunner.results", "results() : array", "No arguments", "Varies", "const rows=TestRunner.results();", "Native bounded measurement records {name,value,low,high,unit,stepId,status}; updates the existing Measurement Table. Transient runtime state, not persisted to .wfpanel."},
    {"TestRunner.state", "state() : string", "No arguments", "", "print(TestRunner.state());", "idle/running/passed/failed/error/aborted. For timed tests, wait for onFinished or inspect the status after completion rather than reading it immediately after run()."},
    {"TestRunner.lastError", "lastError : string", "Read-only", "", "print(TestRunner.lastError);", "Native validation errors, timeout messages and JavaScript handler exceptions; original module filename/line retained in diagnostics."},
    {"ui.set", "set(widgetId, value) : boolean", "Panel widget ID and runtime value", "", "ui.set(\"resultPositive\",3.301);\nui.set(\"stationSequence\",[{id:\"power\",name:\"Check V+\",status:\"pending\"}]);\nui.set(\"stationSequence\",{update:{id:\"power\",status:\"pass\",elapsedMs:125,message:\"3.301 V\"}});\nui.set(\"stationLog\",{append:{level:\"PASS\",message:\"Power verified\",source:\"Station\"}});", "Measurement Result: numeric ui.set evaluates configured min/max inclusive, {reset:true} clears. Measurement Table: array of {name,value,unit,low,high}, or {reset:true}. Event Log: {append:{level,message,source?,timestamp?}}, {append:[entries...]}, full array to replace, or {clear:true}. Levels: DEBUG,INFO,PASS,WARN,FAIL,ERROR. GUI filter/Copy/Clear/Export CSV; timestamps default to local time; maxEntries bounds history, live entries not saved. Test Sequence: array of unique {id,name,description?,status?,message?,elapsedMs?} (maxSteps property), {update:{id,status?,message?,elapsedMs?,name?,description?}}, {reset:true} (retain steps, reset states), or {clear:true}. States: pending,running,pass,done,fail,error,skipped,aborted. The Test Sequence is a VIEW; the native TestRunner (if used) owns scheduling, statuses, deadlines and results. Direct ui.set() can still update a standalone sequence widget without TestRunner. Invalid updates are ignored atomically and logged to Qt warnings. ui.set confirms a known widget ID, not per-widget payload validation. Conversion from JS objects to native Qt values occurs on the script thread. Hidden Pages are updated; runtime values are NOT persisted to .wfpanel. See Examples > Getting Started > Automated Test Station."},
    {"ui.get", "get(widgetId) : value", "Panel widget ID", "", "var setting = ui.get(\"speed\");", "Returns standard JavaScript primitives, arrays and plain objects (including true nested Array rows for Experiment Data Tables). No Qt QVariantList conversion is necessary: Array.isArray(ui.get(\"recipeTable\")) is true. A returned array/object is a snapshot; call ui.set() to commit changes. Reads the most recently assigned value from the persistent Panel runtime, even when the widget is on a hidden Page. Test Sequence initially returns its configured pending steps; subsequent ui.get reads the last payload sent to ui.set, not a merged or evaluated full sequence snapshot. Measurement Result starts as null; Measurement Table and Event Log start as empty arrays. ui.get(eventLogId) returns the last ui.set payload, not the complete accumulated log or any rows cleared from the GUI. For complete examples, open Examples > Getting Started > Automated Test Station."},
    {"ui.on", "on(widgetName, eventName, callback)", "Panel widget, event, callback", "", "ui.on(\"start\", \"click\", function() { print(\"Start\"); });", "Panel scripting API; only available with panel scripts."},
    {"plots.setSeries", "setSeries(id, name, xArray, yArray [, options]) : boolean", "Plot ID, name, matching numeric X/Y arrays, optional {color:'#RRGGBB'}", "User coordinates", "plots.setSeries(\"plot1\", \"Speed\", [0,1,2], [120,150,180]);", "Replaces only the named series on this plot. Other plot instances are not modified."},
    {"plots.setSamples", "setSamples(id, name, samples, rateHz [, startSeconds, options]) : boolean", "Plot ID, name, Y samples, sample rate (Hz), start time", "s", "plots.setSamples(\"plot1\", \"CH1\", [0,.5,0], 1000, 0);", "Creates uniform time-axis X data without manually constructing an X array."},
    {"plots.fromScope", "fromScope(id [, channel=1, name='CH1']) : boolean", "Destination plot ID, Scope1 channel number 1/2, optional series name", "s, V", "if (!Scope1.wait(3)) throw Error(Scope1.lastError);\nplots.fromScope(\"plot1\",1,\"CH1\");", "No implicit auto-delivery to Panel plots; call this after Scope1.wait(). Scope workspace continues to update automatically."},
    {"plots.append", "append(id, name, xArray, yArray [, options]) : boolean", "Plot ID, series, X/Y batches", "User coordinates", "plots.append(\"trend\",\"RMS\",[1,2],[0.7,0.8]);", "Append bounded history, retaining newest 250000 points per series."},
    {"plots.appendPoint", "appendPoint(id, name, x, y) : boolean", "Plot ID, series, finite numeric X and Y", "User coordinates", "plots.appendPoint(\"trend\",\"RMS\",100,0.707);", "For occasional measurements; batch high-rate data with plots.append()."},
    {"plots.configure", "configure(id, options) : boolean", "Plot ID, chartType, labels, xAuto/yAuto, xMin/xMax, yMin/yMax, grid, legend, lineWidth, xLog/yLog, showCursors, cursorA/cursorB", "User coordinates", "plots.configure(\"plot1\",{chartType:\"XY\",xLabel:\"Angle\",yLabel:\"Torque\",yAuto:false,yMin:-10,yMax:10});", "Runtime settings are not saved to .wfpanel; edit the Properties inspector for saved defaults."},
    {"plots.styleSeries", "styleSeries(plotId, name, options) : boolean", "Options {color, width, markerSize, visible, axis: left or right}", "", "plots.styleSeries(\"plot1\",\"CH1\",{color:\"#49d2a1\",width:2.5});", "Changes an existing series without replacing its data."},
    {"plots.removeSeries", "removeSeries(id, name) : boolean", "Plot ID, existing series name", "", "plots.removeSeries(\"plot1\",\"CH1\");", "Removes only this plot's named series; the plot QWidget and other series survive."},
    {"plots.clear", "clear(id) : boolean", "Destination plot ID", "", "plots.clear(\"plot1\");", "Clears runtime chart data, not designer properties, the Page, or other plots."},

};
const Detail* detailFor(const QString& path) {
    for(const auto& d : kDetails)
        if(path==QLatin1String(d.path)) return &d;
    return nullptr;
}
QString esc(const QString& s) { return s.toHtmlEscaped(); }
}
QString canonical(QString p) {
    if(p==QStringLiteral("Supplies1") || p.startsWith(QStringLiteral("Supplies1.")))
        p.replace(0,9,QStringLiteral("Supplies"));
    for (const auto& base : {QStringLiteral("Wavegen"),QStringLiteral("Scope"),QStringLiteral("Logic"),QStringLiteral("Patterns")}) {
        if(p==base || p.startsWith(base+QLatin1Char('.'))) {
            p.replace(0,base.size(),base+QStringLiteral("1"));break;
        }
    }
    p.replace(QStringLiteral(".Channel2"),QStringLiteral(".Channel1"));
    return p;
}
bool isDocumentedSymbol(const QString& fullPath) {
    const QString p=canonical(fullPath);
    if(p.isEmpty()) return false;
    const int i=p.lastIndexOf(QLatin1Char('.'));
    const QString parent=i<0 ? QString() : p.left(i);
    const QString leaf=i<0 ? p : p.mid(i+1);
    for(const auto& entry:CompletionCatalog::entries(parent))
        if(entry.name==leaf) return true;
    return false;
}
Documentation lookup(const QString& fullPath) {
    const QString p=canonical(fullPath);
    Documentation out;out.path=p;
    if(!isDocumentedSymbol(p)) return out;
    const int i=p.lastIndexOf(QLatin1Char('.'));
    const QString parent=i<0 ? QString() : p.left(i);
    out.symbol=i<0 ? p : p.mid(i+1);
    for (const auto& entry : CompletionCatalog::entries(parent)) {
        if(entry.name==out.symbol) { out.summary=entry.description;break; }
    }
    out.known=true;
    if(const auto* d=detailFor(p)) {
        out.signature=QString::fromUtf8(d->signature);
        out.parameters=QString::fromUtf8(d->parameters);
        out.unit=QString::fromUtf8(d->unit);
        out.example=QString::fromUtf8(d->example);
        out.notes=QString::fromUtf8(d->notes);
    }
    return out;
}
QString html(const Documentation& d,bool dark) {
    const QString ink=dark?QStringLiteral("#edf4fc"):QStringLiteral("#17222f");
    const QString sub=dark?QStringLiteral("#bbcde0"):QStringLiteral("#40546a");
    const QString code=dark?QStringLiteral("#122132"):QStringLiteral("#edf3f9");
    QString s=QStringLiteral("<html><body style='color:%1;font-size:10pt'>").arg(ink);
    if(!d.known) return s+QStringLiteral("<b>No documented WFScript API symbol at this location.</b></body></html>");
    s+=QStringLiteral("<h3 style='margin:0 0 5px 0'>%1</h3>").arg(esc(d.path));
    if(!d.signature.isEmpty()) s+=QStringLiteral("<p><b>%1</b></p>").arg(esc(d.signature));
    s+=QStringLiteral("<p>%1</p>").arg(esc(d.summary));
    if(!d.parameters.isEmpty()) s+=QStringLiteral("<p><b>Arguments:</b> %1</p>").arg(esc(d.parameters));
    if(!d.unit.isEmpty()) s+=QStringLiteral("<p><b>Units:</b> %1</p>").arg(esc(d.unit));
    if(!d.example.isEmpty()) s+=QStringLiteral("<p style='color:%1'><b>Example</b></p><pre style='background:%2;padding:7px'>%3</pre>").arg(sub,code,esc(d.example));
    if(!d.notes.isEmpty()) s+=QStringLiteral("<p style='color:%1'>%2</p>").arg(sub,esc(d.notes));
    s+=QStringLiteral("<p style='color:%1;font-size:9pt'>WFScript supported API catalog; validate device capabilities at runtime.</p>").arg(sub);
    return s+QStringLiteral("</body></html>");
}
QString guideHtml(const QString& topic) {
    const QString intro=QStringLiteral("<html><body style='font-size:10pt;color:#203247'>"
        "<h3 style='margin-top:0'>WFScript Test Executive — %1</h3>%2"
        "<p style='color:#536578'><i>Reference: v5.6.63 project-owned C++ TestRunner. "
        "These pages explain concepts; use the TestRunner method nodes for exact signatures.</i></p></body></html>");
    QString content;
    if(topic==QStringLiteral("overview")) {
        content=QStringLiteral(
            "<p><b>There is one authoritative test configuration.</b> The Visual Test Sequence Editor "
            "stores its data in the .wfpanel project under testDefinition. The C++ runtime loads the "
            "definition and binds functions from the selected .mjs library BEFORE the operator script starts.</p>"
            "<p><b>To create a test:</b> Panel Designer &gt; Test Sequence... &gt; edit Setup, Main, Cleanup, "
            "totalTimeoutMs, timing, numeric limits and handler names. Save the .wfpanel. "
            "In the selected embedded JavaScript library, implement the named exported functions. "
            "For a numeric test return a number; C++ evaluates the limits.</p>"
            "<p><b>Operator code is just commands:</b></p>"
            "<pre>ui.on('runButton','clicked',function(){ TestRunner.run(); });\n"
            "ui.on('stopButton','clicked',function(){ TestRunner.terminate(); });\n"
            "TestRunner.onFinished(function(){ print(TestRunner.report().state); });</pre>"
            "<p>Do NOT call configure(), add(), or loadDefinition() from the operator script. "
            "They are disabled for project-managed sequences. The C++ executive owns the saved settings.</p>");
    } else if(topic==QStringLiteral("definitions")) {
        content=QStringLiteral(
            "<p>The .wfpanel project owns testDefinition (schemaVersion 1, id, title, config and steps) "
            "and testLibrary (name of one embedded .mjs library). The Visual Editor edits the project data "
            "directly, not JavaScript source. Save, reopen and run: the same document is used.</p>"
            "<p>A step has a group (setup/main/cleanup), execution mode (once/poll/delay), type "
            "(action/test/numeric), timeout and a named exported function. A numeric step additionally "
            "has limits:{low,high}, unit and optional resultWidget.</p>"
            "<pre>// In TestSteps.mjs:\nexport function readSupplyVoltage() { return 3.301; }\n"
            "// In the Visual Editor: Numeric step, handler=readSupplyVoltage,\n"
            "// low=3.25, high=3.35, unit=V. C++ determines PASS/FAIL.</pre>"
            "<p>The executive automatically validates names and connects the handlers when Panel mode starts. "
            "Missing exports are startup errors. An external .wftest.json export is useful for source control, "
            "but is not a second runtime configuration. definition() reads the saved test.</p>"
            "<p>Legacy manual add()/loadDefinition() APIs remain available only when there is no project-owned "
            "test definition. Do not use them for new Test Executive Panels.</p>");
    } else if(topic==QStringLiteral("groups")) {
        content=QStringLiteral(
            "<p><b>Setup:</b> initialize instruments, fixtures and the unit under test. "
            "<b>Main:</b> acquire/evaluate measurements. <b>Cleanup:</b> restore outputs, release resources.</p>"
            "<p>The executive orders groups Setup → Main → Cleanup regardless of registration order. "
            "When stopOnFailure is true, remaining Main tests become SKIPPED, then Cleanup executes. "
            "Runtime ERROR and total/step timeouts also lead to Cleanup. Cleanup is cooperative, not "
            "a guarantee when an SDK call blocks or the process is terminated.</p>"
            "<p><b>Step type:</b> action completes as DONE (or ERROR); test completes as PASS/FAIL. "
            "For numeric steps the function returns a number; C++ applies limits and records the reading. A failed DUT limit is FAIL; an exception or timed-out test procedure is ERROR.</p>");
    } else if(topic==QStringLiteral("modes")) {
        content=QStringLiteral(
            "<p><b>once:</b> call one handler synchronously and evaluate its boolean result. "
            "<b>delay:</b> nonblocking Qt-timed wait using durationMs; no JS handler is required. "
            "<b>poll:</b> optional start(), repeated poll(stepElapsedMs,totalElapsedMs), optional "
            "finish(status,elapsedMs). Return undefined/null to continue, true to PASS/DONE, false to FAIL.</p>"
            "<pre>TestRunner.add({id:'settle',name:'Settle',group:'setup',type:'action',\n"
            "    mode:'delay',durationMs:150,timeoutMs:600},null);\n"
            "TestRunner.add({id:'clock',name:'Clock',mode:'poll',\n"
            "    intervalMs:10,timeoutMs:800},\n"
            "    {start:steps.beginClock,poll:steps.pollClock,finish:steps.endClock});</pre>"
            "<p>Polling intervals of 10 ms are best effort, not real time; the timer cannot preempt "
            "a synchronous JavaScript/SDK call. Step timeoutMs and totalTimeoutMs are deadlines checked "
            "when the worker returns control to Qt.</p>");
    } else if(topic==QStringLiteral("lifecycle")) {
        content=QStringLiteral(
            "<p><b>run():</b> start or resume execution; for timed steps a true return means "
            "accepted, not final PASS. <b>next():</b> execute one step; a timed step may remain RUNNING. "
            "Use onFinished(callback) or state()/summary() for terminal outcomes.</p>"
            "<p><b>terminate():</b> graceful operator stop, remaining Main steps skipped, Cleanup runs, "
            "overall ABORTED. <b>abort():</b> emergency stop, sequence-level Cleanup skipped. "
            "<b>reset():</b> reset step/results; <b>clear():</b> discard definitions.</p>"
            "<p>The status vocabulary is idle, running, passed, failed, error, aborted for the overall "
            "execution; individual steps use pending, running, pass, done, fail, error, skipped, aborted.</p>"
            "<p><b>Safety:</b> forced process stop, interrupted QJSEngine or blocked hardware driver can "
            "prevent script Cleanup. Design electrical interlocks and safe hardware defaults separately.</p>");
    } else if(topic==QStringLiteral("context")) {
        content=QStringLiteral(
            "<p>setRunInfo({uutId,stationId,operatorId,...}) captures primitive metadata before each "
            "execution. setVariable(name,value) and variable(name) exchange bounded primitive values "
            "between library steps while executing. report() includes runInfo and variables.</p>"
            "<pre>TestRunner.setRunInfo({uutId:'UUT-001',operatorId:'operator'});\n"
            "// Inside a running step handler:\n"
            "TestRunner.setVariable('voltage',3.301);\n"
            "// Inside another step handler:\n"
            "const voltage=TestRunner.variable('voltage');</pre>"
            "<p>Metadata is not saved in a .wfpanel and run variables are cleared when starting "
            "another execution. Keep measurements attached to their originating step with measure().</p>");
    } else if(topic==QStringLiteral("reports")) {
        content=QStringLiteral(
            "<p>TestRunner.report() provides a C++ snapshot: executionId, state, UTC timestamps, "
            "elapsedMs, runInfo, variables, step records and measurement rows. An out-of-limits "
            "measurement is stored with FAIL and the step ID. The native report remains "
            "in memory; when automatic archiving is enabled in Test Sequence > Report Storage, "
            "it is saved as JSON after Cleanup at terminal state.</p>"
            "<pre>TestRunner.onFinished(function(info){\n"
            "    const report=TestRunner.report();\n"
            "    print(report.executionId,info.state);\n"
            "    const archive=TestRunner.archiveInfo();\n"
            "    print(archive.status,archive.path,archive.error);\n"
            "});\n"
            "// After the run has finished:\n"
            "const path=TestRunner.exportReport('json');\n"
            "// Or choose an existing folder yourself:\n"
            "TestRunner.saveReport('C:/TestReports/UUT-001.csv','csv');</pre>"
            "<p>exportReport() writes to Documents/WFScript Reports with a UUT/UUID filename. "
            "JSON contains the complete native record. CSV contains STEP and MEASUREMENT rows, "
            "with stepId linking each measurement to its step. Files are written atomically. "
            "Export rejects idle/running tests.</p>");
    } else if(topic==QStringLiteral("libraries")) {
        content=QStringLiteral(
            "<p>Use the existing Script Libraries manager to store reusable .mjs files, or embed "
            "a module inside a .wfpanel. modules.load('TestStationSteps.mjs') returns module "
            "exports from the same QJSEngine that owns TestRunner; imported callbacks may call "
            "TestRunner.measure(), setVariable() and other normal script APIs.</p>"
            "<p>Keep test functions short and nonblocking. The executive schedules the callbacks; "
            "your library implements the hardware measurement and returns status. "
            "See Examples → Getting Started → Automated Test Station.</p>");
    }
    if(content.isEmpty())return {};
    const QHash<QString,QString> titles={
        {QStringLiteral("overview"),QStringLiteral("Getting Started")},
        {QStringLiteral("definitions"),QStringLiteral("Versioned Sequence Definitions")},
        {QStringLiteral("groups"),QStringLiteral("Setup, Main, Cleanup & Step Types")},
        {QStringLiteral("modes"),QStringLiteral("Execution Modes & Timers")},
        {QStringLiteral("lifecycle"),QStringLiteral("Execution Lifecycle & Safety")},
        {QStringLiteral("context"),QStringLiteral("Run Context & UUT Metadata")},
        {QStringLiteral("reports"),QStringLiteral("Results & Report Export")},
        {QStringLiteral("libraries"),QStringLiteral("Reusable Script Libraries")}};
    return intro.arg(titles.value(topic,topic),content);
}
QStringList curatedPaths() {
    QStringList paths;
    for(const auto& d:kDetails) paths.append(QString::fromLatin1(d.path));
    return paths;
}
QVector<Snippet> snippets() {
    return {
        {QStringLiteral("wgsine"),QStringLiteral("Configure and start a sine generator"),QStringLiteral(
            "Wavegen.Channel1.enable = true;\n"
            "Wavegen.Channel1.Mode.text = \"Simple\";\n"
            "Wavegen.Channel1.Simple.Type.text = \"Sine\";\n"
            "Wavegen.Channel1.Simple.Frequency.value = ${1:1000};\n"
            "Wavegen.Channel1.Simple.Amplitude.value = ${2:1.0};\n"
            "Wavegen.Channel1.Simple.Offset.value = 0;\n"
            "if (!Wavegen.run()) throw new Error(Wavegen.lastError);\n")},
        {QStringLiteral("scopecapture"),QStringLiteral("Single triggered acquisition and RMS"),QStringLiteral(
            "Scope.Time.Rate.value = ${1:1000000};\n"
            "Scope.Time.Samples.value = ${2:16384};\n"
            "if (!Scope.single()) throw new Error(Scope.lastError);\n"
            "if (!Scope.wait(5)) throw new Error(Scope.lastError);\n"
            "var rms = Scope.Channel1.measure(\"RMS\");\n"
            "print(\"CH1 RMS:\", rms);\n")},
        {QStringLiteral("scopefft"),QStringLiteral("Run a native FFT on acquired Scope data"),QStringLiteral(
            "var fft = Scope.Channel1.spectrum(${1:16384}, \"${2:Hann}\");\n"
            "print(\"FFT peak frequency (Hz):\", fft.peakFrequencyHz);\n")},
        {QStringLiteral("scoperef"),QStringLiteral("Save a baseline and subtract a reference"),QStringLiteral(
            "Scope.saveReference(1);\n"
            "// After acquiring a second frame:\n"
            "var difference = Scope.mathData(\"CH1-REF1\");\n"
            "print(\"Reference difference samples:\", difference.length);\n")},
        {QStringLiteral("scoperecord"),QStringLiteral("Record-mode scope acquisition (check capabilities)"),QStringLiteral(
            "Scope.Time.Mode.text = \"Record\";\n"
            "Scope.Time.Rate.value = ${1:200000};\n"
            "Scope.Time.RecordLength.value = ${2:0.5};\n"
            "if (!Scope.single()) throw new Error(Scope.lastError);\n"
            "if (!Scope.wait(10)) throw new Error(Scope.lastError);\n"
            "print(\"Samples:\", Scope.captureInfo().sampleCount);\n")},
        {QStringLiteral("scopewindow"),QStringLiteral("Measure RMS inside a selected time window"),QStringLiteral(
            "var rms = Scope.Channel1.measureWindow(\"RMS\", ${1:0}, ${2:0.05});\n"
            "print(\"Window RMS:\", rms);\n")},
        {QStringLiteral("eventlog"),QStringLiteral("Append timestamped messages to an Event Log widget"),QStringLiteral(
            "// Add an Event Log widget named stationLog; displays and filters entries.\n"
            "ui.set('stationLog',{append:{level:'INFO',source:'Test',message:'Starting check'}});\n"
            "ui.set('stationLog',{append:{level:'PASS',source:'Test',message:'Voltage within tolerance'}});\n"
            "// ui.set('stationLog',{clear:true}); // clear event history\n")},
                {QStringLiteral("testpoll"),QStringLiteral("Native 10ms polling step with imported library"),QStringLiteral(
            "const checks=modules.load('TestStationSteps.mjs');\n"
            "TestRunner.onFinished(function(info){ print(info.state, info.elapsedMs); });\n"
            "TestRunner.add({id:'clock',name:'Clock check',mode:'poll',intervalMs:10,timeoutMs:800},\n"
            "    {start:checks.beginClock,poll:checks.pollClock,finish:checks.endClock});\n"
            "// Start from a button event: TestRunner.run();\n")},
{QStringLiteral("testrunner"),QStringLiteral("Define and execute steps with native Qt/C++ TestRunner"),QStringLiteral(
            "// C++ owns the execution state and updates test widgets.\n"
            "if(!TestRunner.configure({sequenceId:'stationSequence',tableId:'stationTable',logId:'stationLog',stopOnFailure:true}))\n"
            "    throw Error(TestRunner.lastError);\n"
            "if(!TestRunner.add({id:'supply',name:'Verify supply',group:'main',type:'test'},function(){\n"
            "    var measured=${1:3.301}; // replace with actual hardware measurement\n"
            "    return TestRunner.measure('Supply',measured,3.25,3.35,'V','resultPositive');\n"
            "}))throw Error(TestRunner.lastError);\n"
            "// Start from an event: TestRunner.run();\n")},
        {QStringLiteral("teststeps"),QStringLiteral("Drive the Test Sequence visual widget from JavaScript"),QStringLiteral(
            "// Visual state ONLY. Define a Test Sequence widget named stationSequence.\n"
            "ui.set('stationSequence',[{id:'power',name:'Check supply',status:'pending'}]);\n"
            "ui.set('stationSequence',{update:{id:'power',status:'running',message:'Measuring...'}});\n"
            "// Run your own SDK acquisition and evaluate limits here.\n"
            "ui.set('stationSequence',{update:{id:'power',status:'pass',elapsedMs:125,message:'3.301 V'}});\n"
            "// ui.set('stationSequence',{reset:true}); // keep steps, clear result states\n")},
        {QStringLiteral("patternsequence"),QStringLiteral("Generate synchronized repeating 3-bit digital patterns"),QStringLiteral(
            "// AD3: 3.3V outputs, connect DIO0->DIO3, DIO1->DIO4, DIO2->DIO5 via 1k.\n"
            "if(!StaticIO.stop()) throw Error(StaticIO.lastError);\n"
            "if(!Patterns1.runSequence([0,1,2],[\"01010101\",\"00110011\",\"00001111\"],${1:1000})) throw Error(Patterns1.lastError);\n"
            "try {\n"
            "  print(\"Actual bit rate:\",Patterns1.status().sequenceActualBitRateHz);\n"
            "  Logic1.Time.Rate.value=32000; Logic1.Time.Samples.value=4096;\n"
            "  if(!Logic1.single() || !Logic1.wait(5)) throw Error(Logic1.lastError);\n"
            "  print(\"Digital samples:\",Logic1.samples().length);\n"
            "} finally { if(!Patterns1.stop()) print(\"STOP FAILED:\",Patterns1.lastError); }\n")},
        {QStringLiteral("encoderindex"),QStringLiteral("Generate 3-channel hardware quadrature encoder with index"),QStringLiteral(
            "// AD3: 3.3V outputs. Connect DIO0->DIO3, DIO1->DIO4, DIO2->DIO5 through 1k resistors.\n"
            "if(!StaticIO.stop()) throw Error(StaticIO.lastError);\n"
            "if(!Patterns1.runEncoder(0,1,2,${1:16},${2:600},${3:false})) throw Error(Patterns1.lastError);\n"
            "try {\n"
            "  print('Actual RPM:',Patterns1.status().encoderActualRpm);\n"
            "  // Capture DIO3 (A), DIO4 (B), DIO5 (index) with Logic1.\n"
            "  Logic1.Time.Rate.value=17000; Logic1.Time.Samples.value=4096;\n"
            "  if(!Logic1.single() || !Logic1.wait(5)) throw Error(Logic1.lastError);\n"
            "  print('Digital input samples:',Logic1.samples().length);\n"
            "} finally { if(!Patterns1.stop()) print('STOP FAILED:',Patterns1.lastError); }\n")},
        {QStringLiteral("patternclock"),QStringLiteral("Start DIO0 hardware clock/PWM and capture with Logic1"),QStringLiteral(
            "// Connect AD3 DIO0 to DIO1 through 1 kOhm resistor first.\n"
            "StaticIO.stop(); // release StaticIO drivers\n"
            "Patterns1.Channels.DIO0.Pin.value=0;\n"
            "Patterns1.Channels.DIO0.Type.text='Clock';\n"
            "Patterns1.Channels.DIO0.Output.text='PP';\n"
            "Patterns1.Channels.DIO0.Idle.text='Z';\n"
            "Patterns1.Channels.DIO0.Frequency.value=${1:1000};\n"
            "Patterns1.Channels.DIO0.Duty.value=${2:50};\n"
            "if(!Patterns1.run()) throw Error(Patterns1.lastError);\n"
            "try {\n"
            "  Logic1.Time.Rate.value=100000; Logic1.Time.Samples.value=4096;\n"
            "  if(!Logic1.single() || !Logic1.wait(3)) throw Error(Logic1.lastError);\n"
            "  print('Samples:',Logic1.samples().length);\n"
            "} finally { if(!Patterns1.stop()) print('STOP FAILED:',Patterns1.lastError); }\n")},
        {QStringLiteral("logiccapture"),QStringLiteral("Capture digital input DIO1 and report transitions"),QStringLiteral(
            "Logic1.Time.Rate.value = ${1:20000};\n"
            "Logic1.Time.Samples.value = ${2:4096};\n"
            "if (!Logic1.single()) throw Error(Logic1.lastError);\n"
            "if (!Logic1.wait(3)) throw Error(Logic1.lastError);\n"
            "var words=Logic1.samples();\n"
            "var levels=words.map(function(w){return (w >> 1) & 1;});\n"
            "print('DIO1 samples:',levels.length,'rate:',Logic1.sampleRateHz);\n")},
        {QStringLiteral("digitalio"),QStringLiteral("AD3 DIO0 high / DIO1 input / safe cleanup"),QStringLiteral(
            "// Connect DIO0 to DIO1 through 1 kOhm resistor first.\n"
            "StaticIO.Channel0.DIO0.value = 1;\n"
            "StaticIO.Channel0.DIO1.value = 2; // Z, input\n"
            "if(!StaticIO.config()) throw Error(StaticIO.lastError);\n"
            "var s=StaticIO.status();\n"
            "if(!s.valid) throw Error(s.lastError);\n"
            "print('DIO1:', (s.inputMask >> 1) & 1);\n"
            "if(!StaticIO.stop()) print('HIGH-Z NOT CONFIRMED:', StaticIO.lastError);\n")},
        {QStringLiteral("ad3supplies"),QStringLiteral("Start AD3 positive and negative DC supplies"),QStringLiteral(
            "Supplies.Output.PositiveSupply.Voltage.value = ${1:3.3};\n"
            "Supplies.Output.NegativeSupply.Voltage.value = ${2:-3.3};\n"
            "Supplies.Output.PositiveSupply.Enable.value = 1;\n"
            "Supplies.Output.NegativeSupply.Enable.value = 1;\n"
            "if (!Supplies.run()) throw Error(Supplies.lastError);\n"
            "print(Supplies.status().measuredAvailable);\n"
            "// Use Supplies.stop() when done.\n")},
        {QStringLiteral("checkdevice"),QStringLiteral("Verify a device is available"),QStringLiteral(
            "if (!Device.isConnected()) {\n"
            "    throw new Error(\"No device connected\");\n"
            "}\n")},
        {QStringLiteral("trycatch"),QStringLiteral("JavaScript error handling"),QStringLiteral(
            "try {\n"
            "    ${1:// Insert code here}\n"
            "} catch (error) {\n"
            "    print(\"Script error:\", error);\n"
            "}\n")}
    };
}
Snippet snippet(const QString& trigger) {
    for(const auto& s:snippets()) if(s.trigger==trigger) return s;
    return {};
}
QStringList referencedApiPaths(const QString& snippetBody) {
    QStringList found;
    static const QRegularExpression rx(QStringLiteral(R"(\b(?:Scope|Wavegen|Logic1|Logic|Supplies|StaticIO|Patterns1|Patterns|Device|TestRunner)(?:\.[A-Za-z_]\w*)+)"));
    auto match=rx.globalMatch(snippetBody);
    while(match.hasNext()) {
        const QString p=match.next().captured();
        if(!found.contains(p)) found.append(p);
    }
    return found;
}
}
