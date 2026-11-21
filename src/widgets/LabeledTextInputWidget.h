#pragma once

#include <QWidget>
#include <QString>

class QLineEdit;
class QEvent;
class QPaintEvent;
class QResizeEvent;

// One field, one script-visible QString.  Designed to fit the same rounded,
// theme-adaptive visual family as NumericStepperWidget and OptionSelectorWidget.
// The IP variant enforces *IPv4 dotted-decimal* on input, not an arbitrary host.
class LabeledTextInputWidget final : public QWidget {
    Q_OBJECT
public:
    explicit LabeledTextInputWidget(bool ipv4, QWidget* parent = nullptr);

    void configure(const QString& caption, const QString& initialText,
                   const QString& placeholder, const QString& labelPosition,
                   const QString& accent, int maxLength, bool required,
                   bool readOnly);
    bool setRuntimeText(const QString& text); // ui.set(), never emits operator events
    QString text() const;
    QLineEdit* editor() const { return m_editor; }
    bool isIpAddress() const { return m_ipv4; }
    bool isValidValue(const QString& text) const;
    QString labelPosition() const { return m_labelPosition; }
    QSize sizeHint() const override { return QSize(320, 88); }
    QSize minimumSizeHint() const override { return QSize(155, 56); }

signals:
    void userValueChanged(const QString& text);      // accepted operator edit
    void userEditingFinished(const QString& text);   // accepted operator commit
    void userValidationFailed(const QString& text);  // not sent to ui.get()/Panel Data

protected:
    void resizeEvent(QResizeEvent* e) override;
    void changeEvent(QEvent* e) override;
    void paintEvent(QPaintEvent* e) override;

private:
    void updateLayout();
    void updateStyle();
    void onEdited(const QString& text);
    void onFinished();

    QLineEdit* m_editor = nullptr;
    bool m_ipv4 = false;
    bool m_required = false;
    bool m_invalid = false;
    QString m_caption;
    QString m_lastAccepted;
    QString m_labelPosition = QStringLiteral("Top");
    QString m_accent = QStringLiteral("cyan");
};
