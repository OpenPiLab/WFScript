# Lab 23 — Multi-Channel Sequence Lab

**Level:** Advanced\
**Category:** Digital Instruments\
**Pages:** Sequence Lab

## What you will build or observe

Generate synchronized binary, Gray and walking-one sequences on several digital pins.

## Before you run

Simulation first; physical test uses DIO0→DIO3, DIO1→DIO4 and DIO2→DIO5, each through 1 kΩ.

## Follow along

1. Choose a PATTERN PRESET and BIT RATE.
2. Press PREVIEW (MATH) to inspect its expected transitions.
3. When wired, START SEQUENCE then VERIFY 3× LOOPBACK.
4. STOP / RELEASE the outputs.

## Follow the JavaScript

`seqStart` → `Patterns1.runSequence()`; `seqVerify` → captured DIO samples and expected sequence matching.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| Sequence Lab | `seqPreset` (option_selector) | `selectionChanged` | `seqInfo()` |
| Sequence Lab | `seqBitRate` (option_selector) | `selectionChanged` | `seqInfo()` |
| Sequence Lab | `seqStart` (button) | `clicked` | `seqStart()` |
| Sequence Lab | `seqStop` (button) | `clicked` | `seqStop()` |
| Sequence Lab | `seqCapture` (button) | `clicked` | `onSeqCaptureClicked()` |
| Sequence Lab | `seqVerify` (button) | `clicked` | `onSeqVerifyClicked()` |
| Sequence Lab | `seqPreview` (button) | `clicked` | `seqPreview()` |

## Make one small change

Compare Binary Counter and Gray Counter; note how many bits change at a state transition.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

Pattern preview is software-only; hardware verification uses a separate input capture.

## Safety and scope

Check each output-to-input link independently and never connect two driven outputs directly.

## If something does not work

If a captured sequence is offset, remember acquisition may start at any point in the repeating pattern.

## Related files

- **Editable project:** `examples/panels/SequenceLab.wfpanel`
- **Readable JavaScript:** `examples/source/SequenceLab.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
