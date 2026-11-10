// WFScript v5.6.2 type-aware autocomplete test. This is an editor fixture.
// Do not run this file: it uses physical Wavegen settings.
function setup56(f1, amp1, f2, amp2) {
    Scope.stop();
    Wavegen.stop();
    for (var i = 1; i <= 2; i++) {
        var ch = i === 1 ? Wavegen.Channel1 : Wavegen.Channel2;
        ch.enable = true;
        ch.Mode.text = "Simple";
        ch.Simple.Type.text = "Sine";
        ch.Simple.Frequency.value = i === 1 ? f1 : f2;
        ch.Simple.Amplitude.value = i === 1 ? amp1 : amp2;
        ch.Simple.Offset.value = 0;
        ch.Modulation.FM.enable = false;
        ch.Modulation.AM.enable = false;
        ch.Sweep.Frequency.enable = false;
        ch.Sweep.Amplitude.enable = false;
        ch.Trigger.Source.text = "None";
        ch.Trigger.Idle.text = "Offset";

        // Type "ch." below to see Wavegen channel members.
        // Type "ch.Simple." for Frequency, Amplitude, etc.
        // Type "ch.Modulation.FM." for enable, Type, Frequency and Depth.
        // Type "ch.Trigger.Source." for text, value, real, preset.
        // Test on an UNCOMMENTED line, for example just above this comment.
    }
}

function scopeExample() {
    const channel = Scope.Channel2;
    // Try "channel." and "channel.Range." on a new line here.
    const f = Wavegen.Channel1.Simple.Frequency;
    // Try "f." for value/real/text/preset.
}

function shadowingExample() {
    const channel = Scope.Channel1;
    {
        const channel = Wavegen.Channel2;
        // Try "channel." here: Wavegen channel.
    }
    // Try "channel." here: Scope channel.
}
