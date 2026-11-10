#include "DeviceWorkspace.h"
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QAbstractItemView>
#include <QStringList>
#include <QHeaderView>
#include <QScrollArea>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QSignalBlocker>
#include <QFrame>
#include <QVariantMap>
#include <QFont>

namespace {
QLabel* heading(const QString& title, QWidget* parent) {
    auto* label = new QLabel(title,parent);
    QFont f=label->font();f.setPointSize(qMax(13,f.pointSize()+4));f.setBold(true);label->setFont(f);
    return label;
}
QLabel* valueLabel(QWidget* parent) {
    auto* label=new QLabel(QStringLiteral("—"),parent);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setWordWrap(true);
    return label;
}
QLabel* pin(const QString& text, const QString& color, QWidget* parent) {
    auto* label=new QLabel(text,parent);
    label->setAlignment(Qt::AlignCenter);
    label->setMinimumHeight(28);
    label->setStyleSheet(QStringLiteral("QLabel { background:%1; color:#ffffff; border-radius:5px; padding:4px; font-weight:600; }").arg(color));
    label->setToolTip(text);
    return label;
}
QString pinColor(int i) {
    if(i<2)return QStringLiteral("#286f9b"); // Scope
    if(i==2 || i==5)return QStringLiteral("#555e6a"); // GND
    if(i==3)return QStringLiteral("#ad3b45"); // Supplies
    if(i==4)return QStringLiteral("#b48225"); // Wavegen
    if(i==6)return QStringLiteral("#7863a6"); // Trigger
    return QStringLiteral("#237f75"); // Digital
}
}
DeviceWorkspace::DeviceWorkspace(QWidget* parent): QWidget(parent) {
    setObjectName(QStringLiteral("deviceWorkspace"));
    auto* outer=new QVBoxLayout(this);outer->setContentsMargins(0,0,0,0);
    auto* scroll=new QScrollArea(this);scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);
    auto* body=new QWidget(scroll);scroll->setWidget(body);
    auto* layout=new QVBoxLayout(body);layout->setContentsMargins(20,18,20,22);layout->setSpacing(14);
    layout->addWidget(heading(QStringLiteral("Device Center"),body));
    auto* intro=new QLabel(QStringLiteral("Select a simulation or an attached Digilent instrument. The Device page uses the WaveForms SDK to discover real hardware; it never drives outputs during discovery."),body);
    intro->setWordWrap(true);layout->addWidget(intro);
    auto* columns=new QHBoxLayout;columns->setSpacing(16);layout->addLayout(columns);
    auto* left=new QVBoxLayout;left->setSpacing(12);columns->addLayout(left,4);
    auto* right=new QVBoxLayout;right->setSpacing(12);columns->addLayout(right,5);

    auto* connection=new QGroupBox(QStringLiteral("Connection and runtime"),body);
    auto* form=new QFormLayout(connection);form->setSpacing(10);
    m_mode=new QComboBox(connection);m_mode->setObjectName(QStringLiteral("deviceRuntimeMode"));
    m_mode->addItems({QStringLiteral("Simulation"),QStringLiteral("Physical device (DWF SDK)")});
    m_mode->setToolTip(QStringLiteral("Simulation is isolated from hardware. Physical mode opens the selected device only when a script/panel/scope starts."));
    form->addRow(QStringLiteral("Runtime"),m_mode);
    m_device=new QSpinBox(connection);m_device->setObjectName(QStringLiteral("deviceIndex"));m_device->setRange(0,15);
    form->addRow(QStringLiteral("Device index"),m_device);
    m_config=new QSpinBox(connection);m_config->setObjectName(QStringLiteral("deviceConfigIndex"));m_config->setRange(1,16);
    form->addRow(QStringLiteral("Configuration"),m_config);
    auto* actions=new QWidget(connection);auto* buttons=new QHBoxLayout(actions);buttons->setContentsMargins(0,0,0,0);
    m_detect=new QPushButton(QStringLiteral("Refresh devices"),actions);m_detect->setObjectName(QStringLiteral("refreshDevicesButton"));
    m_inspect=new QPushButton(QStringLiteral("Inspect capabilities"),actions);m_inspect->setObjectName(QStringLiteral("inspectDeviceCapabilities"));
    buttons->addWidget(m_detect);buttons->addWidget(m_inspect);
    form->addRow(actions);
    m_status=valueLabel(connection);m_status->setObjectName(QStringLiteral("deviceDetectionStatus"));form->addRow(QStringLiteral("SDK status"),m_status);
    left->addWidget(connection);
    auto* details=new QGroupBox(QStringLiteral("Detected hardware (from DWF SDK)"),body);
    auto* detailForm=new QFormLayout(details);
    m_model=valueLabel(details);m_serial=valueLabel(details);m_configs=valueLabel(details);
    detailForm->addRow(QStringLiteral("Model"),m_model);
    detailForm->addRow(QStringLiteral("Serial number"),m_serial);
    detailForm->addRow(QStringLiteral("Configurations"),m_configs);
    m_list=new QTableWidget(details);m_list->setObjectName(QStringLiteral("detectedDeviceList"));
    m_list->setColumnCount(4);m_list->setHorizontalHeaderLabels({QStringLiteral("#"),QStringLiteral("Device"),QStringLiteral("Serial"),QStringLiteral("Configs")});
    m_list->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);
    m_list->horizontalHeader()->setSectionResizeMode(2,QHeaderView::Stretch);
    m_list->setSelectionBehavior(QAbstractItemView::SelectRows);m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);m_list->setMinimumHeight(115);
    detailForm->addRow(m_list);
    left->addWidget(details);
    auto* capabilities=new QGroupBox(QStringLiteral("Hardware capabilities"),body);
    auto* capLayout=new QVBoxLayout(capabilities);
    m_capabilities=valueLabel(capabilities);m_capabilities->setObjectName(QStringLiteral("deviceCapabilityReadout"));
    m_capabilities->setText(QStringLiteral("Select Physical device and use Inspect capabilities to read the connected device. No device is opened for automatic discovery."));
    capLayout->addWidget(m_capabilities);
    left->addWidget(capabilities);left->addStretch(1);

    auto* connector=new QGroupBox(QStringLiteral("Analog Discovery 3 • 2×15 MTE connector"),body);
    auto* pinLayout=new QVBoxLayout(connector);
    auto* pinNote=new QLabel(QStringLiteral("Connector signal names, top and bottom rows. Verify physical orientation and wire colours against the Digilent AD3 datasheet before connecting a circuit."),connector);
    pinNote->setWordWrap(true);pinLayout->addWidget(pinNote);
    auto* pins=new QGridLayout;pins->setSpacing(4);
    const QStringList top={"1+","2+","GND","V+","W1","GND","T1","DIO 0","DIO 1","DIO 2","DIO 3","DIO 4","DIO 5","DIO 6","DIO 7"};
    const QStringList bottom={"1−","2−","GND","V−","W2","GND","T2","DIO 8","DIO 9","DIO 10","DIO 11","DIO 12","DIO 13","DIO 14","DIO 15"};
    for(int i=0;i<15;++i){
        pins->addWidget(pin(top.at(i),pinColor(i),connector),0,i);
        pins->addWidget(pin(bottom.at(i),pinColor(i),connector),1,i);
    }
    pinLayout->addLayout(pins);
    auto* legend=new QLabel(QStringLiteral("Blue: differential scope • Grey: ground • Red: power • Amber: waveform generator • Purple: trigger • Teal: digital I/O"),connector);
    legend->setWordWrap(true);pinLayout->addWidget(legend);
    right->addWidget(connector);
    auto* overview=new QGroupBox(QStringLiteral("AD3 instrument interfaces"),body);
    auto* summary=new QGridLayout(overview);
    const QStringList names={"Oscilloscope","Waveform generators","Digital I/O","Triggers","Programmable supplies","Connection"};
    const QStringList descriptions={
        "2 differential analog inputs (1+/1−, 2+/2−)",
        "W1 and W2 analog waveform outputs",
        "16 bidirectional DIO lines (0–15)",
        "T1 and T2 digital trigger I/O",
        "V+ and V− variable voltage rails",
        "USB-C; optional external 5 V supply"};
    for(int i=0;i<names.size();++i){
        auto* name=new QLabel(names.at(i),overview);QFont f=name->font();f.setBold(true);name->setFont(f);
        auto* desc=new QLabel(descriptions.at(i),overview);desc->setWordWrap(true);
        summary->addWidget(name,i,0);summary->addWidget(desc,i,1);
    }
    right->addWidget(overview);
    auto* warning=new QLabel(QStringLiteral("Safety: Scope inputs, waveform outputs, digital pins and supplies have different electrical ratings. This reference shows assignments, not permission to connect arbitrary voltages. Always consult the official AD3 datasheet."),body);
    warning->setWordWrap(true);right->addWidget(warning);right->addStretch(1);
    connect(m_detect,&QPushButton::clicked,this,[this]{refreshDevices();});
    connect(m_inspect,&QPushButton::clicked,this,[this]{inspectCapabilities();});
    connect(m_device,QOverload<int>::of(&QSpinBox::valueChanged),this,[this]{updateSelection();});
    connect(m_mode,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{setBusy(m_busy);});
    connect(m_list,&QTableWidget::cellClicked,this,[this](int row,int){
        if(row>=0 && row<m_devices.size())m_device->setValue(m_devices.at(row).index);
    });
    m_status->setText(QStringLiteral("Press Refresh devices to query the WaveForms SDK."));
    setBusy(false);
}

