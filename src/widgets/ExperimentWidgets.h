#pragma once
#include "WidgetFactory.h"
#include <QWidget>
#include <QTableWidget>
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

class ExperimentDataTableWidget final : public QTableWidget {
public:
    using ChangeSink=WidgetFactory::EventSink;
    explicit ExperimentDataTableWidget(QWidget* parent=nullptr);
    void configure(const PanelControl& d);
    bool setRows(const QVariant& rows);
    QVariantList rows() const;
    bool exportCsv(const QString& path, QString* error=nullptr) const;
    void setChangeSink(ChangeSink sink) {m_sink=std::move(sink);}
    QSize sizeHint() const override {return QSize(660,270);}
private:
    int m_maxRows=250;
    bool m_editable=false;
    bool m_busy=false;
    QStringList m_columns;
    QString m_initialSource; // detect design-time Initial rows edits
    ChangeSink m_sink;
    void installRows(const QVariantList& rows);
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
