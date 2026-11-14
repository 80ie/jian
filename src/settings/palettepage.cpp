#include "settings/palettepage.h"
#include "palette/palette.h"
#include "settings/paletteeditor.h"
#include "settings/rampstrip.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace {

// Slider steps per unit of contrast, and the most contrast it reaches.
constexpr int contrastSteps = 100;
constexpr int mostContrast = 2;
constexpr int iconSize = 16;

struct Named {
    QString label;
    QString use;
    QColor Palette::* member;
};

QIcon swatchIcon(const QColor& color)
{
    QPixmap picture(iconSize, iconSize);
    picture.fill(color);
    return picture;
}

} // namespace

PalettePage::PalettePage(PaletteEditor& editor, QWidget* parent)
    : QWidget(parent)
    , m_editor(editor)
    , m_strip(new RampStrip(this))
    , m_show(new QCheckBox(u"Show the tone under the pointer on the panel"_s, this))
    , m_reset(new QPushButton(this))
    , m_contrast(new QSlider(Qt::Horizontal, this))
    , m_contrastValue(new QLabel(this))
{
    auto* ramp = new QGroupBox(u"Ramp"_s, this);
    auto* rampLayout = new QVBoxLayout(ramp);
    rampLayout->addWidget(m_strip);
    auto* tools = new QHBoxLayout;
    tools->addWidget(m_show);
    tools->addStretch();
    tools->addWidget(m_reset);
    rampLayout->addLayout(tools);
    auto* contrast = new QHBoxLayout;
    contrast->addWidget(new QLabel(u"Contrast"_s, ramp));
    contrast->addWidget(m_contrast, 1);
    contrast->addWidget(m_contrastValue);
    rampLayout->addLayout(contrast);
    hint->setWordWrap(true);
    rampLayout->addWidget(hint);

    auto* named = new QGroupBox(u"Named colors"_s, this);
    auto* form = new QFormLayout(named);
    for (const Named& color : colors) {
        auto* button = new QPushButton(named);
        const auto member = color.member;
        connect(button, &QPushButton::clicked, this, [this, label = color.label, member] {
            pick(label, m_editor.edited().*member, [member](Palette palette, const QColor& chosen) {
                palette.*member = chosen;
                return palette;
            });
        });
        auto* row = new QHBoxLayout;
        row->addWidget(button);
        row->addWidget(new QLabel(color.use, named), 1);
        form->addRow(color.label, row);
        m_named.push_back({ member, button });
    }

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(ramp);
    layout->addWidget(named);
    layout->addStretch();

    m_contrast->setRange(0, mostContrast * contrastSteps);
    connect(m_contrast, &QSlider::valueChanged, this, [this](int value) {
        Palette palette = m_editor.edited();
        palette.contrast = double(value) / contrastSteps;
        m_editor.edit(palette);
    });
    connect(m_strip, &RampStrip::clicked, this, &PalettePage::chooseTone);
    connect(m_strip, &RampStrip::resetRequested, this, &PalettePage::resetTone);
    connect(m_strip, &RampStrip::hovered, this, [this](std::optional<int> tone) {
        if (m_show->isChecked())
            m_editor.showTone(tone);
    });
    connect(m_show, &QCheckBox::toggled, this, [this](bool on) {
        if (!on)
            m_editor.showTone(std::nullopt);
    });
    connect(m_reset, &QPushButton::clicked, this, [this] {
        if (m_current)
            resetTone(*m_current);
    });
    connect(&m_editor, &PaletteEditor::changed, this, &PalettePage::refresh);
    refresh();
}

void PalettePage::hideEvent(QHideEvent*) { m_editor.showTone(std::nullopt); }

void PalettePage::refresh()
{
    const Palette& palette = m_editor.edited();
    m_strip->setColors(palette);
    {
        const QSignalBlocker blocker(m_contrast);
        m_contrast->setValue(qRound(palette.contrast * contrastSteps));
    }
    m_contrastValue->setText(QString::number(palette.contrast, 'f', 2));
    for (const auto& [member, button] : m_named) {
        button->setIcon(swatchIcon(palette.*member));
        button->setText((palette.*member).name());
    }
    const bool overridden = m_current && palette.overrides.contains(*m_current);
    // Buttons cannot have sentences like this.
    m_reset->setText(u"Reset"_s);
    m_reset->setEnabled(overridden);
}

void PalettePage::pick(const QString& title, const QColor& initial, const Change& change)
{
    auto* dialog = new QColorDialog(initial, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    // A native dialog may not report the colors it passes through.
    dialog->setOption(QColorDialog::DontUseNativeDialog);
    dialog->setWindowTitle(title);
    const Palette before = m_editor.edited();
    connect(dialog, &QColorDialog::currentColorChanged, this,
        [this, before, change](const QColor& color) { m_editor.edit(change(before, color)); });
    connect(dialog, &QDialog::rejected, this, [this, before] { m_editor.edit(before); });
    dialog->open();
}

void PalettePage::chooseTone(int tone)
{
    m_current = tone;
    m_strip->setCurrent(tone);
    refresh();
    pick(tone == 0 ? u"Face"_s : u"Tone %1"_s.arg(toneName(tone)), m_editor.edited().tone(tone),
        [tone](Palette palette, const QColor& chosen) {
            if (tone == 0)
                palette.face = chosen;
            else
                palette.overrides[tone] = chosen;
            return palette;
        });
}

void PalettePage::resetTone(int tone)
{
    Palette palette = m_editor.edited();
    palette.overrides.erase(tone);
    m_editor.edit(palette);
}
