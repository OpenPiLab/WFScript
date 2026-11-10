# WFScript `.wfpanel` embedded image contract (v5.6.69 and newer)

**The document format has NOT changed in v5.6.69.1.** This is a change to the **image importer policy**, not to the project schema or a migration. The `.wfpanel` root remains `format: "WFScriptPanel"` and `version: 2` for current multipage projects.

An Image / Diagram control has `type: "image"` and uses its existing JSON `properties` keys:

```json
{
  "type": "image",
  "properties": {
    "imageData": "<base64 of JPEG or PNG binary>",
    "imageName": "optional-original-filename.jpg",
    "scaleMode": "Fit",
    "captionVisible": false,
    "borderVisible": true
  }
}
```

The snippet omits the other normal widget fields. `imageData` is a **standard Base64 encoding of complete binary image bytes** (a JPEG file beginning `FF D8 FF` or PNG beginning `89 50 4E 47`). It is not a data URL and has no separate MIME field. Qt detects the image format from its contents. `.wfpanel` itself remains ordinary JSON text, and image filenames do not point to external files.

## Import behavior

- JPEG and PNG files that are **2048 pixels or less in each dimension** and **at most 2 MiB** are **embedded byte-for-byte**, including their existing metadata. They are validated as images first. No recompression, format conversion, or unintended lossy pass occurs. A 39,399-byte JPEG occupies 52,532 Base64 characters, plus a small JSON overhead.
- For oversized JPEG, resize and encode as JPEG. Oversized PNG is resized and encoded as PNG, with a JPEG fallback only for opaque images that cannot fit the payload limit. BMP and WebP are decoded and normalized to PNG or JPEG, subject to the Qt-supported image codecs, dimensions, and size limits.
- The Properties inspector shows **the actual encoded format**, binary size, and Base64 size. It does not infer it from the filename.
- If you embed the original JPEG or PNG, source EXIF metadata (including possible camera/location information) is retained in the project. Remove metadata before importing if that is undesirable.
- Current bounds remain: input file up to 16 MiB, raster up to 2048 px on import, each embedded payload up to 2 MiB, 3 MiB Base64 property limit, 20 MiB cumulative project image data, 32 MiB maximum `.wfpanel` file. Other widget metadata still has its separate 16 KiB limit.

## Backward compatibility

A Panel created using **v5.6.69**, including the earlier enlarged PNG data that came from a JPEG import, **still opens unchanged**, and an ordinary save never transcodes its `imageData`. A previously optimized file containing embedded JPEG already satisfies the same schema and also opens unchanged. The importer will preserve binary bytes when the user imports or replaces an image in v5.6.69.1; **there is no implicit asset conversion or file migration on open/save**.

This change does not affect the AD3 driver, Test Executive, Panel script runtime or reporting. Do not increase the `.wfpanel` schema version to modify importer compression policy. Any future incompatible project-data changes require an explicit versioned schema and migration tests.
