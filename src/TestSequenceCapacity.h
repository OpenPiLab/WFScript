#pragma once
// RC1.5.14.2 — independent bounded execution and operator display capacities.
// A short live table is not a limit on the native report or the test scheduler.
namespace TestSequenceCapacity {
inline constexpr int maxExpandedSteps = 10000;
inline constexpr int maxSweepAxisPoints = 10000;
inline constexpr int maxRepeatCount = 10000;
inline constexpr int maxMeasurementsPerRun = 100000;
inline constexpr int visibleSequenceSteps = 100;
inline constexpr int visibleMeasurementRows = 100;
}
