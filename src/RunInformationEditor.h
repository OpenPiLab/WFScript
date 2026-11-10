#pragma once
#include <QDialog>
#include <QJsonObject>
class QTableWidget;

class RunInformationEditor final : public QDialog {
public:
    explicit RunInformationEditor(const QJsonObject& schema,QWidget* parent=nullptr);
    QJsonObject schema() const {return m_schema;}
private:
    QTableWidget* m_table=nullptr;
    QJsonObject m_schema;
    void addRow(const QJsonObject& field);
    void acceptFields();
};
