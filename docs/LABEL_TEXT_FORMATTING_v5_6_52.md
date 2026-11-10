# Editing Label text in WFScript v5.6.52

Stop Panel, select a Label on its Page, then open Properties. The **Label text / HTML** field is now a multiline source editor: press Enter for a line break. After changing text, click a different property (or pause briefly) to apply. Save/Export preserves the newlines. Existing projects open unchanged.

Under **Text**:
- **Text Format — Automatic** (default) is the legacy `QLabel::AutoText` behavior. Qt guesses plain or rich text.
- **Plain Text** displays all characters literally, including `<` and `>`; newlines show as separate lines.
- **Rich Text** interprets Qt's supported HTML subset, e.g. `<b>PASS</b><br><span style="color:#3bbd85;">3.300 V</span>`. Enter in the source editor does not automatically insert an HTML `<br>`; write `<br>` explicitly.
- **Word Wrap** wraps paragraphs within the Label width, and defaults to OFF to preserve previous layouts.

Script runtime can set any Label in either format:

```javascript
ui.set('myLabel', 'Line 1\nLine 2');             // Plain Text or Automatic
ui.set('myRichLabel', '<b>PASS</b><br>3.30 V'); // Rich Text
```

Rich Text is not arbitrary webpage HTML or executable JavaScript; it is rendered by Qt QLabel. For safe portable examples prefer self-contained text and simple formatting, not external image or URL resources. A too-small Label may clip the rendered content.

Examples → Getting Started → **Multiline and Rich Text Labels** demonstrates every mode, including a runtime button updating a formatted Label without AD3 hardware.
