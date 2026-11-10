#pragma once
#include <QTableWidget>
#include <QVariant>
#include <QVariantList>

// Bounded, read-only table for scripted test measurements.
// ui.set(tableId, [{name,value,unit,low,high},...]) replaces all visible rows.
class MeasurementTableWidget final : public QTableWidget {
public:
    explicit MeasurementTableWidget(QWidget* parent=nullptr);
    void configure(int decimals,int maxRows,const QString& headers);
    bool setMeasurements(const QVariant& rows);
    int displayedRows() const {return rowCount();}
private:
    int m_decimals=3;
    int m_maxRows=100;
    QString m_headers=QStringLiteral("Measurement|Actual|Limits|Result");
};
