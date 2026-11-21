#include "ExperimentWidgets.h"
#include "PanelThemedCheckBox.h"
#include <QEvent>
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
#include <QMessageBox>
#include <QScrollBar>
#include <QPersistentModelIndex>
#include <QFileInfo>
#include <QTextStream>
#include <QHeaderView>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QTimer>
#include <QItemSelectionModel>
#include <QAbstractItemView>
#include <QSet>
#include <QMap>
#include <climits>
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
// This is a deliberately limited TeX-style renderer, not a full LaTeX engine.
// Keep the command table explicit: introductory engineering notation must NEVER
// display a literal \Sigma or \varepsilon as if it were part of the formula.
QString special(const QString& name) {
    static const QHash<QString, QString> mapping = {
        {"alpha", QString::fromUtf8("α")}, {"beta", QString::fromUtf8("β")},
        {"gamma", QString::fromUtf8("γ")}, {"Gamma", QString::fromUtf8("Γ")},
        {"delta", QString::fromUtf8("δ")}, {"Delta", QString::fromUtf8("Δ")},
        {"epsilon", QString::fromUtf8("ε")}, {"varepsilon", QString::fromUtf8("ϵ")},
        {"zeta", QString::fromUtf8("ζ")}, {"eta", QString::fromUtf8("η")},
        {"theta", QString::fromUtf8("θ")}, {"vartheta", QString::fromUtf8("ϑ")},
        {"Theta", QString::fromUtf8("Θ")}, {"iota", QString::fromUtf8("ι")},
        {"kappa", QString::fromUtf8("κ")}, {"lambda", QString::fromUtf8("λ")},
        {"Lambda", QString::fromUtf8("Λ")}, {"mu", QString::fromUtf8("μ")},
        {"nu", QString::fromUtf8("ν")}, {"xi", QString::fromUtf8("ξ")},
        {"Xi", QString::fromUtf8("Ξ")}, {"pi", QString::fromUtf8("π")},
        {"Pi", QString::fromUtf8("Π")}, {"rho", QString::fromUtf8("ρ")},
        {"varrho", QString::fromUtf8("ϱ")}, {"sigma", QString::fromUtf8("σ")},
        {"Sigma", QString::fromUtf8("Σ")}, {"varsigma", QString::fromUtf8("ς")},
        {"tau", QString::fromUtf8("τ")}, {"upsilon", QString::fromUtf8("υ")},
        {"Upsilon", QString::fromUtf8("Υ")}, {"phi", QString::fromUtf8("φ")},
        {"varphi", QString::fromUtf8("ϕ")}, {"Phi", QString::fromUtf8("Φ")},
        {"chi", QString::fromUtf8("χ")}, {"psi", QString::fromUtf8("ψ")},
        {"Psi", QString::fromUtf8("Ψ")}, {"omega", QString::fromUtf8("ω")},
        {"Omega", QString::fromUtf8("Ω")}, {"ohm", QString::fromUtf8("Ω")},
        {"infty", QString::fromUtf8("∞")}, {"cdot", QString::fromUtf8("·")},
        {"times", QString::fromUtf8("×")}, {"div", QString::fromUtf8("÷")},
        {"pm", QString::fromUtf8("±")}, {"mp", QString::fromUtf8("∓")},
        {"leq", QString::fromUtf8("≤")}, {"geq", QString::fromUtf8("≥")},
        {"approx", QString::fromUtf8("≈")}, {"sim", QString::fromUtf8("∼")},
        {"equiv", QString::fromUtf8("≡")}, {"neq", QString::fromUtf8("≠")},
        {"propto", QString::fromUtf8("∝")}, {"partial", QString::fromUtf8("∂")},
        {"nabla", QString::fromUtf8("∇")}, {"sum", QString::fromUtf8("∑")},
        {"prod", QString::fromUtf8("∏")}, {"int", QString::fromUtf8("∫")},
        {"rightarrow", QString::fromUtf8("→")}, {"to", QString::fromUtf8("→")},
        {"leftarrow", QString::fromUtf8("←")}, {"leftrightarrow", QString::fromUtf8("↔")},
        {"Rightarrow", QString::fromUtf8("⇒")}, {"Leftarrow", QString::fromUtf8("⇐")},
        {"Leftrightarrow", QString::fromUtf8("⇔")},
        {"Longleftrightarrow", QString::fromUtf8("⟺")},
        {"angle", QString::fromUtf8("∠")}, {"degree", QString::fromUtf8("°")},
        {"ldots", QString::fromUtf8("…")}, {"cdots", QString::fromUtf8("⋯")},
        {"sin", "sin"}, {"cos", "cos"}, {"tan", "tan"},
        {"log", "log"}, {"ln", "ln"}, {"exp", "exp"},
        {"left", ""}, {"right", ""},
        {"quad", "  "}, {"qquad", "    "},
        {",", " "}, {";", " "}, {" ", " "}, {"!", ""},
        {"%", "%"}, {"{", "{"}, {"}", "}"}, {"_", "_"},
        {"$", "$"}, {"#", "#"}, {"&", "&"}
    };
    // Retain a visible diagnostic for truly unsupported commands instead of
    // silently dropping part of an equation.
    return mapping.value(name, QStringLiteral("\\") + name);
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
ExperimentDataTableWidget::ExperimentDataTableWidget(QWidget* parent):QWidget(parent) {
    setObjectName(QStringLiteral("experimentDataTableWidget"));
    auto* layout=new QVBoxLayout(this);
    layout->setContentsMargins(5,5,5,5);layout->setSpacing(4);
    m_toolbar=new QWidget(this);m_toolbar->setObjectName(QStringLiteral("experimentTableToolbar"));
    auto* bar=new QHBoxLayout(m_toolbar);bar->setContentsMargins(2,0,2,0);bar->setSpacing(5);
    m_heading=new QLabel(QStringLiteral("Experiment Data"),m_toolbar);
    m_heading->setObjectName(QStringLiteral("experimentTableTitle"));
    auto titleFont=m_heading->font();titleFont.setBold(true);m_heading->setFont(titleFont);
    bar->addWidget(m_heading,1);
    m_count=new QLabel(m_toolbar);m_count->setObjectName(QStringLiteral("experimentTableRowCount"));
    bar->addWidget(m_count);
    m_follow=new PanelThemedCheckBox(QStringLiteral("Follow"),m_toolbar);
    m_follow->setObjectName(QStringLiteral("experimentTableFollow"));m_follow->setChecked(true);
    m_follow->setToolTip(QStringLiteral("Keep following new rows while viewing the bottom of the table"));
    bar->addWidget(m_follow);
    m_importButton=new QPushButton(QStringLiteral("Import CSV"),m_toolbar);
    m_exportButton=new QPushButton(QStringLiteral("Export CSV"),m_toolbar);
    m_copyButton=new QPushButton(QStringLiteral("Copy"),m_toolbar);
    m_clearButton=new QPushButton(QStringLiteral("Clear"),m_toolbar);
    m_importButton->setObjectName(QStringLiteral("experimentTableImport"));
    m_exportButton->setObjectName(QStringLiteral("experimentTableExport"));
    m_copyButton->setObjectName(QStringLiteral("experimentTableCopy"));
    m_clearButton->setObjectName(QStringLiteral("experimentTableClear"));
    bar->addWidget(m_importButton);bar->addWidget(m_exportButton);
    bar->addWidget(m_copyButton);bar->addWidget(m_clearButton);
    layout->addWidget(m_toolbar);
    m_status=new QLabel(this);m_status->setObjectName(QStringLiteral("experimentTableStatus"));
    m_status->setVisible(false);layout->addWidget(m_status);
    m_view=new QTableView(this);m_view->setObjectName(QStringLiteral("experimentTableView"));
    m_model=new ExperimentTableModel(m_view);m_view->setModel(m_model);
    m_view->setAlternatingRowColors(true);
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_view->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    m_view->setWordWrap(false);
    m_view->horizontalHeader()->setStretchLastSection(true);
    m_view->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_view->verticalHeader()->setDefaultSectionSize(25);
    m_view->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    layout->addWidget(m_view,1);
    m_countTimer=new QTimer(this);m_countTimer->setSingleShot(true);m_countTimer->setInterval(100);
    QObject::connect(m_countTimer,&QTimer::timeout,this,[this]{updateCount();});
    m_model->setEditedCallback([this](int row,int column,const QString& text,const QString& oldText){
        if(!m_editable || !m_sink)return;
        m_sink(QStringLiteral("cellEdited"),QVariantMap{
            {QStringLiteral("row"),row},{QStringLiteral("column"),column},
            {QStringLiteral("value"),text},{QStringLiteral("oldValue"),oldText}});
        // No full-table copy by default. Explicitly opted-in legacy Panels
        // may still request the expensive valueChanged(rows) notification.
        if(m_emitFullChange)m_sink(QStringLiteral("valueChanged"),rows());
    });
    QObject::connect(m_view->selectionModel(),&QItemSelectionModel::selectionChanged,
                     this,[this]{publishSelection();});
    QObject::connect(m_view->selectionModel(),&QItemSelectionModel::currentChanged,
                     this,[this]{publishSelection();});
    QObject::connect(m_follow,&QCheckBox::toggled,this,[this](bool enabled){m_autoScroll=enabled;});
    QObject::connect(m_importButton,&QPushButton::clicked,this,[this]{importCsvAction();});
    QObject::connect(m_exportButton,&QPushButton::clicked,this,[this]{exportCsvAction();});
    QObject::connect(m_copyButton,&QPushButton::clicked,this,[this]{copySelection();});
    QObject::connect(m_clearButton,&QPushButton::clicked,this,[this]{clearFromToolbar();});
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(m_view,&QWidget::customContextMenuRequested,this,[this](const QPoint& pos){
        QMenu menu(m_view);
        QAction* importAction=menu.addAction(QStringLiteral("Import CSV..."));
        QAction* exportAction=menu.addAction(QStringLiteral("Export CSV..."));
        QAction* copyAction=menu.addAction(QStringLiteral("Copy selected cells"));
        QAction* clearAction=menu.addAction(QStringLiteral("Clear retained rows"));
        QAction* chosen=menu.exec(m_view->mapToGlobal(pos));
        if(chosen==importAction)importCsvAction();
        else if(chosen==exportAction)exportCsvAction();
        else if(chosen==copyAction)copySelection();
        else if(chosen==clearAction)clearFromToolbar();
    });
    updateCount();
}
void ExperimentDataTableWidget::setTableTitle(const QString& text) {
    m_heading->setText(text.left(120));
}
void ExperimentDataTableWidget::setTableStatus(const QString& text) {
    const QString bounded=text.left(200);m_status->setText(bounded);
    m_status->setVisible(!bounded.isEmpty());
}
void ExperimentDataTableWidget::setAutoScroll(bool enabled) {
    m_autoScroll=enabled;
    QSignalBlocker blocker(m_follow);m_follow->setChecked(enabled);
}
void ExperimentDataTableWidget::updateCount() {
    m_count->setText(QStringLiteral("%1 / %2 rows")
                         .arg(m_model->rowCount()).arg(m_maxRows));
}
void ExperimentDataTableWidget::scheduleCount() {
    if(!m_countTimer->isActive())m_countTimer->start();
}
void ExperimentDataTableWidget::publishSelection() {
    if(!m_sink)return;
    const auto* select=m_view->selectionModel();
    const QModelIndex index=select->currentIndex();
    QVariantList selectedRows;
    QSet<int> seen;
    qint64 total=0;
    for(const QItemSelectionRange& range:select->selection()) {
        total+=qint64(range.bottom()-range.top()+1)*qint64(range.right()-range.left()+1);
        for(int row=range.top();row<=range.bottom() && selectedRows.size()<128;++row) {
            if(!seen.contains(row)){seen.insert(row);selectedRows<<row;}
        }
    }
    m_sink(QStringLiteral("selectionChanged"),QVariantMap{
        {QStringLiteral("row"),index.isValid()?index.row():-1},
        {QStringLiteral("column"),index.isValid()?index.column():-1},
        {QStringLiteral("rows"),selectedRows},
        {QStringLiteral("selectedCells"),total},
        {QStringLiteral("truncated"),total>128}});
}
void ExperimentDataTableWidget::copySelection() {
    auto* sel=m_view->selectionModel();
    qint64 estimated=0;
    for(const QItemSelectionRange& range:sel->selection()) {
        estimated+=qint64(range.bottom()-range.top()+1)*qint64(range.right()-range.left()+1);
        if(estimated>100000)break;
    }
    if(estimated>100000){
        QMessageBox::warning(this,QStringLiteral("Copy Experiment Data"),
          QStringLiteral("Select fewer cells for clipboard copy; use Export CSV for the full table."));
        return;
    }
    const QModelIndexList indexes=sel->selectedIndexes();
    if(indexes.isEmpty())return;
    QMap<int,QMap<int,QString>> values;
    int minCol=INT_MAX,maxCol=0;
    for(const auto& cell:indexes){
        values[cell.row()].insert(cell.column(),m_model->data(cell,Qt::DisplayRole).toString());
        minCol=qMin(minCol,cell.column());maxCol=qMax(maxCol,cell.column());
    }
    QStringList output;
    for(auto row=values.cbegin();row!=values.cend();++row){
        QStringList fields;
        for(int c=minCol;c<=maxCol;++c)fields<<row.value().value(c);
        output<<fields.join(QLatin1Char('\t'));
    }
    QApplication::clipboard()->setText(output.join(QLatin1Char('\n')));
}
void ExperimentDataTableWidget::clearFromToolbar() {
    if(!applyValue(QVariantMap{{QStringLiteral("clear"),true}}))return;
    if(m_sink) {
        // Private native mirror update: never exposed as a JavaScript event.
        m_sink(QStringLiteral("__experimentTableMirrorReset"),QVariant());
        if(m_emitFullChange)m_sink(QStringLiteral("valueChanged"),rows());
        m_sink(QStringLiteral("cleared"),QVariantMap{{QStringLiteral("rows"),0}});
    }
}
bool ExperimentDataTableWidget::importCsvAction(const QString& optionalPath) {
    const QString path=optionalPath.isEmpty()?
        QFileDialog::getOpenFileName(this,QStringLiteral("Import experiment data"),{},
                                      QStringLiteral("CSV files (*.csv)")):optionalPath;
    if(path.isEmpty())return false; // Cancel is not an error.
    QString error;
    const bool ok=importCsv(path,&error);
    if(m_sink)m_sink(QStringLiteral("importCompleted"),QVariantMap{
        {QStringLiteral("ok"),ok},{QStringLiteral("path"),path},
        {QStringLiteral("rows"),ok?rowCount():0},{QStringLiteral("error"),error}});
    if(!ok)QMessageBox::warning(this,QStringLiteral("Experiment CSV import"),error);
    return ok;
}
bool ExperimentDataTableWidget::exportCsvAction(const QString& optionalPath) {
    const QString path=optionalPath.isEmpty()?
        QFileDialog::getSaveFileName(this,QStringLiteral("Export experiment data"),{},
                                       QStringLiteral("CSV files (*.csv)")):optionalPath;
    if(path.isEmpty())return false;
    QString error;const bool ok=exportCsv(path,&error);
    if(m_sink)m_sink(QStringLiteral("exportCompleted"),QVariantMap{
        {QStringLiteral("ok"),ok},{QStringLiteral("path"),path},
        {QStringLiteral("rows"),ok?rowCount():0},{QStringLiteral("error"),error}});
    if(!ok)QMessageBox::warning(this,QStringLiteral("Experiment CSV export"),error);
    return ok;
}
void ExperimentDataTableWidget::configure(const PanelControl& d) {
    const QStringList names=parseColumns(d.properties.value(QStringLiteral("columns")).toString(
        QStringLiteral("Frequency (Hz)|Predicted|Measured|Error (%)")));
    if(names.isEmpty() || names.size()>ExperimentTableLimits::MaxColumns)return;
    const QString initialSource=d.properties.value(QStringLiteral("initialRows")).toString();
    const bool initialChanged=initialSource!=m_initialSource;
    const bool columnsChanged=names!=m_columns;
    m_columns=names;m_initialSource=initialSource;
    m_maxRows=qBound(1,d.properties.value(QStringLiteral("maxRows")).toInt(ExperimentTableLimits::DefaultRows),
                      ExperimentTableLimits::MaxRows);
    m_editable=d.properties.value(QStringLiteral("editable")).toBool(false);
    // Explicit opt-in only. Panels predating this setting no longer make
    // 50,000-row copies merely because an operator edited one cell.
    m_emitFullChange=d.properties.value(QStringLiteral("emitFullChangeEvent")).toBool(false);
    setTableTitle(d.properties.value(QStringLiteral("tableTitle")).toString(
        d.text.isEmpty()?QStringLiteral("Experiment Data"):d.text));
    setTableStatus(d.properties.value(QStringLiteral("statusText")).toString());
    m_toolbar->setVisible(d.properties.value(QStringLiteral("showToolbar")).toBool(true));
    m_count->setVisible(d.properties.value(QStringLiteral("showRowCount")).toBool(true));
    m_importButton->setVisible(d.properties.value(QStringLiteral("showImportButton")).toBool(true));
    m_exportButton->setVisible(d.properties.value(QStringLiteral("showExportButton")).toBool(true));
    m_copyButton->setVisible(d.properties.value(QStringLiteral("showCopyButton")).toBool(true));
    m_clearButton->setVisible(d.properties.value(QStringLiteral("showClearButton")).toBool(true));
    m_follow->setVisible(d.properties.value(QStringLiteral("showFollowToggle")).toBool(true));
    setAutoScroll(d.properties.value(QStringLiteral("autoScroll")).toBool(true));
    m_model->configure(m_columns,m_maxRows,m_editable);
    m_view->setEditTriggers(m_editable ?
        (QAbstractItemView::DoubleClicked|QAbstractItemView::EditKeyPressed|QAbstractItemView::AnyKeyPressed) :
        QAbstractItemView::NoEditTriggers);
    applyTheme();
    if(rowCount()==0 || initialChanged || columnsChanged)
        setRows(initialTableRows(initialSource,m_columns.size(),m_maxRows));
    updateCount();
}
void ExperimentDataTableWidget::applyTheme() {
    const bool light=lightMode(this);
    const QString ink=light?QStringLiteral("#22394c"):QStringLiteral("#e0ebf5");
    const QString secondary=light?QStringLiteral("#536b7c"):QStringLiteral("#aebecf");
    m_heading->setStyleSheet(QStringLiteral("color:%1;").arg(ink));
    m_count->setStyleSheet(QStringLiteral("color:%1;").arg(secondary));
    m_status->setStyleSheet(QStringLiteral("color:%1;").arg(secondary));
    m_view->setStyleSheet(lightMode(this)?
        QStringLiteral("QTableView {background:#ffffff;alternate-background-color:#eff5f9;color:#15283b;gridline-color:#ccdce7;} QHeaderView::section {background:#dce7ef;color:#23374a;padding:4px;border:1px solid #cad7e2;}"):
        QStringLiteral("QTableView {background:#142638;alternate-background-color:#203449;color:#eaf6ff;gridline-color:#415b75;selection-background-color:#286a91;} QHeaderView::section {background:#29445f;color:#eaf6ff;padding:4px;border:1px solid #395773;}"));
    // Presentation only. Do not modify the retained data or worker state.
}
void ExperimentDataTableWidget::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if(event->type()==QEvent::ThemeChange)applyTheme();
}

