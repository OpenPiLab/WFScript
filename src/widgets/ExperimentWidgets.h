#pragma once
#include "WidgetFactory.h"
#include <QWidget>
#include <QTableView>
#include "ExperimentTableData.h"
#include "ExperimentTableModel.h"
#include <QImage>
#include <QColor>
#include <QVariantList>
#include <utility>

namespace EquationMath {
// Small, documented TeX-like subset; not a general LaTeX engine.
// Renders fractions, roots, groups, scripts, operators and common Greek symbols
// as a transparent image using native Qt painting (no WebEngine/network/files).
QImage render(const QString& source, int pointSize, const QColor& ink);
bool validSource(const QString& source, QString* error=nullptr);
}

class PanelEquationWidget final : public QWidget {
public:
    explicit PanelEquationWidget(QWidget* parent=nullptr);
    void configure(const PanelControl& definition);
    void setExpression(const QString& source);
    QString expression() const {return m_source;}
    QSize sizeHint() const override {return QSize(420,130);}
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QString m_source;
    QString m_heading;
    int m_pointSize=25;
    bool m_border=true;
    bool m_titleVisible=false;
    mutable QImage m_cached;
    mutable QColor m_cachedInk;
    void clearCache();
};

class EquationWidgetFactory final : public WidgetFactory {
public:
    QString typeId() const override {return QStringLiteral("equation");}
    QString displayName() const override {return QStringLiteral("Mathematical Equation");}
    PanelControl defaultDefinition(const QString& id) const override;
    QVector<WidgetProperty> properties() const override;
    QVariant initialRuntimeValue(const PanelControl& d) const override {
        return d.properties.value("source").toString();
    }
    QWidget* create(const PanelControl& d,QWidget* parent,EventSink) const override;
    void applyDefinition(QWidget* widget,const PanelControl& d) const override;
    void readDefinition(QWidget*,PanelControl&) const override {}
    void applyRuntimeValue(QWidget* widget,const PanelControl&,const QVariant& v) const override;
};

class QLabel;
class QPushButton;
class QCheckBox;
class QTimer;
class ExperimentDataTableWidget final : public QWidget {
public:
    using ChangeSink=WidgetFactory::EventSink;
    explicit ExperimentDataTableWidget(QWidget* parent=nullptr);
    void configure(const PanelControl& d);
    bool setRows(const QVariant& rows);
    bool applyValue(const QVariant& value);
    QVariantList rows() const;
    bool exportCsv(const QString& path, QString* error=nullptr) const;
    bool importCsv(const QString& path, QString* error=nullptr);
    void setChangeSink(ChangeSink sink) {m_sink=std::move(sink);}
    ExperimentTableModel* tableModel() const {return m_model;}
    QAbstractItemModel* model() const {return m_model;} // compatibility for native tests
    QTableView* tableView() const {return m_view;}
    int rowCount() const {return m_model->rowCount();}
    int columnCount() const {return m_model->columnCount();}
    QSize sizeHint() const override {return QSize(660,300);}
    bool importCsvAction(const QString& optionalPath={});
    bool exportCsvAction(const QString& optionalPath={});
    void clearFromToolbar();
    void setTableTitle(const QString& text);
    void setTableStatus(const QString& text);
    void setAutoScroll(bool enabled);
protected:
    void changeEvent(QEvent* event) override;
private:
    void applyTheme();
    void updateCount();
    void scheduleCount();
    void publishSelection();
    void copySelection();
    ExperimentTableModel* m_model=nullptr;
    QTableView* m_view=nullptr;
    QWidget* m_toolbar=nullptr;
    QLabel* m_heading=nullptr;
    QLabel* m_count=nullptr;
    QLabel* m_status=nullptr;
    QPushButton* m_importButton=nullptr;
    QPushButton* m_exportButton=nullptr;
    QPushButton* m_copyButton=nullptr;
    QPushButton* m_clearButton=nullptr;
    QCheckBox* m_follow=nullptr;
    QTimer* m_countTimer=nullptr;
    bool m_autoScroll=true;
    int m_maxRows=ExperimentTableLimits::DefaultRows;
    bool m_editable=false;
    bool m_emitFullChange=false;
    QStringList m_columns;
    QString m_initialSource;
    ChangeSink m_sink;
};
class ExperimentDataTableFactory final : public WidgetFactory {
public:
    QString typeId() const override {return QStringLiteral("experiment_table");}
    QString displayName() const override {return QStringLiteral("Experiment Data Table");}
    PanelControl defaultDefinition(const QString& id) const override;
    QVector<WidgetProperty> properties() const override;
    QVector<PanelEventDefinition> events() const override;
    QVariant initialRuntimeValue(const PanelControl& d) const override;
    QWidget* create(const PanelControl& d,QWidget* parent,EventSink sink) const override;
    void applyDefinition(QWidget* widget,const PanelControl& d) const override;
    void readDefinition(QWidget*,PanelControl&) const override {}
    void applyRuntimeValue(QWidget* widget,const PanelControl&,const QVariant& v) const override;
};
