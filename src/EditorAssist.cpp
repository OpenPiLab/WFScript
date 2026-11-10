#include "EditorAssist.h"
#include <QRegularExpression>
#include <QStringList>
#include <QVector>

namespace EditorAssist {
namespace {
// Mark non-code positions so a brace in a string/comment is never paired or
// highlighted as a matching bracket. Conservatively treats template raw text
// as non-code. JS template interpolation is handled by the highlighter, not this
// small bracket-assist lexer.
QVector<bool> codeMask(const QString& s) {
    QVector<bool> mask(s.size(),true);
    enum Mode { Code, LineComment, BlockComment, Single, Double, Template };
    Mode mode=Code;
    for (int i=0;i<s.size();++i) {
        const QChar c=s.at(i), next=(i+1<s.size()?s.at(i+1):QChar());
        if (mode==Code) {
            if (c==QLatin1Char('/') && next==QLatin1Char('/')) {
                mask[i]=false; mask[++i]=false; mode=LineComment;
            } else if (c==QLatin1Char('/') && next==QLatin1Char('*')) {
                mask[i]=false; mask[++i]=false; mode=BlockComment;
            } else if (c==QLatin1Char('\'')) { mask[i]=false; mode=Single; }
            else if(c==QLatin1Char('"')) { mask[i]=false; mode=Double; }
            else if(c==QLatin1Char('`')) { mask[i]=false; mode=Template; }
        } else {
            mask[i]=false;
            if (mode==LineComment && c==QLatin1Char('\n')) mode=Code;
            else if(mode==BlockComment && c==QLatin1Char('*') && next==QLatin1Char('/')) {
                if(i+1<s.size()) mask[++i]=false;
                mode=Code;
            } else if (mode==Single || mode==Double || mode==Template) {
                if(c==QLatin1Char('\\')) { if(i+1<s.size()) mask[++i]=false; }
                else if((mode==Single && c==QLatin1Char('\'')) ||
                        (mode==Double && c==QLatin1Char('"')) ||
                        (mode==Template && c==QLatin1Char('`'))) mode=Code;
                else if(c==QLatin1Char('\n') && mode!=Template) mode=Code;
            }
        }
    }
    return mask;
}
QString canonical(QString p) {
    if(p==QStringLiteral("Wavegen") || p.startsWith(QStringLiteral("Wavegen.")))
        p.replace(0,7,QStringLiteral("Wavegen1"));
    if(p==QStringLiteral("Scope") || p.startsWith(QStringLiteral("Scope.")))
        p.replace(0,5,QStringLiteral("Scope1"));
    if(p==QStringLiteral("Supplies1") || p.startsWith(QStringLiteral("Supplies1."))) p.replace(0,9,QStringLiteral("Supplies"));
    if(p==QStringLiteral("Patterns") || p.startsWith(QStringLiteral("Patterns."))) p.replace(0,8,QStringLiteral("Patterns1"));
    p.replace(QStringLiteral(".Channel2"),QStringLiteral(".Channel1"));
    return p;
}
}

bool inCode(const QString& source, int position) {
    if(position<0 || position>source.size()) return false;
    // At a caret, check the next character context by appending an identifier.
    const auto mask=codeMask(source.left(position)+QStringLiteral("x"));
    return !mask.isEmpty() && mask.constLast();
}
int matchingBracket(const QString& source, int bracketIndex) {
    if(bracketIndex<0 || bracketIndex>=source.size()) return -1;
    const QString open=QStringLiteral("([{"), close=QStringLiteral(")]} ").trimmed();
    const QChar c=source.at(bracketIndex);
    const int left=open.indexOf(c),right=close.indexOf(c);
    if(left<0 && right<0) return -1;
    const auto mask=codeMask(source);
    if(!mask.at(bracketIndex)) return -1;
    const bool forward=left>=0;
    const QChar target=forward ? close.at(left) : open.at(right);
    const int step=forward?1:-1;
    int depth=0;
    for(int i=bracketIndex;i>=0 && i<source.size();i+=step) {
        if(!mask.at(i)) continue;
        if(source.at(i)==c) ++depth;
        else if(source.at(i)==target && --depth==0) return i;
    }
    return -1;
}

ValueContext valueContext(const QString& beforeCaret) {
    // Matches only when the caret is inside a string being assigned to a
    // supported *.text enum, or inside the first argument of a measure call.
    // Comments and escaped quotes must not open a value popup.
    static const QRegularExpression rx(QStringLiteral(
        R"(([A-Za-z_$][\w$]*(?:\.[A-Za-z_$][\w$]*)+)\s*=\s*(["'])([^"'\n]*)$)"));
    static const QRegularExpression fn(QStringLiteral(
        R"(([A-Za-z_$][\w$]*(?:\.[A-Za-z_$][\w$]*)+)\s*\(\s*(["'])([^"'\n]*)$)"));
    auto m=rx.match(beforeCaret);
    if(!m.hasMatch()) m=fn.match(beforeCaret);
    if(!m.hasMatch()) return {};
    const int quoteAt=m.capturedStart(2);
    // The opening quote must be a code character's lexical delimiter, not a
    // quote inside another string or a comment.
    if (quoteAt<0 || (quoteAt>0 && !inCode(beforeCaret,quoteAt))) return {};
    return {m.captured(1),m.captured(3),true};
}

