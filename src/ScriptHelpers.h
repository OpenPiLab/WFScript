#pragma once
#include <QString>
// Shared WaveForms-style JavaScript convenience functions. This is intentionally
// independent from both the one-shot Script Editor and the persistent Panel UI.
namespace ScriptHelpers {
inline QString source() { return QStringLiteral(R"WFJS(
    var PI = Math.PI;
    var E = Math.E;
    var min = Math.min;
    var max = Math.max;
    var pow = Math.pow;
    var round = Math.round;
    var abs = Math.abs;
    var floor = Math.floor;
    var ceil = Math.ceil;
    var sqrt = Math.sqrt;
    var sin = Math.sin;
    var cos = Math.cos;
    var tan = Math.tan;
    var asin = Math.asin;
    var acos = Math.acos;
    var atan = Math.atan;
    var atan2 = Math.atan2;
    var exp = Math.exp;
    var log = Math.log;
    var log10 = function(x) { return Math.log(x) / Math.LN10; };
    var clear = function() { __bridge.clear(); };
)WFJS"); }
}
