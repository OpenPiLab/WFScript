#pragma once

// Controls for independently implemented, documented DWF analog-input triggers.
// Percent is horizontal position measured from left edge of displayed capture.
enum class ScopeTriggerMode { Auto, Normal, None };

struct ScopeTriggerSettings {
    ScopeTriggerMode mode = ScopeTriggerMode::Auto;
    int channel = 0;                    // 0: CH1, 1: CH2
    bool rising = true;
    double levelVolts = 0.0;
    double hysteresisVolts = 0.05;    // 50 mV default for reliable slope discrimination
    double positionPercent = 50.0;     // 0% left, 50% center, 100% right
    int type = 0;                      // 0 edge, 1 pulse, 2 transition
    unsigned char source = 2;         // DWF TRIGSRC: detector=2, AO1=7, AO2=8, EXT1=11, etc.
    int lengthCondition = 1;          // Less=0 Timeout=1 More=2
    double lengthSeconds = 0.0005;
    double holdoffSeconds = 0.0;
    int filter = 0;                    // Decimate=0, Average=1
    double autoTimeoutSeconds = 0.20;  // ignored for Normal and None
};
