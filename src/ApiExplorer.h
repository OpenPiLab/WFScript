#pragma once
#include <QWidget>
#include <QString>

class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QTextBrowser;
class QPushButton;

// Read-only navigation generated from CompletionCatalog, the same symbol set
// used by editor completion. Never evaluates JavaScript or touches hardware.
class ApiExplorer final : public QWidget {
    Q_OBJECT
public:
    explicit ApiExplorer(QWidget* parent=nullptr);
    bool containsPath(const QString& path) const;
signals:
    void insertRequested(const QString& apiPath);
private:
    QTreeWidget* m_tree=nullptr;
    QLineEdit* m_search=nullptr;
    QTextBrowser* m_details=nullptr;
    QPushButton* m_insert=nullptr;
    void fill();
    void addChildren(QTreeWidgetItem* parent,const QString& path,int depth);
    bool filterItem(QTreeWidgetItem* item,const QString& term);
    void updateDetails();
};
