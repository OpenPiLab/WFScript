#pragma once

#include <QWidget>
#include <QVariant>
#include <deque>
#include <cstdint>
#include <QString>

class QTableView;
class QAbstractTableModel;
class QComboBox;
class QLabel;
class QPushButton;
class QShowEvent;

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
protected:
    void changeEvent(QEvent* event) override;
private:
    void applyTheme();
    static bool parseEntry(const QVariant& value, Entry* out);
    bool appendEvents(const QVariant& payload);
    bool replaceEvents(const QVariantList& entries);
    void trimToCapacity();
    void refresh();                 // full refresh on filter/replace/configuration
    void showEvent(QShowEvent* event) override;
    void exportCsv();
    void copySelected();
    // Constant-time front eviction, even after the log reaches 100k entries.
    // m_firstId is the stable ID of m_entries.front(); m_nextId is the ID
    // assigned to the next append. IDs are never reused on clear/replace.
    std::deque<Entry> m_entries;
    std::uint64_t m_firstId=0;
    std::uint64_t m_nextId=0;
    QString m_title=QStringLiteral("Event Log");
    int m_maxEntries=500;
    bool m_autoScroll=true;
    bool m_showTimestamp=true;
    bool m_showSource=true;
    QLabel* m_heading=nullptr;
    QComboBox* m_filter=nullptr;
    QTableView* m_table=nullptr;
    QAbstractTableModel* m_model=nullptr;
    bool m_viewDirty=false;       // coalesce hidden-Page appends until shown
};
