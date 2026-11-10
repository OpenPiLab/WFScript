#include "RunInformationEditor.h"
#include "RunInformationSchema.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QJsonArray>
#include <QSet>

RunInformationEditor::RunInformationEditor(const QJsonObject& schema,QWidget* parent)
    :QDialog(parent),m_schema(schema) {
    setWindowTitle("Run Information — Define UUT fields");resize(1040,460);
    auto* layout=new QVBoxLayout(this);
    auto* help=new QLabel("Define the UUT/operator data requested for each test. IDs are stable report keys; rename labels freely. "
        "For Choice fields separate options with |. These definitions are shared with the "
        "Run Information widgets on any Pages (all forms share one live value model) and automatically validated by the "
        "project TestRunner. No ID binding or metadata JavaScript is required. "
        "Field definitions are saved in the .wfpanel; operator values are NOT.",this);
    help->setWordWrap(true);layout->addWidget(help);
    m_table=new QTableWidget(0,6,this);
    m_table->setObjectName("runInformationFieldsTable");
    m_table->setHorizontalHeaderLabels({"Field ID","Operator label","Type","Required","Default","Choice options (separate with |)"});
    m_table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(5,QHeaderView::Stretch);
    m_table->setColumnWidth(0,135);m_table->setColumnWidth(2,105);m_table->setColumnWidth(3,82);
    layout->addWidget(m_table,1);
    auto* actions=new QHBoxLayout;
    auto* add=new QPushButton("+ Add field",this);
    auto* remove=new QPushButton("Remove selected",this);
    actions->addWidget(add);actions->addWidget(remove);actions->addStretch();
    layout->addLayout(actions);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,this);
    buttons->button(QDialogButtonBox::Save)->setText("Save fields");layout->addWidget(buttons);
    connect(add,&QPushButton::clicked,this,[this]{addRow(QJsonObject{{"id",QStringLiteral("newField%1").arg(m_table->rowCount()+1)},{"label","New Field"},{"type","text"}});});
    connect(remove,&QPushButton::clicked,this,[this]{if(m_table->currentRow()>=0)m_table->removeRow(m_table->currentRow());});
    connect(buttons,&QDialogButtonBox::accepted,this,[this]{acceptFields();});
    connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    for(const auto& f:schema.value("fields").toArray())addRow(f.toObject());
}
void RunInformationEditor::addRow(const QJsonObject& f) {
    const int row=m_table->rowCount();m_table->insertRow(row);
    const auto set=[&](int col,const QString& str){m_table->setItem(row,col,new QTableWidgetItem(str));};
    set(0,f.value("id").toString());set(1,f.value("label").toString());
    auto* type=new QComboBox(m_table);type->addItems({"text","number","choice","boolean"});
    const int idx=type->findText(f.value("type").toString("text"));type->setCurrentIndex(idx<0?0:idx);
    m_table->setCellWidget(row,2,type);
    auto* required=new QCheckBox(m_table);required->setChecked(f.value("required").toBool());
    required->setStyleSheet("margin-left:28px;");m_table->setCellWidget(row,3,required);
    set(4,f.value("default").toVariant().toString());
    QStringList choices;
    for(const auto& choice:f.value("options").toArray())choices.append(choice.toString());
    set(5,choices.join(" | "));
}
void RunInformationEditor::acceptFields() {
    QJsonArray fields;
    for(int r=0;r<m_table->rowCount();++r) {
        const auto cell=[&](int col){auto* item=m_table->item(r,col);return item?item->text().trimmed():QString();};
        const QString type=static_cast<QComboBox*>(m_table->cellWidget(r,2))->currentText();
        const bool required=static_cast<QCheckBox*>(m_table->cellWidget(r,3))->isChecked();
        QJsonObject field{{"id",cell(0)},{"label",cell(1)},{"type",type},{"required",required}};
        const QString raw=cell(4);
        if(type=="choice") {
            QJsonArray options;
            for(const auto& option:cell(5).split('|',Qt::SkipEmptyParts))options.append(option.trimmed());
            field.insert("options",options);
        }
        if(type=="boolean")field.insert("default",raw.compare("true",Qt::CaseInsensitive)==0 || raw=="1");
        else if(type=="number" && !raw.isEmpty()) {
            bool ok=false;const double n=raw.toDouble(&ok);
            if(!ok) {QMessageBox::warning(this,"Invalid number","Default for "+cell(1)+" must be numeric");return;}
            field.insert("default",n);
        } else if(type!="number")field.insert("default",raw);
        fields.append(field);
    }
    const QJsonObject next{{"fields",fields}};
    QString error;
    if(!RunInformationSchema::validate(next,&error)){QMessageBox::warning(this,"Invalid Run Information",error);return;}
    m_schema=next;accept();
}
