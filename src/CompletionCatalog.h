#pragma once
#include <QString>
#include <QVector>

// Explicit PUBLIC API contract. Never expose arbitrary QObject internals in completion.
namespace CompletionCatalog {
enum class Kind { Object, Function, Number, String, Boolean, Array, Keyword, Snippet };
struct Item { QString name; Kind kind; QString description; };
QVector<Item> entries(const QString& objectPath);
}
