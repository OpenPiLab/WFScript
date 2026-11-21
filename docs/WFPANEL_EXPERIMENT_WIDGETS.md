# v5.6.70.1 Education widgets / on-disk contract

Existing `.wfpanel` format remains `WFScriptPanel` version 2. No file-level migration, no external files, and no base64 image changes.

## Mathematical Equation (`type: equation`)

`properties.source` is a UTF-8 JSON string (up to 2,048 characters) containing a documented **LaTeX-style subset**, not a complete LaTeX interpreter. `properties.pointSize` (12–48), `titleVisible`, `borderVisible` are ordinary properties. `text` is the optional title. Brace nesting at most 12. The Properties inspector has a multiline source editor, live preview and Fraction/Subscript/Square root/Greek π insert buttons. Rendered locally via Qt; no network, MathJax, TeX distribution or extra Qt modules.

Supported: literal letters and numbers; grouped expressions `{...}`; `_` and `^` scripts;
`\frac{a}{b}`, `\sqrt{x}`, `\mathrm{}`, `\text{}`, `\operatorname{}`;
common Greek symbols including `\alpha`, `\beta`, `\pi`, `\omega`, `\tau`,
`\Sigma`, `\varepsilon`, `\epsilon`, `\phi`, `\Delta`, `\theta`, and more;
mathematical operators `\cdot`, `\times`, `\pm`, `\sum`, `\int`, `\approx`,
`\neq`, `\leq`, `\geq`; arrows such as `\rightarrow` and
`\Longleftrightarrow`; and escaped punctuation such as `\%`. The 19 Equation
widget sources in all 40 shipped Panels are automatically audited against the
native symbol table. Unsupported commands still appear with a literal
backslash diagnostic: this intentionally remains a **limited TeX-style
renderer**, not MathJax or general LaTeX. Use `ui.set(equationId, "...")` to
change a displayed equation at runtime without changing the saved source.

## Experiment Data Table (`type: experiment_table`) — RC1.5.14.5

The Experiment DataTable now uses a native `QAbstractTableModel` and `QTableView`
with a **100,000 retained-row** maximum. Its storage is a bounded deque, not
one `QTableWidgetItem` per cell. Incremental append removes expired oldest
rows and inserts new rows without resetting the whole model. The default
limit remains 250; `maxRows` is a design-time rolling retention capacity.
There may be 1–32 named columns; cell text max 256 characters; the existing
initialRows and schema limits remain. Existing `.wfpanel` documents load
without migration. Full-array `ui.set(id, rows)` still **replaces** rows.

Fast JavaScript update operations (all via `ui.set`):

```javascript
ui.set('dataTable', {append: [index, timeMs, voltage]});
ui.set('dataTable', {appendRows: [[1, 10, 3.3], [2, 20, 3.31]]});
ui.set('dataTable', {setCell: {row: 0, column: 2, value: 3.32}});
ui.set('dataTable', {setRow: {row: 0, values: [1, 10, 3.32]}});
ui.set('dataTable', {clear: true});
```

`appendRows` accepts up to 2048 rows in one call. When capacity is reached,
the oldest retained rows roll off automatically. `ui.tableRowCount(id)`
returns the row count in O(1) without copying any rows; `ui.tableSlice(id,
start, count)` reads up to 2048 retained rows. `ui.get(id)` still returns a
complete native JavaScript array-of-arrays snapshot, but it is O(N) and should
not be called on every timer or network packet. `ui.set()` validates rows and
returns `false` without altering worker state for malformed operations.

Right-click in Run mode to **Import CSV**, **Export CSV**, or **Copy TSV**.
CSV import expects one header row with the same number of columns as the
configured widget; all imported rows replace the current contents. The column
labels remain those defined in the Panel; import does not rewrite `.wfpanel`
properties. CSV import is limited to 32 MiB and the configured retention cap.
Export uses quoted UTF-8 fields and guards against spreadsheet formula
injection. Import and export currently use modal GUI file dialogs; they are
not a high-rate streaming-to-disk interface. Use FileIO/SQLite for durable
acquisition history independent of the table's rolling retention.

If `editable=true`, operator cell edits still fire `valueChanged(rows)` with
all retained rows for compatibility; CSV import does the same. Script appends
never fire that event. This large full-snapshot event should be used sparingly
for huge editable tables. CSV export and `ui.get` return retained rows only.

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

## Fit Page / presentation geometry (v0.2-dev.8.9.5)

Run mode starts with **Fit Page** enabled. The screen display shrinks or
expands *down to* the viewport size so every saved Page is visible without
vertical/horizontal scrolling at ordinary desktop sizes. The layout remains
interactable. The user can turn off Fit Page for **100% resolution and scroll**
when reading lengthy explanations or adjusting precise data. The percent
indicator shows the current presentation scale.

Fit Page is a presentation transform only: it does not change the Page's
logical width/height or any control's `x`, `y`, `width`, `height`, ID, events,
script or data. **Design** mode always returns to 100% logical coordinates.
A newly created Panel starts at 1360 × 620 pixels; existing saved Pages retain
their original size and remain fully editable. This does not reflow text and
very dense teaching Pages may still require 100% viewing for best readability.
