# PanelProject model (v5.6.21)

## Vocabulary
- **Project**: a `.wfpanel` application, containing one main script, shared library sources, runtime settings, and pages.
- **Page**: a group of widgets with an immutable stable ID and human-readable name, independent of scripts.
- **Widget**: a control belonging to one page. Widget IDs are unique across the whole project to keep the existing `ui.set("id", value)` / `ui.on("id", ...)` API unambiguous.
- **Runtime**: one long-lived JavaScript engine per running Project. Page switching will never restart it.

## Responsibilities

`PanelProject.{h,cpp}` (Qt Core only): owns document content, validates it, migrates v1 `controls`/`canvasWidth`/`canvasHeight` to `pages[0]`, atomically saves `.wfpanel` and stores optional lifecycle enablement flags. It knows nothing of QWidgets or AD3 hardware.

`PanelDesigner` + `PanelCanvas`: a graphical view/editor of `PanelProject::mainPage()` in this release. Canvas edits update that page's `controls` and `size`. Editing a second page and switching visible control groups is deliberately reserved for the subsequent Page UI revision.

`MainWindow`: manages file dialogs, transient opened editor tabs, Problems navigation, Run/Stop actions, and transitions between the model and designer. Script and embedded library buffers are now owned by `PanelProject`, not duplicated as independent `MainWindow` fields.

`PanelRuntime`: unchanged. It is passed the project's JavaScript, libraries, and currently visible control values. It remains one QJSEngine per panel. Eventually the Page UI will provide all shared widget values and handle visibility changes through the GUI, not by recreating the engine.

## Existing v1 document compatibility

Old projects like `AD3_Loopback.wfpanel`, `Designer_Edit_Test.wfpanel` and `Embedded_Libraries_Demo.wfpanel` load as one page with ID `main`, title `Main`, original control IDs and dimensions. `PanelProject::saveFile()` writes the same v1 top-level keys and preserves unknown metadata. It rejects >1 page rather than lose widgets. The upcoming Page UI will introduce a v2 schema with a `pages` collection and a legacy v1 import path.

Lifecycle settings can be represented as:

```json
"lifecycle": {
  "initializeEnabled": false,
  "shutdownEnabled": false
}
```

These flags **do not activate any hooks in v5.6.21**. They are model data for the separate lifecycle UI/runtime implementation; default false preserves existing behavior.

## Future Page UI design

Use compact page tabs at the top of the *Panel Designer canvas* (not the outer application's Script/Scope/Panel tabs). Add, rename, reorder and delete page actions manage `PanelProject::pages`. Switch pages by hiding controls from the previous page and showing controls from the new one. Each page has its own saved bounds and widget group. Deleting a page deletes all widgets on that page, after a confirmation. The script and all library objects retain state across navigation.

Implement runtime `ui.showPage("settings")` and `ui.currentPage` only when the multi-page UI and runtime widget routing have been verified. In particular, `ui.get/set` must remain consistent for widgets not presently visible. No page-specific script engine will be created.
