#include "ExperimentWidgets.h"
#include <QPainter>
#include <QFontMetricsF>
#include <QPaintEvent>
#include <QPalette>
#include <QHash>
#include <QMenu>
#include <QClipboard>
#include <QApplication>
#include <QFileDialog>
#include <QSaveFile>
#include <QTextStream>
#include <QHeaderView>
#include <QSignalBlocker>
#include <QRegularExpression>
#include <QtGlobal>
#include <QDebug>
#include <cmath>
#include <utility>

namespace {
bool lightMode(const QWidget* widget) {
    for(const QWidget* p=widget;p;p=p->parentWidget())
        if(p->property("wfPanelLight").isValid())return p->property("wfPanelLight").toBool();
    return widget->palette().color(QPalette::Window).lightness()>=130;
}
struct MathTile { QImage bitmap; int baseline=0; int width() const {return bitmap.width();} int height() const {return bitmap.height();} };
QImage blank(int w,int h) {
    QImage image(qBound(1,w,4096),qBound(1,h,1024),QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);return image;
}
MathTile glyph(const QString& text,int px,const QColor& ink) {
    QFont font(QStringLiteral("Cambria Math"));font.setPixelSize(qBound(10,px,144));
    QFontMetricsF metrics(font);
    int w=qBound(1,int(std::ceil(metrics.horizontalAdvance(text)))+3,4096);
    int h=qBound(1,int(std::ceil(metrics.height()))+4,1024);
    MathTile tile{blank(w,h),int(std::ceil(metrics.ascent()))+2};
    QPainter painter(&tile.bitmap);painter.setRenderHint(QPainter::TextAntialiasing,true);
    painter.setFont(font);painter.setPen(ink);painter.drawText(QPointF(1,tile.baseline),text);
    return tile;
}
MathTile horizontal(const QVector<MathTile>& tiles) {
    if(tiles.isEmpty())return glyph("",14,Qt::white);
    int above=0,below=0,width=0;
    for(const auto& t:tiles) {above=qMax(above,t.baseline);below=qMax(below,t.height()-t.baseline);width+=t.width();}
    MathTile result{blank(width,above+below),above};
    QPainter p(&result.bitmap);int x=0;
    for(const auto& t:tiles){p.drawImage(x,above-t.baseline,t.bitmap);x+=t.width();}
    return result;
}
MathTile fraction(const MathTile& numerator,const MathTile& denominator,const QColor& ink) {
    const int w=qMin(4096,qMax(numerator.width(),denominator.width())+14);
    const int ruleY=numerator.height()+4;
    MathTile tile{blank(w,numerator.height()+denominator.height()+12),ruleY+2};
    QPainter p(&tile.bitmap);
    p.drawImage((w-numerator.width())/2,0,numerator.bitmap);
    p.setPen(QPen(ink,1.5));p.drawLine(2,ruleY,w-3,ruleY);
    p.drawImage((w-denominator.width())/2,ruleY+6,denominator.bitmap);
    return tile;
}
MathTile sqrtTile(const MathTile& arg,const QColor& ink,int px) {
    MathTile root=glyph(QString::fromUtf8("√"),px+2,ink);
    MathTile tile{blank(root.width()+arg.width()+4,qMax(root.height(),arg.height()+5)),qMax(root.baseline,arg.baseline+3)};
    QPainter p(&tile.bitmap);p.drawImage(0,tile.baseline-root.baseline,root.bitmap);
    p.drawImage(root.width()+3,tile.baseline-arg.baseline,arg.bitmap);
    p.setPen(QPen(ink,1.2));p.drawLine(root.width()+1,1,tile.width()-1,1);
    return tile;
}
MathTile scripted(const MathTile& base,const MathTile& sub,const MathTile& superscript) {
    const bool hasSub=!sub.bitmap.isNull();const bool hasSup=!superscript.bitmap.isNull();
    int up=hasSup?qMax(0,superscript.height()-base.baseline/2):0;
    int down=hasSub?qMax(0,sub.height()-(base.height()-base.baseline)/2):0;
    int w=base.width()+qMax(hasSup?superscript.width():0,hasSub?sub.width():0)+2;
    MathTile tile{blank(w,base.height()+up+down),base.baseline+up};
    QPainter p(&tile.bitmap);p.drawImage(0,up,base.bitmap);
    if(hasSup)p.drawImage(base.width()+1,qMax(0,tile.baseline-base.baseline/2-superscript.height()),superscript.bitmap);
    if(hasSub)p.drawImage(base.width()+1,tile.baseline+base.height()-base.baseline/2-sub.baseline,sub.bitmap);
    return tile;
}
QString special(const QString& name) {
    static const QHash<QString,QString> mapping={
        {"alpha",QString::fromUtf8("α")},{"beta",QString::fromUtf8("β")},{"gamma",QString::fromUtf8("γ")},
        {"delta",QString::fromUtf8("δ")},{"Delta",QString::fromUtf8("Δ")},{"epsilon",QString::fromUtf8("ε")},
        {"theta",QString::fromUtf8("θ")},{"lambda",QString::fromUtf8("λ")},{"mu",QString::fromUtf8("μ")},
        {"pi",QString::fromUtf8("π")},{"sigma",QString::fromUtf8("σ")},{"omega",QString::fromUtf8("ω")},
        {"Omega",QString::fromUtf8("Ω")},{"tau",QString::fromUtf8("τ")},{"phi",QString::fromUtf8("φ")},
        {"infty",QString::fromUtf8("∞")},{"cdot",QString::fromUtf8("·")},{"times",QString::fromUtf8("×")},
        {"pm",QString::fromUtf8("±")},{"leq",QString::fromUtf8("≤")},{"geq",QString::fromUtf8("≥")},
        {"approx",QString::fromUtf8("≈")},{"neq",QString::fromUtf8("≠")},{"rightarrow",QString::fromUtf8("→")},
        {"degree",QString::fromUtf8("°")},{"partial",QString::fromUtf8("∂")},
        {"sum",QString::fromUtf8("∑")},{"int",QString::fromUtf8("∫")},
        {"sin","sin"},{"cos","cos"},{"tan","tan"},{"log","log"},{"ln","ln"},
        {"left",""},{"right",""},{"quad","  "},{"qquad","    "},{","," "},{";"," "}
    };
    return mapping.value(name,QStringLiteral("\\")+name);
}
class MathParser {
public:
    MathParser(QString text,int fontSize,QColor color):m_text(std::move(text)),m_px(fontSize),m_ink(std::move(color)){}
    MathTile parse() {return sequence(0);}
private:
    QString m_text;int m_pos=0;int m_px;QColor m_ink;
    MathTile grouped(int size,int depth) {
        if(m_pos<m_text.size() && m_text.at(m_pos)==QLatin1Char('{')){
            ++m_pos;MathTile t=sequence(depth+1,size);
            if(m_pos<m_text.size() && m_text.at(m_pos)==QLatin1Char('}'))++m_pos;
            return t;
        }
        return atom(depth+1,size);
    }
    MathTile atom(int depth,int size) {
        if(m_pos>=m_text.size())return glyph("",size,m_ink);
        const QChar c=m_text.at(m_pos++);
        if(c==QLatin1Char('{')) {
            MathTile t=sequence(depth+1,size);
            if(m_pos<m_text.size() && m_text.at(m_pos)==QLatin1Char('}'))++m_pos;
            return t;
        }
        if(c==QLatin1Char('\\')) {
            QString command;
            while(m_pos<m_text.size() && m_text.at(m_pos).isLetter())command+=m_text.at(m_pos++);
            if(command.isEmpty() && m_pos<m_text.size())command+=m_text.at(m_pos++);
            if(command=="frac" || command=="dfrac" || command=="tfrac") {
                const MathTile top=grouped(qMax(10,size*4/5),depth+1);
                const MathTile bottom=grouped(qMax(10,size*4/5),depth+1);
                return fraction(top,bottom,m_ink);
            }
            if(command=="sqrt")return sqrtTile(grouped(size,depth+1),m_ink,size);
            if(command=="mathrm" || command=="text" || command=="operatorname")return grouped(size,depth+1);
            return glyph(special(command),size,m_ink);
        }
        return glyph(QString(c),size,m_ink);
    }
    MathTile sequence(int depth,int size=0) {
        if(size==0)size=m_px;
        if(depth>12)return glyph("[nesting limit]",size,m_ink);
        QVector<MathTile> pieces;
        while(m_pos<m_text.size()) {
            QChar c=m_text.at(m_pos);
            if(c==QLatin1Char('}'))break;
            if(c.isSpace()) {++m_pos;pieces.append(glyph(" ",size,m_ink));continue;}
            MathTile base=atom(depth,size);
            MathTile super,sub;
            while(m_pos<m_text.size() && (m_text.at(m_pos)==QLatin1Char('^') || m_text.at(m_pos)==QLatin1Char('_'))) {
                QChar op=m_text.at(m_pos++);
                MathTile small=grouped(qMax(10,size*65/100),depth+1);
                if(op==QLatin1Char('^'))super=std::move(small);else sub=std::move(small);
            }
            if(!super.bitmap.isNull() || !sub.bitmap.isNull())base=scripted(base,sub,super);
            pieces.append(std::move(base));
            if(pieces.size()>2048)break;
        }
        return horizontal(pieces);
    }
};
QStringList parseColumns(const QString& text) {
    QStringList parts=text.split('|',Qt::KeepEmptyParts);
    for(auto& p:parts)p=p.trimmed();
    return parts;
}
QVariantList initialTableRows(const QString& value, int columns,int limit) {
    QVariantList rows;
    for(const QString& line:value.split('\n',Qt::SkipEmptyParts)) {
        const QStringList cells=line.split('|',Qt::KeepEmptyParts);
        QVariantList row;
        for(int i=0;i<columns;++i)row.append(cells.value(i).trimmed());
        rows.append(QVariant(row)); // Preserve the row as ONE nested QVariantList.
        if(rows.size()>=limit)break;
    }
    return rows;
}
QString escapedCsv(QString text) {
    // Defuse spreadsheet formulas while preserving signed numeric measurements.
    static const QRegularExpression number(QStringLiteral("^[+-]?(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+)(?:[eE][+-]?[0-9]+)?$"));
    const QString trimmed=text.trimmed();
    const bool dangerous=!trimmed.isEmpty() &&
        QStringLiteral("=+-@\t\r").contains(trimmed.at(0)) &&
        !number.match(trimmed).hasMatch();
    if(dangerous)text.prepend('\'');
    text.replace('"',QStringLiteral("\"\""));
    return QStringLiteral("\"")+text+QStringLiteral("\"");
}
}
namespace EquationMath {
bool validSource(const QString& source,QString* error) {
    if(source.size()>2048 || source.contains(QChar::Null)){
        if(error)*error=QStringLiteral("Equation must be at most 2048 characters, with no NUL character.");
        return false;
    }
    int braces=0;
    for(int i=0;i<source.size();++i){
        if(source.at(i)==QLatin1Char('\\')) { // escaped braces and supported commands
            if(i+1<source.size() && (source.at(i+1)=='{' || source.at(i+1)=='}')) ++i;
            continue;
        }
        if(source.at(i)==QLatin1Char('{')){if(++braces>12)break;}
        else if(source.at(i)==QLatin1Char('}')){if(--braces<0)break;}
    }
    if(braces!=0){if(error)*error=QStringLiteral("Equation braces must be paired and nesting must not exceed 12.");return false;}
    return true;
}
QImage render(const QString& source,int pointSize,const QColor& ink) {
    if(!validSource(source))return glyph("Invalid equation",18,ink).bitmap;
    return MathParser(source,qBound(12,pointSize,120),ink).parse().bitmap;
}
}
PanelEquationWidget::PanelEquationWidget(QWidget* parent):QWidget(parent) {
    setObjectName(QStringLiteral("panelEquationWidget"));
}
void PanelEquationWidget::clearCache(){m_cached=QImage{};update();}
void PanelEquationWidget::configure(const PanelControl& d) {
    m_heading=d.text;
    m_source=d.properties.value("source").toString(QStringLiteral("f_c=\\frac{1}{2\\pi RC}"));
    m_pointSize=d.properties.value("pointSize").toInt(25);
    m_border=d.properties.value("borderVisible").toBool(true);
    m_titleVisible=d.properties.value("titleVisible").toBool(false);
    setToolTip(QStringLiteral("Equation (limited LaTeX-style syntax): ")+m_source);
    clearCache();
}
void PanelEquationWidget::setExpression(const QString& source) {
    if(!EquationMath::validSource(source))return;
    m_source=source;clearCache();
}
void PanelEquationWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setRenderHint(QPainter::SmoothPixmapTransform);
    const bool light=lightMode(this);
    const QColor ink=light?QColor("#233c52"):QColor("#edf5ff");
    p.fillRect(rect(),light?QColor("#f7fafc"):QColor("#182d41"));
    if(m_border){p.setPen(light?QColor("#afc4d2"):QColor("#4c687f"));p.drawRect(rect().adjusted(0,0,-1,-1));}
    int top=8;
    if(m_titleVisible){p.setPen(ink);QFont f=p.font();f.setBold(true);p.setFont(f);p.drawText(QRect(10,5,width()-20,22),Qt::AlignLeft|Qt::AlignVCenter,m_heading);top=32;}
    // Render at 2x resolution, then display at the *configured* font size.
    // In v5.6.70 the bitmap was always scaled to fill the available rectangle:
    // that upscaled small equations, causing blur and making Font size ineffective.
    constexpr qreal qualityScale = 2.0;
    if(m_cached.isNull() || m_cachedInk!=ink) {
        m_cached=EquationMath::render(m_source,m_pointSize*int(qualityScale),ink);
        m_cachedInk=ink;
    }
    const QRectF available=QRectF(rect().adjusted(12,top,-12,-10));
    if(available.width()<=0 || available.height()<=0 || m_cached.isNull())return;
    const QSizeF natural(m_cached.width()/qualityScale,m_cached.height()/qualityScale);
    // Never upscale to fill the widget. Only shrink if the actual equation
    // cannot fit. Enlarging the widget adds whitespace, not blurry glyphs.
    const qreal scale=qMin(1.0,qMin(available.width()/natural.width(),available.height()/natural.height()));
    const QSizeF displayed(natural.width()*scale,natural.height()*scale);
    const QRectF target(available.center().x()-displayed.width()/2.0,
                        available.center().y()-displayed.height()/2.0,
                        displayed.width(),displayed.height());
    p.drawImage(target,m_cached,QRectF(m_cached.rect()));
}
PanelControl EquationWidgetFactory::defaultDefinition(const QString& id) const {
    auto d=WidgetFactory::defaultDefinition(id);d.geometry.setSize(QSize(425,135));
    d.text=QStringLiteral("RC cutoff frequency");
    d.properties={{"source",QStringLiteral("f_c=\\frac{1}{2\\pi RC}")},{"pointSize",25},{"borderVisible",true},{"titleVisible",false}};
    return d;
}
QVector<WidgetProperty> EquationWidgetFactory::properties() const {
    WidgetProperty source{"source","Equation source (LaTeX subset)","multiline",true};source.category="Equation";
    WidgetProperty title{"text","Optional heading","string",true};title.category="Equation";
    WidgetProperty size{"pointSize","Font size","integer",true,12,48};size.defaultValue=25;size.category="Appearance";
    WidgetProperty header{"titleVisible","Show heading","boolean",true};header.defaultValue=false;header.category="Appearance";
    WidgetProperty border{"borderVisible","Show border","boolean",true};border.defaultValue=true;border.category="Appearance";
    return {source,title,size,header,border};
}
QWidget* EquationWidgetFactory::create(const PanelControl& d,QWidget* parent,EventSink) const {
    auto* w=new PanelEquationWidget(parent);w->configure(d);return w;
}
void EquationWidgetFactory::applyDefinition(QWidget* widget,const PanelControl& d) const {
    if(auto* w=dynamic_cast<PanelEquationWidget*>(widget))w->configure(d);
}
void EquationWidgetFactory::applyRuntimeValue(QWidget* widget,const PanelControl&,const QVariant& value) const {
    if(auto* w=dynamic_cast<PanelEquationWidget*>(widget))w->setExpression(value.toString());
}
ExperimentDataTableWidget::ExperimentDataTableWidget(QWidget* parent):QTableWidget(parent) {
    setObjectName(QStringLiteral("experimentDataTableWidget"));
    setAlternatingRowColors(true);
    setSelectionBehavior(QAbstractItemView::SelectItems);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    horizontalHeader()->setStretchLastSection(true);
    verticalHeader()->setDefaultSectionSize(25);
    setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(this,&QWidget::customContextMenuRequested,this,[this](const QPoint& pos){
        QMenu menu(this);
        QAction* csv=menu.addAction(QStringLiteral("Export CSV..."));
        QAction* copy=menu.addAction(QStringLiteral("Copy table as TSV"));
        QAction* choice=menu.exec(mapToGlobal(pos));
        if(choice==csv) {
            QString path=QFileDialog::getSaveFileName(this,QStringLiteral("Export experiment data"),QStringLiteral("experiment.csv"),QStringLiteral("CSV files (*.csv)"));
            if(path.isEmpty())return;
            QString error;if(!exportCsv(path,&error))qWarning()<<"Experiment CSV export:"<<error;
        } else if(choice==copy){
            QStringList lines;lines.append(m_columns.join('\t'));
            for(const QVariant& r:rows()){
                QStringList cells;for(const QVariant& c:r.toList())cells<<c.toString();lines<<cells.join('\t');
            }
            QApplication::clipboard()->setText(lines.join('\n'));
        }
    });
    QObject::connect(this,&QTableWidget::itemChanged,this,[this](QTableWidgetItem*) {
        if(!m_busy && m_editable && m_sink)m_sink("valueChanged",rows());
    });
}
void ExperimentDataTableWidget::configure(const PanelControl& d) {
    const QStringList names=parseColumns(d.properties.value("columns").toString(QStringLiteral("Frequency (Hz)|Predicted|Measured|Error (%)")));
    if(names.size()<1 || names.size()>8)return;
    const QString initialSource=d.properties.value("initialRows").toString();
    const bool initialChanged=initialSource!=m_initialSource;
    const bool columnsChanged=names!=m_columns;
    m_busy=true;
    m_columns=names;
    m_initialSource=initialSource;
    m_maxRows=qBound(1,d.properties.value("maxRows").toInt(250),250);
    m_editable=d.properties.value("editable").toBool(false);
    setColumnCount(m_columns.size());setHorizontalHeaderLabels(m_columns);
    setEditTriggers(m_editable?(QAbstractItemView::DoubleClicked|QAbstractItemView::EditKeyPressed):QAbstractItemView::NoEditTriggers);
    setStyleSheet(lightMode(this)?
        QStringLiteral("QTableWidget {background:#ffffff;alternate-background-color:#eff5f9;color:#15283b;gridline-color:#ccdce7;} QHeaderView::section {background:#dce7ef;color:#23374a;padding:4px;border:1px solid #cad7e2;}"):
        QStringLiteral("QTableWidget {background:#142638;alternate-background-color:#203449;color:#eaf6ff;gridline-color:#415b75;selection-background-color:#286a91;} QHeaderView::section {background:#29445f;color:#eaf6ff;padding:4px;border:1px solid #395773;}"));
    m_busy=false;
    if(rowCount()==0 || initialChanged || columnsChanged) {
        // Design-time edits of Initial rows/headers must update the visible table.
        // Otherwise preserve script/runtime values when only appearance changes.
        setRows(initialTableRows(initialSource,m_columns.size(),m_maxRows));
    } else {
        // Preserve live rows when changing only appearance.
        for(int r=0;r<rowCount();++r)for(int c=0;c<columnCount();++c)if(!item(r,c))setItem(r,c,new QTableWidgetItem());
    }
}
void ExperimentDataTableWidget::installRows(const QVariantList& data) {
    m_busy=true;
    setRowCount(data.size());
    for(int r=0;r<data.size();++r) {
        QVariantList cells=data.at(r).toList();
        for(int c=0;c<m_columns.size();++c){
            auto* item=new QTableWidgetItem(cells.value(c).toString());
            Qt::ItemFlags flags=Qt::ItemIsEnabled|Qt::ItemIsSelectable;
            if(m_editable)flags|=Qt::ItemIsEditable;
            item->setFlags(flags);
            setItem(r,c,item);
        }
    }
    m_busy=false;
}
bool ExperimentDataTableWidget::setRows(const QVariant& input) {
    if(!input.canConvert<QVariantList>())return false;
    const QVariantList data=input.toList();
    if(data.size()>m_maxRows)return false;
    QVariantList normalized;
    for(const QVariant& entry:data) {
        if(!entry.canConvert<QVariantList>())return false;
        const QVariantList cells=entry.toList();
        if(cells.size()!=m_columns.size())return false;
        QVariantList row;
        for(const QVariant& cell:cells) {
            const QString s=cell.toString();
            if(s.size()>256 || s.contains(QChar::Null))return false;
            row<<s;
        }
        normalized.append(QVariant(row)); // Do not flatten the cells into outer rows.
    }
    installRows(normalized);return true;
}
QVariantList ExperimentDataTableWidget::rows() const {
    QVariantList result;
    for(int r=0;r<rowCount();++r) {
        QVariantList row;
        for(int c=0;c<columnCount();++c)row<<(item(r,c)?item(r,c)->text():QString());
        result.append(QVariant(row)); // ui.get()/valueChanged must return row arrays.
    }
    return result;
}
bool ExperimentDataTableWidget::exportCsv(const QString& path, QString* error) const {
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly|QIODevice::Text)){
        if (error) *error = file.errorString();
        return false;
    }
    QTextStream stream(&file);
    QStringList header;for(const QString& cell:m_columns)header<<escapedCsv(cell);
    stream<<header.join(',')<<'\n';
    for(const QVariant& r:rows()) {
        QStringList cells;for(const QVariant& cell:r.toList())cells<<escapedCsv(cell.toString());
        stream<<cells.join(',')<<'\n';
    }
    stream.flush();
    if(stream.status()!=QTextStream::Ok || !file.commit()){
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}
PanelControl ExperimentDataTableFactory::defaultDefinition(const QString& id) const {
    auto d=WidgetFactory::defaultDefinition(id);d.geometry.setSize(QSize(700,270));d.text="Experiment observations";
    d.properties={{"columns","Frequency (Hz)|Predicted|Measured|Error (%)"},{"initialRows","100|0.998||\n500|0.954||\n1000|0.847||\n1592|0.707||\n5000|0.303||"},{"maxRows",250},{"editable",false}};
    return d;
}
QVector<WidgetProperty> ExperimentDataTableFactory::properties() const {
    WidgetProperty cols{"columns","Column headers (separate with |)","string",true};cols.category="Table";
    WidgetProperty initial{"initialRows","Initial rows (one per line, | separated)","multiline",true};initial.category="Table";
    WidgetProperty max{"maxRows","Maximum rows","integer",true,1,250};max.defaultValue=250;max.category="Table";
    WidgetProperty edit{"editable","Allow user cell editing","boolean",true};edit.defaultValue=false;edit.category="Table";
    return {cols,initial,max,edit};
}
QVector<PanelEventDefinition> ExperimentDataTableFactory::events() const {
    return {{"valueChanged","valueChanged(rows: array)","Fires after an operator edits a cell; rows is an array of arrays.","rows"}};
}
QVariant ExperimentDataTableFactory::initialRuntimeValue(const PanelControl& d) const {
    const int cols=parseColumns(d.properties.value("columns").toString()).size();
    return initialTableRows(d.properties.value("initialRows").toString(),cols,qBound(1,d.properties.value("maxRows").toInt(250),250));
}
QWidget* ExperimentDataTableFactory::create(const PanelControl& d,QWidget* parent,EventSink sink) const {
    auto* table=new ExperimentDataTableWidget(parent);table->setChangeSink(std::move(sink));table->configure(d);return table;
}
void ExperimentDataTableFactory::applyDefinition(QWidget* widget,const PanelControl& d) const {
    if(auto* t=dynamic_cast<ExperimentDataTableWidget*>(widget))t->configure(d);
}
void ExperimentDataTableFactory::applyRuntimeValue(QWidget* widget,const PanelControl&,const QVariant& data) const {
    if(auto* t=dynamic_cast<ExperimentDataTableWidget*>(widget))
        if(!t->setRows(data))qWarning()<<"Experiment Data Table rejected ui.set payload"<<t->objectName();
}
