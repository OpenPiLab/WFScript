# WFScript Built-in Examples — v0.1

The **Examples** menu in Panel Designer loads editable `.wfpanel` projects embedded by `src/resources/examples.qrc`. Each example is a normal project that can be inspected, run, and saved.

## Groups

| Group | Focus |
| --- | --- |
| Getting Started | Orientation and general widgets |
| Graphics & Charts | Labels, equations, images, and plotting techniques |
| Tables & Data | Equations, editable tables, calibration, and table-driven workflows |
| Analog Instruments | Wavegen, Scope, and supplies examples |
| Digital Instruments | Static I/O, logic/timing, clocks, quadrature, and sequences |
| Test Sequences & Executive | Test widgets, run information, and automated test flows |

## Particularly useful examples

- **Getting Started** — first orientation example.
- **TableDrivenWavegen** — operator-entered table rows used to drive a sweep and retain traces.
- **TableLimitPlot** — table data turned into nominal/observed/limit plots.
- **SignalLab** — practical analog controls.
- **AD3HardwareTestStation** — full automated test example.

## Example maintenance

Readable source is kept in `examples/source/` and embedded project panels in `examples/panels/`.
After editing example scripts, run:

```powershell
python tools/build_examples.py
python tools/build_examples.py --check
```