CallContext callContext(const QString& beforeCaret) {
    // Identify the innermost open function call; ignore braces in comments or
    // strings. Keep nested calls from incrementing the enclosing arg index.
    const auto mask=codeMask(beforeCaret);
    struct Frame { int start; int arg; int square; int curly; };
    QVector<Frame> frames;
    int square=0,curly=0;
    for(int i=0;i<beforeCaret.size();++i) {
        if(!mask.at(i)) continue;
        const QChar c=beforeCaret.at(i);
        if(c==QLatin1Char('[')) ++square;
        else if(c==QLatin1Char(']')) square=qMax(0,square-1);
        else if(c==QLatin1Char('{')) ++curly;
        else if(c==QLatin1Char('}')) curly=qMax(0,curly-1);
        else if(c==QLatin1Char('(')) frames.append({i,0,square,curly});
        else if(c==QLatin1Char(')') && !frames.isEmpty()) frames.removeLast();
        else if(c==QLatin1Char(',') && !frames.isEmpty() &&
                frames.last().square==square && frames.last().curly==curly) ++frames.last().arg;
    }
    if(frames.isEmpty()) return {};
    const Frame f=frames.constLast();
    const QString lhs=beforeCaret.left(f.start);
    static const QRegularExpression name(QStringLiteral(R"(([A-Za-z_$][\w$]*(?:\.[A-Za-z_$][\w$]*)*)\s*$)"));
    const auto m=name.match(lhs);
    if(!m.hasMatch()) return {};
    return {m.captured(1),f.arg,true};
}

QStringList enumValues(QString p) {
    p=canonical(p);
    if(p.startsWith(QStringLiteral("Wavegen1.Channel1.")) &&
       p.endsWith(QStringLiteral(".Simple.Type.text")))
        return {"DC","Sine","Square","Triangle","Ramp Up","Ramp Down","Noise","Custom"};
    if(p.startsWith(QStringLiteral("Wavegen1.Channel1.")) &&
       (p.endsWith(QStringLiteral(".Modulation.FM.Type.text")) ||
        p.endsWith(QStringLiteral(".Modulation.AM.Type.text"))))
        return {"Sine","Triangle","Square","Ramp Up","Ramp Down"};
    if(p.endsWith(QStringLiteral(".Mode.text")) && p.startsWith(QStringLiteral("Wavegen1.Channel")))
        return {"Simple","Sweep","Play"};
    if(p==QStringLiteral("Scope1.Time.Mode.text")) return {"Repeated","Screen","Shift","Record"};
    if(p==QStringLiteral("Scope1.Trigger.Trigger.text")) return {"None","Auto","Normal","Repeated"};
    if(p==QStringLiteral("Scope1.Trigger.Type.text")) return {"Edge","Pulse","Transition"};
    if(p==QStringLiteral("Scope1.Trigger.Source.text"))
        return {"Channel 1","Channel 2","Wavegen 1","Wavegen 2","External 1","External 2","PC"};
    if(p==QStringLiteral("Scope1.Trigger.Condition.text")) return {"Rising","Falling"};
    if(p==QStringLiteral("Scope1.Trigger.Filter.text")) return {"Decimate","Average"};
    if((p==QStringLiteral("Scope1.Channel1.SampleMode.text"))) return {"Decimate","Average","Min/Max"};
    if(p.endsWith(QStringLiteral(".Trigger.Source.text")) && p.startsWith(QStringLiteral("Wavegen1.Channel")))
        return {"None","PC","Detector 1","Detector 2","Wavegen 1","Wavegen 2","External 1","External 2"};
    if(p.endsWith(QStringLiteral(".Trigger.Idle.text")) && p.startsWith(QStringLiteral("Wavegen1.Channel")))
        return {"Disable","Offset","Initial"};
    if(p.endsWith(QStringLiteral(".Type.text")) && p.startsWith(QStringLiteral("Patterns1.Channels.DIO")))return {"Clock","Custom"};
    if(p.endsWith(QStringLiteral(".Output.text")) && p.startsWith(QStringLiteral("Patterns1.Channels.DIO")))return {"PP"};
    if(p.endsWith(QStringLiteral(".Idle.text")) && p.startsWith(QStringLiteral("Patterns1.Channels.DIO")))return {"Z"};
    if(p.startsWith(QStringLiteral("Scope1.Channel1.")) &&
       (p.endsWith(QStringLiteral(".measure")) ||
        p.endsWith(QStringLiteral(".measureCapture")) ||
        p.endsWith(QStringLiteral(".measureWindow"))))
        return {"RMS","ACRMS","Average","Minimum","Maximum","Peak2Peak","Middle","Amplitude", "Frequency", "Period", "DutyCycle", "RiseTime", "FallTime", "PositiveWidth", "NegativeWidth", "CrestFactor", "Variance", "Standard Deviation"};
    return {};
}

