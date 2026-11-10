#pragma once
#include <QWidget>
#include <QStringList>
#include <QVariant>
class QComboBox;

class OptionSelectorWidget final : public QWidget {
    Q_OBJECT
public:
    explicit OptionSelectorWidget(QWidget* parent=nullptr);
    void configure(QString title,QString accent,QStringList options,int defaultIndex);
    void setSelection(const QVariant& selection); // string label or integer index
    int currentIndex() const;
    QString currentText() const;
    QStringList options() const;
    QSize sizeHint() const override {return QSize(325,112);}
    QSize minimumSizeHint() const override {return QSize(150,60);}
signals:
    void userSelectionChanged(const QString& text);
    void userIndexChanged(int index);
protected:
    void resizeEvent(QResizeEvent*) override;
    void changeEvent(QEvent*) override;
    void paintEvent(QPaintEvent*) override;
private:
    void updateLayout();
    QComboBox* m_combo=nullptr;
    QString m_title="OPERATING MODE";
    QString m_accent="cyan";
};
