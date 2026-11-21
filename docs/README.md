# WFScript Documentation

**Welcome to WFScript v0.2.** Start with the illustrated manuals, explore working examples,
then use the searchable API documentation whenever you need a specific method or property.

## Read online or locally

**[Open the documentation home page](site/index.html)** — 65-page static HTML site,
with an illustrated introduction, 18-chapter User Guide, a searchable JavaScript API index,
and 41 example walkthroughs. Download the repository and open `docs/site/index.html`
locally in your browser; no server or internet connection is needed.

## Illustrated books

| Publication | PDF | Editable Word document |
| --- | --- | --- |
| **Getting Started** — build and run your first Panel | [PDF](manuals/WFScript_Getting_Started.pdf) | [DOCX](manuals/WFScript_Getting_Started.docx) |
| **User Guide** — design, JavaScript, data, charts, instruments and Test Executive | [PDF](manuals/WFScript_User_Guide.pdf) | [DOCX](manuals/WFScript_User_Guide.docx) |

Markdown originals and illustration assets are in [`source/`](source/).
The structured API dataset and specialist reference notes are in [`reference/`](reference/).

## For maintainers and advanced users

- [Windows build instructions](BUILD_WINDOWS.md)
- [Testing and validation](TESTING.md)
- [Software architecture](ARCHITECTURE.md)
- [Panel project file format](PANEL_PROJECT_ARCHITECTURE.md)
- [JavaScript API companion](SCRIPT_REFERENCE.md)
- [Known limitations](KNOWN_LIMITATIONS.md)
- [Licensing / Qt notes](QT_OPEN_SOURCE_NOTES.md)
- [GitHub release procedure](GITHUB_V0_2_PUBLISHING.md)

### Documentation scope

This documentation follows the Windows Qt 6.9.2 RC1.5.14.10.8 application baseline,
which passed 116 of 116 registered native CTests on the maintainer's machine.
Native CTests do not establish exhaustive real-instrument behavior.
The Rigol v0.7.2 synchronized dual-channel and RAW experiment still needs physical verification.
Some screenshots originated during earlier RC development; labels may not match the final release title.
