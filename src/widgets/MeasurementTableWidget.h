#pragma once
#include <QWidget>
#include <QVariant>
#include <QString>
class QAbstractItemModel;
class QAbstractTableModel;
class QTableView;
class QHeaderView;
class QScrollBar;
class QLabel;
class QComboBox;
class QPushButton;
class QTimer;
class QSortFilterProxyModel;

// RC1.5.14.8: compact toolbar around the existing 100k-row virtual view.
// The source model and TestRunner append/reset contract are unchanged.
class MeasurementTableWidget final : public QWidget {
public:
    explicit MeasurementTableWidget(QWidget* parent=nullptr);
    void configure(int decimals,int maxRows,const QString& headers,
                   const QString& title=QStringLiteral("Measurement results"),
                   bool showToolbar=true,bool showRowCount=true,bool showFilter=true,
                   bool showCopy=true,bool showExport=true,bool showClear=false,
                   bool autoScroll=true);
    bool setMeasurements(const QVariant& rows);
    int displayedRows() const;
    int rowCount() const {return displayedRows();}
    // Compatibility with old native tests and other C++ callers. `model()`
    // always exposes the unfiltered source, not the temporary filter view.
    QAbstractItemModel* model() const;
    QTableView* tableView() const {return m_view;}
    QHeaderView* horizontalHeader() const;
    QScrollBar* verticalScrollBar() const;
    void scrollToBottom();
    void setResultFilter(const QString& status);
    QString resultFilter() const;
    QString tableTitle() const;
    QString countText() const;
    bool exportCsv(const QString& path,QString* error=nullptr) const;
protected:
    void changeEvent(QEvent* event) override;
private:
    void applyTheme();
    void refreshCount();
    void copySelection();
    void clearView();
    void exportDialog();
    int m_decimals=3;
    int m_legacyRows=100; // old design property; NOT a retention limit
    QString m_headers=QStringLiteral("Measurement|Actual|Limits|Result");
    bool m_autoScroll=true;
    QTableView* m_view=nullptr;
    QAbstractTableModel* m_source=nullptr;
    QSortFilterProxyModel* m_proxy=nullptr;
    QLabel* m_heading=nullptr;
    QLabel* m_count=nullptr;
    QComboBox* m_filter=nullptr;
    QPushButton* m_copy=nullptr;
    QPushButton* m_export=nullptr;
    QPushButton* m_clear=nullptr;
    QWidget* m_toolbar=nullptr;
    QTimer* m_countTimer=nullptr;
};