QString signature(QString p,int argument) {
    p=canonical(p);
    const auto tip=[&](const QString& s,const QString& d) {return s+QStringLiteral("\n")+d+QStringLiteral("\nArgument ")+QString::number(argument+1);};
    if(p.startsWith(QStringLiteral("Scope1.Channel1.")) &&
       (p.endsWith(QStringLiteral(".measure")) || p.endsWith(QStringLiteral(".measureCapture"))))
        return tip(QStringLiteral("measure(name [, startSeconds, endSeconds])"),QStringLiteral("Measurement name and optional time window; units depend on measurement"));
    if(p.startsWith(QStringLiteral("Scope1.Channel1.")) && p.endsWith(QStringLiteral(".measureWindow")))
        return tip(QStringLiteral("measureWindow(name, startSeconds, endSeconds)"),QStringLiteral("Window bounds in seconds"));
    if(p.endsWith(QStringLiteral(".spectrum")))
        return tip(QStringLiteral("spectrum(fftSize, window)"),QStringLiteral("Power-of-two size; Hann, Rectangular or Blackman"));
    if(p.endsWith(QStringLiteral(".statistics")))
        return tip(QStringLiteral("statistics([startSeconds, endSeconds])"),QStringLiteral("Statistics over complete or selected acquisition"));
    if(p==QStringLiteral("Patterns1.runSequence")) return tip(QStringLiteral("runSequence([pin0, pin1, ...], [bits0, bits1, ...], bitRateHz)"),QStringLiteral("1..3 synchronized cyclic bit strings; use 0/1 characters with equal lengths"));
    if(p==QStringLiteral("Patterns1.configSequence")) return tip(QStringLiteral("configSequence(pins, bitStrings, bitRateHz)"),QStringLiteral("Load synchronized bit strings without starting the digital outputs"));
    if(p==QStringLiteral("Patterns1.runEncoder")) return tip(QStringLiteral("runEncoder(aPin, bPin, indexPin, ppr, rpm, reverse)"),QStringLiteral("Start synchronized hardware A/B/index custom patterns; 3 unique output pins"));
    if(p==QStringLiteral("Patterns1.configEncoder")) return tip(QStringLiteral("configEncoder(aPin, bPin, indexPin, ppr, rpm, reverse)"),QStringLiteral("Configure A/B/index outputs idle Z without starting"));
    if(p==QStringLiteral("Patterns1.run")) return tip(QStringLiteral("run()"),QStringLiteral("Start hardware DigitalOut clock/PWM after staging parameters"));
    if(p==QStringLiteral("Patterns1.config")) return tip(QStringLiteral("config()"),QStringLiteral("Commit channel settings without starting output"));
    if(p==QStringLiteral("Patterns1.stop")) return tip(QStringLiteral("stop()"),QStringLiteral("Stop DigitalOut and release driven pattern pin to high-Z"));
    if(p==QStringLiteral("Patterns1.status")) return tip(QStringLiteral("status()"),QStringLiteral("Return cached settings plus actual SDK state and repeat readback when available"));
    if(p==QStringLiteral("TestRunner.loadDefinition")) return tip(QStringLiteral("loadDefinition({schemaVersion:1,id,title,config,steps}, exports)"),QStringLiteral("Validate all steps and named library exports atomically; no test starts"));
    if(p==QStringLiteral("TestRunner.loadDefinitionFile")) return tip(QStringLiteral("loadDefinitionFile(filePath, exports)"),QStringLiteral("Load a .wftest.json definition from disk; max 128 KiB"));
    if(p==QStringLiteral("TestRunner.saveDefinition")) return tip(QStringLiteral("saveDefinition(filePath)"),QStringLiteral("Export loaded data-only definition as .wftest.json"));
    if(p==QStringLiteral("TestRunner.definition")) return tip(QStringLiteral("definition()"),QStringLiteral("Retrieve imported versioned definition; excludes function bodies"));
    if(p==QStringLiteral("TestRunner.add")) return tip(QStringLiteral("add({id,name,group?,type?,mode?,intervalMs?,timeoutMs?,durationMs?}, handler)"),QStringLiteral("Setup/Main/Cleanup groups; action/test types; one-shot, Qt delay or JS poll; imported modules"));
    if(p==QStringLiteral("TestRunner.run")) return tip(QStringLiteral("run()"),QStringLiteral("Start/resume native sequence; async timed steps report final state via onFinished"));
    if(p==QStringLiteral("TestRunner.onFinished")) return tip(QStringLiteral("onFinished(callback)"),QStringLiteral("Register terminal-state callback(summary) on same JS worker thread"));
    if(p==QStringLiteral("TestRunner.terminate")) return tip(QStringLiteral("terminate()"),QStringLiteral("Graceful stop; skip Main tests and run Cleanup"));
    if(p==QStringLiteral("TestRunner.setRunInfo")) return tip(QStringLiteral("setRunInfo({uutId?,stationId?,operatorId?,...})"),QStringLiteral("C++ run metadata; call after reset/clear, before run; retained in report()"));
    if(p==QStringLiteral("TestRunner.setVariable")) return tip(QStringLiteral("setVariable(name,value)"),QStringLiteral("Share a primitive value between library step functions; active callback only"));
    if(p==QStringLiteral("TestRunner.variable")) return tip(QStringLiteral("variable(name)"),QStringLiteral("Retrieve a C++-owned value from an earlier step"));
    if(p==QStringLiteral("TestRunner.runInfo")) return tip(QStringLiteral("runInfo()"),QStringLiteral("Frozen UUT metadata of current/last execution; never changes when editing the next unit"));
    if(p==QStringLiteral("TestRunner.pendingRunInfo")) return tip(QStringLiteral("pendingRunInfo()"),QStringLiteral("Live project-owned UUT inputs shared by all Run Information widgets, including hidden Pages"));
    if(p==QStringLiteral("TestRunner.variables")) return tip(QStringLiteral("variables()"),QStringLiteral("Snapshot of shared per-run variables"));
    if(p==QStringLiteral("TestRunner.saveReport")) return tip(QStringLiteral("saveReport(filePath, format='json'|'csv')"),QStringLiteral("Write a completed run to a named .json/.csv file atomically; folder must exist"));
    if(p==QStringLiteral("TestRunner.exportReport")) return tip(QStringLiteral("exportReport(format='json'|'csv')"),QStringLiteral("Save completed run under Documents/WFScript Reports; returns full path or empty string"));
    if(p==QStringLiteral("TestRunner.archiveInfo")) return tip(QStringLiteral("archiveInfo()"),QStringLiteral("Last automatic archive status: status, path, directory, error; no new run and no disk write"));
    if(p==QStringLiteral("TestRunner.report")) return tip(QStringLiteral("report()"),QStringLiteral("Structured per-run record of step states, timestamps and measurements"));
    if(p==QStringLiteral("TestRunner.summary")) return tip(QStringLiteral("summary()"),QStringLiteral("Native state, total elapsedMs, stepElapsedMs, pollCount, currentStepId"));
    if(p==QStringLiteral("Logic1.wait")) return tip(QStringLiteral("wait([timeoutSeconds])"),QStringLiteral("Wait for DigitalIn completion and transfer 16-bit sample words"));
    if(p==QStringLiteral("Logic1.single")) return tip(QStringLiteral("single()"),QStringLiteral("Arm untriggered DigitalIn acquisition using Logic1.Time settings"));
    if(p==QStringLiteral("Logic1.samples")) return tip(QStringLiteral("samples()"),QStringLiteral("Returns last completed 16-bit DIO sample words"));
    if(p==QStringLiteral("Logic1.captureDiagnostics")) return tip(QStringLiteral("captureDiagnostics()"),QStringLiteral("Read byte-count and half-buffer acquisition diagnostics"));
    if(p==QStringLiteral("StaticIO.setPin")) return tip(QStringLiteral("setPin(index, state)"),QStringLiteral("Pin 0..15; state 0=LOW, 1=HIGH, 2=Z; commit immediately"));
    if(p==QStringLiteral("StaticIO.readPin")) return tip(QStringLiteral("readPin(index)"),QStringLiteral("Read fresh pin level, returns -1 on error"));
    if(p==QStringLiteral("StaticIO.config")) return tip(QStringLiteral("config()"),QStringLiteral("Apply staged 16-pin StaticIO state to hardware"));
    if(p==QStringLiteral("StaticIO.stop")) return tip(QStringLiteral("stop()"),QStringLiteral("Release all StaticIO outputs to high impedance"));
    if(p==QStringLiteral("Supplies.run")) return tip(QStringLiteral("run()"),QStringLiteral("Explicitly enables master and applies supply settings"));
    if(p==QStringLiteral("Supplies.config")) return tip(QStringLiteral("config()"),QStringLiteral("Apply staged V+/V- settings to AD3"));
    if(p==QStringLiteral("Supplies.status")) return tip(QStringLiteral("status()"),QStringLiteral("Read actual AD3 voltage status when available"));
    if(p==QStringLiteral("Wavegen1.play")) return tip(QStringLiteral("play(channel, timeoutSeconds)"),QStringLiteral("Finite normalized waveform stream"));
    if(p==QStringLiteral("Scope1.wait")) return tip(QStringLiteral("wait([timeoutSeconds])"),QStringLiteral("Wait for acquisition to finish; returns bool"));
    if(p==QStringLiteral("Scope1.saveReference")) return tip(QStringLiteral("saveReference(channelNumber)"),QStringLiteral("Store channel 1 or 2 samples"));
    if(p==QStringLiteral("Scope1.mathData")) return tip(QStringLiteral("mathData(operation)"),QStringLiteral("Math operation, e.g. CH1-CH2 or CH1-REF1"));
    if(p.endsWith(QStringLiteral(".Custom.set")) || p.endsWith(QStringLiteral(".Play.set")))
        return tip(QStringLiteral("set(samples)"),QStringLiteral("Array of normalized voltage samples [-1,1]"));
    if(p==QStringLiteral("plots.setSeries")) return tip(QStringLiteral("setSeries(id, seriesName, xArray, yArray [, options])"),QStringLiteral("Replace explicitly addressed X/Y series"));
    if(p==QStringLiteral("plots.setSamples")) return tip(QStringLiteral("setSamples(id, name, samples, sampleRateHz [, startSeconds, options])"),QStringLiteral("Uniformly sampled time axis"));
    if(p==QStringLiteral("plots.fromScope")) return tip(QStringLiteral("fromScope(plotId [, channel=1, name])"),QStringLiteral("Copy completed Scope1 channel capture to one Panel plot"));
    if(p==QStringLiteral("plots.configure")) return tip(QStringLiteral("configure(plotId, options)"),QStringLiteral("Chart types, labels and X/Y bounds; Panel-only"));
    if(p==QStringLiteral("plots.append")) return tip(QStringLiteral("append(plotId, name, xArray, yArray [, options])"),QStringLiteral("Append bounded chart history"));
    if(p==QStringLiteral("plots.appendPoint")) return tip(QStringLiteral("appendPoint(plotId, name, x, y)"),QStringLiteral("Append one measurement to a trend"));
    if(p==QStringLiteral("plots.styleSeries")) return tip(QStringLiteral("styleSeries(plotId, name, {color, width, visible, markerSize})"),QStringLiteral("Restyle a named chart series without replacing its samples"));
    if(p==QStringLiteral("plots.removeSeries")) return tip(QStringLiteral("removeSeries(plotId, name)"),QStringLiteral("Remove only one series"));
    if(p==QStringLiteral("plots.clear")) return tip(QStringLiteral("clear(plotId)"),QStringLiteral("Clear all series in one chart"));
    if(p==QStringLiteral("wait")) return tip(QStringLiteral("wait(seconds)"),QStringLiteral("Delay, or cancellation-aware pause"));
    if(p==QStringLiteral("print")) return tip(QStringLiteral("print(value, ...)"),QStringLiteral("Write output to script console"));
    return {};
}
} // namespace EditorAssist
