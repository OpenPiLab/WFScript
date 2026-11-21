#pragma once
#include <QtGlobal>
// Conservative bounded expansion for reusable JavaScript packages.
// Keep editor/import, runtime, schema validation, and save/load in sync.
namespace ScriptSizeLimits {
inline constexpr qint64 LibraryFileBytes = 8LL * 1024 * 1024;
inline constexpr qint64 EmbeddedLibraryBytes = 24LL * 1024 * 1024;
inline constexpr qint64 MainScriptBytes = 8LL * 1024 * 1024;
inline constexpr qint64 PanelFileBytes = 96LL * 1024 * 1024;
inline constexpr int EmbeddedLibraryCount = 64;
}
