#pragma once

#include <QColor>
#include <QWidget>

#include <functional>
#include <optional>
#include <vector>

class PaletteEditor;
class QCheckBox;
class QLabel;
class QPushButton;
class QSlider;
class RampStrip;
struct Palette;

// The palette editor of spec 8.2: the ramp as a strip, the contrast, the named colors. Every
// change shows on the panel at once.
class PalettePage : public QWidget {
    Q_OBJECT

public:
    PalettePage(PaletteEditor& editor, QWidget* parent);

protected:
    void hideEvent(QHideEvent* event) override;

private:
    using Change = std::function<Palette(Palette, const QColor&)>;
    struct NamedButton {
        QColor Palette::* member;
        QPushButton* button;
    };

    void refresh();
    // A color dialog on `initial` that shows each color it passes through on the panel, made into
    // a palette by `change`, and puts the palette back if it is cancelled.
    void pick(const QString& title, const QColor& initial, const Change& change);
    void chooseTone(int tone);
    void resetTone(int tone);

    PaletteEditor& m_editor;
    RampStrip* m_strip;
    QCheckBox* m_show;
    QPushButton* m_reset;
    QSlider* m_contrast;
    QLabel* m_contrastValue;
    std::vector<NamedButton> m_named;
    std::optional<int> m_current;
};
