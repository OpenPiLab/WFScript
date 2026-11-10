# v5.6.70.1 Education widgets / on-disk contract

Existing `.wfpanel` format remains `WFScriptPanel` version 2. No file-level migration, no external files, and no base64 image changes.

## Mathematical Equation (`type: equation`)

`properties.source` is a UTF-8 JSON string (up to 2,048 characters) containing a documented **LaTeX-style subset**, not a complete LaTeX interpreter. `properties.pointSize` (12–48), `titleVisible`, `borderVisible` are ordinary properties. `text` is the optional title. Brace nesting at most 12. The Properties inspector has a multiline source editor, live preview and Fraction/Subscript/Square root/Greek π insert buttons. Rendered locally via Qt; no network, MathJax, TeX distribution or extra Qt modules.

Supported: literal letters, numbers, grouping `{...}`, subscript `_`, superscript `^`, `\frac{a}{b}`, `\sqrt{x}`, `\mathrm{}`, `\text{}`, `\operatorname{}`, common Greek letters (`\alpha`, `\beta`, `\pi`, `\omega`, `\tau`), `\cdot`, `\times`, `\pm`, `\approx`, inequality signs. Invalid/unrecognized sequences are displayed in a literal fallback; unsupported full LaTeX features are not promised. Use `ui.set(equationId, "...")` to replace the displayed source during the current run; the saved source isn't overwritten. `ui.get` initially returns the saved equation source.

## Experiment Data Table (`type: experiment_table`)

`properties.columns` pipe-separated 1–8 column headers; `properties.initialRows` is one pipe-separated row per line with the same column count (blank cells are allowed). `maxRows` 1–250; `editable` Boolean. The normal 16 KiB property limit remains. No runtime rows are saved as a document mutation. `ui.set(tableId, [[column1, column2, ...],...])` replaces all rows atomically, rejecting incompatible data. Cells are bounded to 256 characters. When `editable=true`, user cell edits can emit `valueChanged(rows)` via a configured Panel event handler. Read the edited rows with `ui.get(tableId)` while running. Right-click in Run mode for CSV export or TSV copy. CSV export uses UTF-8 and quotes every cell, defusing spreadsheet formula injection.

The table is general experimental data, independent of the native TestRunner Measurement Table. Initial row data and columns are design-time values; archived TestRunner records are unchanged.

## v5.6.70.1 corrective behavior (format unchanged)

- Equation `pointSize` now controls the displayed glyph dimensions: 2x supersampled raster graphics are displayed at their natural logical size, shrinking only when the widget is smaller than the equation. Canvas resizing no longer stretches equations to fill available space. The equation source and properties are unchanged.
- Table `initialRows`, runtime `ui.set` nested row arrays, `ui.get`, and `valueChanged` now preserve the outer row structure rather than flattening its cells with `QVariantList.append(QVariantList)`. Saved `columns`/`initialRows` strings have not changed; previous Panels open without migration.

## Native JavaScript value contract (v5.6.70.2)

An Experiment Data Table is an ordinary JavaScript **array of row arrays** when read by `ui.get(id)` or passed to `valueChanged(rows)`. `Array.isArray(rows)` and `Array.isArray(rows[0])` must both return `true`. The same applies to script-originated `ui.set(tableId, rows)` after the widget reads back the value. `map()`, `filter()`, `push()`, `JSON.stringify()` and ordinary indexing work directly. This applies to any Panel values with nested lists/maps; old `.wfpanel` files need no migration.

```javascript
const rows = ui.get('recipeTable');
const freqs = rows.filter(row => row[6] === 'yes').map(row => Number(row[0]));
ui.on('recipeTable', 'valueChanged', rows => print('Edited rows:', rows.length));
```

Returned arrays/objects are copies of native Panel state: mutating a copy does not update the widget until `ui.set()` is called. Scalars remain strings, numbers, booleans, or null/undefined as appropriate. `TestRunner.variable()` currently stores scalar values only; `TestRunner.results()` explicitly returns a list.