bool ExperimentDataTableWidget::setRows(const QVariant& input) {return applyValue(input);}
bool ExperimentDataTableWidget::applyValue(const QVariant& input) {
    ExperimentTableData::Mutation mutation;
    if(!ExperimentTableData::decode(input,m_columns.size(),m_maxRows,&mutation))return false;
    using Kind=ExperimentTableData::Kind;
    if(mutation.kind==Kind::Title){setTableTitle(mutation.value);return true;}
    if(mutation.kind==Kind::Status){setTableStatus(mutation.value);return true;}
    if(mutation.kind==Kind::Follow){setAutoScroll(mutation.enabled);return true;}
    if(mutation.kind==Kind::ImportCsv){importCsvAction(mutation.value);return true;}
    if(mutation.kind==Kind::ExportCsv){exportCsvAction(mutation.value);return true;}
    if(mutation.kind==Kind::CopySelection){copySelection();return true;}
    const bool atBottom=m_view->verticalScrollBar()->value()>=m_view->verticalScrollBar()->maximum()-2;
    const QModelIndex firstVisible=m_view->indexAt(QPoint(2,2));
    QPersistentModelIndex anchor=firstVisible.isValid()?QPersistentModelIndex(firstVisible):QPersistentModelIndex();
    if(!m_model->mutate(mutation))return false;
    if(mutation.kind==Kind::Append) {
        if(m_autoScroll && atBottom)m_view->scrollToBottom();
        else if(anchor.isValid())m_view->scrollTo(anchor,QAbstractItemView::PositionAtTop);
    }
    scheduleCount();
    return true;
}
QVariantList ExperimentDataTableWidget::rows() const {return m_model->rows();}
bool ExperimentDataTableWidget::exportCsv(const QString& path,QString* error) const {
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly|QIODevice::Text)){
        if(error) {
            *error=file.errorString();
        }
        return false;
    }
    QTextStream out(&file);
    QStringList headers;for(const QString& heading:m_columns)headers<<escapedCsv(heading);
    out<<headers.join(',')<<'\n';
    for(const auto& row:m_model->retained()) {
        QStringList cells;cells.reserve(row.size());
        for(const QString& cell:row)cells<<escapedCsv(cell);
        out<<cells.join(',')<<'\n';
    }
    out.flush();
    if(out.status()!=QTextStream::Ok || !file.commit()){
        if(error) {
            *error=file.errorString();
        }
        return false;
    }
    return true;
}
namespace {
// RFC4180-style quoted fields, doubled quote escapes and embedded line breaks.
// A bounded import is intentional: CSV import is an operator action on the GUI
// thread, not a substitute for long-running streaming acquisition to disk.
bool decodeExperimentCsv(const QString& text,int columns,int maxRows,
                         QVector<ExperimentTableData::Row>* data,QString* error) {
    using Row=ExperimentTableData::Row;
    QVector<Row> result;
    Row record;QString cell;
    bool quoted=false,afterQuote=false,atFieldStart=true;
    int records=0;
    const auto fail=[&](const QString& reason){if(error)*error=reason;return false;};
    auto finishCell=[&](){record<<cell;cell.clear();atFieldStart=true;afterQuote=false;};
    auto finishRecord=[&]()->bool {
        finishCell();++records;
        if(record.size()!=columns)return false;
        if(records>1) {
            if(result.size()>=maxRows)return false;
            for(const QString& part:record)
                if(part.size()>ExperimentTableLimits::MaxCellCharacters || part.contains(QChar::Null))return false;
            result.push_back(record);
        }
        record.clear();return true;
    };
    for(int i=0;i<text.size();++i) {
        const QChar ch=text.at(i);
        if(quoted) {
            if(ch==QLatin1Char('"')) {
                if(i+1<text.size() && text.at(i+1)==QLatin1Char('"')){cell+=QLatin1Char('"');++i;}
                else {quoted=false;afterQuote=true;}
            } else cell+=ch;
        } else if(ch==QLatin1Char('"')) {
            if(!atFieldStart || afterQuote)return fail(QStringLiteral("Malformed CSV quoting"));
            quoted=true;atFieldStart=false;
        } else if(ch==QLatin1Char(','))finishCell();
        else if(ch==QLatin1Char('\n') || ch==QLatin1Char('\r')) {
            if(!finishRecord())return fail(QStringLiteral("CSV column count, row count, or cell limit exceeded"));
            if(ch==QLatin1Char('\r') && i+1<text.size() && text.at(i+1)==QLatin1Char('\n'))++i;
        } else {
            if(afterQuote)return fail(QStringLiteral("Unexpected characters after quoted CSV field"));
            cell+=ch;atFieldStart=false;
        }
        if(cell.size()>ExperimentTableLimits::MaxCellCharacters+256)
            return fail(QStringLiteral("CSV cell exceeds size limit"));
        if(record.size()>columns)return fail(QStringLiteral("CSV contains too many columns"));
    }
    if(quoted)return fail(QStringLiteral("Unterminated quoted CSV cell"));
    if(!record.isEmpty() || !cell.isEmpty() || !atFieldStart) {
        if(!finishRecord())return fail(QStringLiteral("Invalid last CSV row"));
    }
    if(records==0)return fail(QStringLiteral("CSV must have a header row"));
    *data=std::move(result);return true;
}
}
bool ExperimentDataTableWidget::importCsv(const QString& path,QString* error) {
    QFile file(path);
    constexpr qint64 MaxCsvBytes=32ll*1024*1024;
    if(!file.open(QIODevice::ReadOnly)){
        if(error) {
            *error=file.errorString();
        }
        return false;
    }
    if(file.size()>MaxCsvBytes){if(error)*error=QStringLiteral("CSV exceeds 32 MiB import limit");return false;}
    const QByteArray bytes=file.readAll();
    QString content=QString::fromUtf8(bytes);
    if(content.startsWith(QChar(0xfeff)))content.remove(0,1);
    QVector<ExperimentTableData::Row> parsed;
    if(!decodeExperimentCsv(content,columnCount(),m_maxRows,&parsed,error))return false;
    ExperimentTableData::Mutation mutation;
    mutation.kind=ExperimentTableData::Kind::Replace;
    mutation.rows=std::move(parsed);
    if(!m_model->mutate(mutation))return false;
    updateCount();
    // Synchronize the worker mirror after operator CSV import without
    // publishing the entire table as a JavaScript event. Chunks are bounded
    // and ordered before importCompleted, so handlers see imported data.
    if(m_sink) {
        m_sink(QStringLiteral("__experimentTableMirrorReset"),QVariant());
        constexpr int MirrorChunkRows=1024;
        const auto& stored=m_model->retained();
        for(int start=0;start<int(stored.size());start+=MirrorChunkRows) {
            QVariantList batch;
            const int end=qMin(int(stored.size()),start+MirrorChunkRows);
            batch.reserve(end-start);
            for(int i=start;i<end;++i) {
                QVariantList fields;
                for(const auto& field:stored[std::size_t(i)])fields.append(field);
                batch.append(QVariant(fields));
            }
            m_sink(QStringLiteral("__experimentTableMirrorAppend"),
                   QVariantMap{{QStringLiteral("appendRows"),batch}});
        }
        if(m_emitFullChange)m_sink(QStringLiteral("valueChanged"),rows());
    }
    return true;
}
PanelControl ExperimentDataTableFactory::defaultDefinition(const QString& id) const {
    auto d=WidgetFactory::defaultDefinition(id);d.geometry.setSize(QSize(700,270));d.text="Experiment observations";
    d.properties={{"columns","Frequency (Hz)|Predicted|Measured|Error (%)"},
                  {"initialRows","100|0.998||\n500|0.954||\n1000|0.847||\n1592|0.707||\n5000|0.303||"},
                  {"maxRows",250},{"editable",false},
                  {"tableTitle","Experiment observations"},{"statusText",""},
                  {"showToolbar",true},{"showRowCount",true},{"showImportButton",true},
                  {"showExportButton",true},{"showCopyButton",true},{"showClearButton",true},
                  {"showFollowToggle",true},{"autoScroll",true},{"emitFullChangeEvent",false}};
    return d;
}
QVector<WidgetProperty> ExperimentDataTableFactory::properties() const {
    WidgetProperty cols{"columns","Column headers (separate with |)","string",true};cols.category="Table";
    WidgetProperty initial{"initialRows","Initial rows (one per line, | separated)","multiline",true};initial.category="Table";
    WidgetProperty max{"maxRows","Maximum retained rows (rolling)","integer",true,1,ExperimentTableLimits::MaxRows};
    max.defaultValue=ExperimentTableLimits::DefaultRows;max.category="Table";
    WidgetProperty edit{"editable","Allow user cell editing","boolean",true};edit.defaultValue=false;edit.category="Table";
    WidgetProperty title{"tableTitle","Table title","string",true};title.category="Header";
    WidgetProperty status{"statusText","Status text","string",true};status.category="Header";
    WidgetProperty bar{"showToolbar","Show title and toolbar","boolean",true};bar.defaultValue=true;bar.category="Header";
    WidgetProperty count{"showRowCount","Show retained row count","boolean",true};count.defaultValue=true;count.category="Header";
    WidgetProperty imp{"showImportButton","Show Import CSV button","boolean",true};imp.defaultValue=true;imp.category="Toolbar";
    WidgetProperty exp{"showExportButton","Show Export CSV button","boolean",true};exp.defaultValue=true;exp.category="Toolbar";
    WidgetProperty copy{"showCopyButton","Show Copy button","boolean",true};copy.defaultValue=true;copy.category="Toolbar";
    WidgetProperty clear{"showClearButton","Show Clear button","boolean",true};clear.defaultValue=true;clear.category="Toolbar";
    WidgetProperty follow{"showFollowToggle","Show Follow toggle","boolean",true};follow.defaultValue=true;follow.category="Toolbar";
    WidgetProperty scroll{"autoScroll","Follow newest rows by default","boolean",true};scroll.defaultValue=true;scroll.category="Table";
    WidgetProperty legacy{"emitFullChangeEvent","Enable legacy full valueChanged(rows) snapshots (slow)","boolean",true};
    legacy.defaultValue=false;legacy.category="Events";
    return {cols,initial,max,edit,title,status,bar,count,imp,exp,copy,clear,follow,scroll,legacy};
}
QVector<PanelEventDefinition> ExperimentDataTableFactory::events() const {
    return {{"valueChanged","valueChanged(rows: array)",
        "Deprecated, explicitly opt-in only. Full retained rows after operator edits, Clear and import; avoid for large datasets.","rows"},
        {"cellEdited","cellEdited(edit: object)","One edited cell: {row,column,value,oldValue}. Zero-based indexes.","edit"},
        {"selectionChanged","selectionChanged(selection: object)",
         "Current cell and up to 128 selected row indexes: {row,column,rows,selectedCells,truncated}.","selection"},
        {"importCompleted","importCompleted(result: object)","CSV import finished: {ok,path,rows,error}.","result"},
        {"exportCompleted","exportCompleted(result: object)","CSV export finished: {ok,path,rows,error}.","result"},
        {"cleared","cleared(info: object)","User cleared the retained table through the toolbar.","info"}};
}
QVariant ExperimentDataTableFactory::initialRuntimeValue(const PanelControl& d) const {
    const int cols=parseColumns(d.properties.value("columns").toString()).size();
    return initialTableRows(d.properties.value("initialRows").toString(),cols,
        qBound(1,d.properties.value("maxRows").toInt(ExperimentTableLimits::DefaultRows),ExperimentTableLimits::MaxRows));
}
QWidget* ExperimentDataTableFactory::create(const PanelControl& d,QWidget* parent,EventSink sink) const {
    auto* table=new ExperimentDataTableWidget(parent);
    table->setChangeSink(std::move(sink));table->configure(d);return table;
}
void ExperimentDataTableFactory::applyDefinition(QWidget* widget,const PanelControl& d) const {
    if(auto* t=dynamic_cast<ExperimentDataTableWidget*>(widget))t->configure(d);
}
void ExperimentDataTableFactory::applyRuntimeValue(QWidget* widget,const PanelControl&,const QVariant& data) const {
    if(auto* t=dynamic_cast<ExperimentDataTableWidget*>(widget))
        if(!t->applyValue(data))qWarning()<<"Experiment Data Table rejected ui.set payload"<<t->objectName();
}
