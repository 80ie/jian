#include "settings/paletteeditor.h"
#include "palette/library.h"

#include <QDateTime>

#include <utility>

PaletteEditor::PaletteEditor(Palette& live, QDir dir, QObject* parent)
    : QObject(parent)
    , m_live(live)
    , m_dir(std::move(dir))
    , m_edited(live)
    , m_saved(live)
{
}

void PaletteEditor::edit(const Palette& palette)
{
    if (palette == m_edited)
        return;
    m_edited = palette;
    apply();
    emit changed();
}

bool PaletteEditor::save()
{
    if (!savePaletteWithHistory(m_edited, m_dir, QDateTime::currentDateTime()))
        return false;
    m_saved = m_edited;
    emit changed();
    emit wrote();
    return true;
}

void PaletteEditor::load(const Palette& palette)
{
    if (palette == m_saved)
        return;
    const bool followed = !unsaved();
    m_saved = palette;
    if (followed) {
        m_edited = palette;
        apply();
    }
    emit changed();
}

void PaletteEditor::showTone(std::optional<int> tone)
{
    if (tone == m_shown)
        return;
    m_shown = tone;
    apply();
}

void PaletteEditor::apply()
{
    m_live = m_shown ? withToneShown(m_edited, *m_shown) : m_edited;
    emit liveChanged();
}
