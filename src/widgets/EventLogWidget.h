#pragma once

#include <QWidget>
#include <QVariant>
#include <QVector>
#include <QString>

class QTableWidget;
class QComboBox;
class QLabel;
class QPushButton;

// Panel-facing, bounded, read-only event log. All changes are delivered on the
// GUI thread; script-owned QJSValues must be converted by PanelUiBridge first.
// ui.set(id, {append:{level:'INFO',message:'Ready',source:'Test'}});
// ui.set(id, {append:[{level:'PASS',message:'3.301 V'}, ...]});
// ui.set(id, [{level:'INFO',message:'New log'}]); // replace
// ui.set(id, {clear:true});
// Runtime entries are intentionally NOT persisted with a .wfpanel project.
class EventLogWidget final : public QWidget {
public:
    struct Entry {
        QString timestamp; // Local ISO-8601 text generated at append, or supplied ISO timestamp
        QString level;
        QString source;
        QString message;
    };
    explicit EventLogWidget(QWidget* parent=nullptr);
    void configure(const QString& title, int maxEntries, bool autoScroll,
                   bool showTimestamp, bool showSource);
    bool applyEvents(const QVariant& payload);
    int entryCount() const { return m_entries.size(); }
    QString messageAt(int i) const { return m_entries.at(i).message; }
    QString levelAt(int i) const { return m_entries.at(i).level; }
    void setFilter(const QString& level);
    int visibleEntryCount() const;
    QString csvText() const;
    void clearEvents();
private:
    static bool parseEntry(const QVariant& value, Entry* out);
    bool appendEvents(const QVariant& payload);
    bool replaceEvents(const QVariantList& entries);
    void trimToCapacity();
    void refresh();
    void exportCsv();
    void copySelected();
    QVector<Entry> m_entries;
    QString m_title=QStringLiteral("Event Log");
    int m_maxEntries=500;
    bool m_autoScroll=true;
    bool m_showTimestamp=true;
    bool m_showSource=true;
    QLabel* m_heading=nullptr;
    QComboBox* m_filter=nullptr;
    QTableWidget* m_table=nullptr;
};