void DeviceWorkspace::setBusy(bool busy) {
    m_busy=busy;
    const bool idle=!busy;
    m_mode->setEnabled(idle);
    m_device->setEnabled(idle && m_mode->currentIndex()==1);
    m_config->setEnabled(idle && m_mode->currentIndex()==1);
    m_detect->setEnabled(idle);
    m_inspect->setEnabled(idle && m_mode->currentIndex()==1 && !m_devices.isEmpty());
    m_list->setEnabled(idle);
}

void DeviceWorkspace::refreshDevices() {
    if(m_busy)return;
    DwfRuntime sdk;QString error;
    m_devices=sdk.enumerate(&error);
    m_list->setRowCount(m_devices.size());
    for(int row=0;row<m_devices.size();++row){
        const auto& d=m_devices.at(row);
        const QStringList items={QString::number(d.index),d.name,d.serial,QString::number(d.configurations)};
        for(int col=0;col<4;++col)m_list->setItem(row,col,new QTableWidgetItem(items.at(col)));
    }
    if(m_devices.isEmpty()){
        m_status->setText(error.isEmpty()?QStringLiteral("No devices connected. Simulation remains available."):error);
        m_capabilities->setText(QStringLiteral("No hardware information available."));
    }else{
        m_status->setText(QStringLiteral("%1 device(s) enumerated successfully.").arg(m_devices.size()));
        bool found=false;for(const auto& d:m_devices)if(d.index==m_device->value())found=true;
        if(!found)m_device->setValue(m_devices.first().index);
    }
    updateSelection();setBusy(m_busy);
}

