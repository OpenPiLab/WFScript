#pragma once

#include <QLabel>

// Standard text label with a predictable, in-bounds optional outline.
// Presentation settings belong to the widget; Panel Page switches never
// recreate this object or snapshot its live text.
class PanelLabelWidget final : public QLabel {
public:
    explicit PanelLabelWidget(const QString& text, QWidget* parent = nullptr);
    void setBorderAppearance(bool visible, int cornerRadius);
    void setTextPresentation(const QString& format, bool wordWrap);
    bool borderVisible() const { return m_borderVisible; }
    int cornerRadius() const { return m_cornerRadius; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    bool m_borderVisible = true;
    int m_cornerRadius = 0;
};
