# Native sampled plots — WFScript RC1.4 candidate

## Goal

A hardware-independent, opt-in way for a Panel plot to display *all* samples from a `BinarySamples` buffer without constructing JavaScript X/Y arrays, while maintaining the existing `plots.setSeries()`, `plots.setSamples()`, and visual Panel serialization contracts.

## API

```javascript
const raw = Binary.wordPattern(1000000);
const samples = raw.decodeSamples('u16le', 0, 1000000, 2/65535, -1);
plots.setNativeSamples('plot1', 'CH1', samples, 0.00002, -10, {
    color: '#f5d23f', width: 1.5
});
raw.release();
// Safe: the chart keeps a Qt implicit-shared copy of the numerical data.
samples.release();
```

`plots.setNativeSamples(plotId, name, BinarySamples, xStep, xStart=0, options={})` accepts a **positive finite X increment** in the *plot's X-axis units*, a finite start, and up to 4,000,000 native samples per named series. Maximum named series per plot remains 12. For Rigol X is milliseconds: `xStep = xIncrementSeconds * 1000`. For arbitrary or nonuniform X use the existing `plots.setSeries()` method.

All native rendering, viewport range selection, exact-extrema index tracking and cursor sampling are in C++. The renderer builds 64-sample extrema blocks once on the GUI thread, then visits only relevant indexed blocks and two boundary fragments on each redraw. At close zoom it draws original samples; at wide zoom it emits actual min/max sample coordinates in their original time order. It **never** plots artificially ordered min-then-max values. The immutable queued `PlotCommand` contains Qt shared numeric storage, not JavaScript arrays or a QObject pointer across threads.

### Working with view and data in JavaScript

```javascript
plots.onViewChanged('plot1', function(view) {
    // Sent on zoom, pan, double-click fit, or plots.setView().
    print(view.xMin, view.xMax, view.yMin, view.yMax, view.isZoomed);
});

const lastView = plots.getView('plot1'); // last GUI view, asynchronous snapshot
plots.setView('plot1', 2, 4, -1, 1);     // queued, does not synchronously alter lastView
const info = plots.nativeSeriesInfo('plot1', 'CH1');
// -> {native:true, count:1000000, xStart:-10, xStep:0.00002, xEnd:9.99998}

// Keep a decoded BinarySamples variable alive if you intend to inspect
// original samples later. Plot ownership does NOT make released wrappers readable.
const from = Math.max(0, Math.ceil((lastView.xMin-info.xStart)/info.xStep));
const until = Math.min(info.count, Math.floor((lastView.xMax-info.xStart)/info.xStep)+1);
if (until > from) {
    const stats = samples.rangeStats(from, until-from);
    print(stats.min,stats.max,stats.mean,stats.rms);
}
```

The GUI -> worker view callback reuses the Panel's ordinary event dispatch and may be bound in the visual Events editor (`viewChanged(view)`) or through `ui.on()`. `plots.getView()` returns an empty object until the first zoom/pan/reset/programmatic `setView` event. The callback is asynchronous and should remain cheap: avoid scanning millions of samples in every drag event. Use an explicit button, throttling or small ranges for costly calculations.

`BinarySamples.rangeStats(offset,count)` returns `{start,count,min,max,mean,rms}` after exact native calculation of the selected samples. It does not allocate JavaScript arrays. `toArray(offset,count)` remains limited to 4096 for intentional small exports.

## Ownership and safety

- `BinarySamples` and `BinaryBuffer` remain local to the script worker. The GUI never holds worker `QObject*` pointers.
- `QVector<double>` in the queued `PlotCommand` uses Qt implicit sharing. Calling `BinarySamples.release()` after `setNativeSamples()` is safe; mutation/removal of one owner detaches. The plot retains its own immutable view.
- `plots.clear()` / `plots.removeSeries()` release the chart data. Series changes never change the underlying SCPI RAW export bytes.
- Native series support `Line`/`XY` and basic analogue cursors. Existing chart XY/scatter/bar/area/step/digital representations continue using the established JavaScript data path. The native path currently renders as a connected analogue line regardless of chart type; this is a deliberate initial scope-oriented limit. Do not use native series for digital lanes, Log Y mixed-sign data or nonuniform/unsorted X traces without further testing.
- The first full-resolution index build occurs on the GUI thread when a native series is installed. It has an up-front O(N) cost. Subsequent zoom/pan redraws use indexed ranges instead of O(N) scans.
- These changes are not yet Windows Qt compiled or physically hardware-tested. The native index's compiled C++ smoke test and Panel JS driver mocks pass offline; the offscreen Qt test must be run on Windows.

## Rigol v0.6.4 example

`Rigol_Four_Instrument_Operator_Bench_v0_6_4_Native_Charts.wfpanel` preserves the v0.6.3 UI/SCPI modules. FULL RAW in native mode concatenates its original binary chunks, performs one native decode per channel for the plot, installs the full-resolution series, and releases its temporary decode. The unchanged chunked RAW storage remains available for byte-identical export. SCREEN (~1000 samples/channel) stays on the legacy XY API because it has no need for million-sample native indexing. With RC1.3.1, this Panel still falls back to the older ~1700-point JS preview, but native high-resolution zoom is only available in RC1.4.

## Validation plan

1. Windows Qt 6.9.2 MinGW: build Release, run all CTests, especially `WFScriptNativeSampleIndexSmoke`, `WFScriptNativeChartSmoke`, existing Plot/Panel and Binary tests.
2. Open the hardware-free lab: four 1M series, zoom repeatedly, verify no major lag and inspect visible stats. Confirm genuine original-sample zoom and narrow spikes with the Qt smoke.
3. Open Rigol v0.6.4; verify the four Pages load and no SCPI output changes on startup. Connect MHO954 and capture 1M samples with CH1, then with CH1+CH2. Zoom and pan both traces; compare colours, X times, measurements and correct waveform falling slopes.
4. Save RAW and compare with a v0.6.3 capture from the same stopped record (CRC32/SHA256); reject any byte difference. Verify legacy transport remains functional.
5. Record Windows GUI CPU time and subjective responsiveness before promoting RC1.4. No C++/hardware speed claim is made until those measurements exist.


## RC1.5.1 — Dense-trace appearance

A dense native series renders as a screen-bounded antialiased centerline of
the arithmetic mean of the **original finite samples in each horizontal
column**, with a lower-opacity min/max envelope drawn behind it. The raw
samples are unchanged. The envelope keeps narrow spikes and noise visible
even when a pixel column contains thousands of samples. Zooming to the sample
level uses the exact original samples rather than display-column averages.
The mean centerline is a *visual aggregation*, not a low-pass filter applied
to stored samples.

The default envelope opacity is 0.32; tune it for a particular native plot:

```js
plots.configure('scopeOverlay', { nativeEnvelopeOpacity: 0.32 });
// 0.0: show clean mean trace only (spikes may be visually hidden when zoomed out)
// 1.0: full-strength min/max envelope (more like RC1.5)
```

The setting is display-only and does not affect BinarySamples, native
statistics or raw exports. Chart legend visibility changes preserve the
current wheel/drag zoom. Double-click performs Fit/Reset.