void DeviceWorkspace::updateSelection() {
    for(int row=0;row<m_devices.size();++row){
        const auto& d=m_devices.at(row);
        if(d.index!=m_device->value())continue;
        m_model->setText(d.name);m_serial->setText(d.serial);m_configs->setText(QString::number(d.configurations));
        const QSignalBlocker blocked(m_config);
        m_config->setRange(1,qMax(1,d.configurations));
        m_list->selectRow(row);
        return;
    }
    m_model->setText(QStringLiteral("Not detected"));m_serial->setText(QStringLiteral("—"));m_configs->setText(QStringLiteral("—"));
}

void DeviceWorkspace::inspectCapabilities() {
    if(m_busy || m_mode->currentIndex()!=1 || m_devices.isEmpty())return;
    DwfRuntime sdk;QString error;
    if(!sdk.open(m_device->value(),m_config->value()-1,&error)){
        m_capabilities->setText(QStringLiteral("Could not inspect device: ")+error);return;
    }
    const auto scope=sdk.scopeCapabilities(&error);
    QString output=QStringLiteral("Active SDK configuration %1.\n").arg(m_config->value());
    if(!scope.isEmpty()){
        output+=QStringLiteral("Analog input capabilities (reported by hardware):\n");
        for(auto it=scope.constBegin();it!=scope.constEnd();++it){
            output+=QStringLiteral("  %1: %2\n").arg(it.key(),it.value().toString());
        }
    }else output+=QStringLiteral("Scope capability query: ")+error+QStringLiteral("\n");
    for(int ch=0; ch<2; ++ch) {
        QString waveError;
        const auto wave=sdk.wavegenCapabilities(ch,&waveError);
        output+=QStringLiteral("\nWavegen %1 capabilities:\n").arg(ch+1);
        if(wave.isEmpty()) output+=QStringLiteral("  Unavailable: ")+waveError+QStringLiteral("\n");
        else for(auto it=wave.constBegin();it!=wave.constEnd();++it)
            output+=QStringLiteral("  %1: %2\n").arg(it.key(),it.value().toString());
    }
    sdk.close();
    m_capabilities->setText(output);
}
