#pragma once

#include "palette/palette.h"

#include <QDir>
#include <QObject>

#include <optional>

// The palette the panel draws, as the settings window edits it, against the one last saved to
// palette.ini. An edit shows on the panel at once and stays unsaved until save().
class PaletteEditor : public QObject {
    Q_OBJECT

public:
    // `live` is the palette the panel draws. `dir` holds palette.ini, presets and history.
    PaletteEditor(Palette& live, QDir dir, QObject* parent = nullptr);

    const Palette& edited() const { return m_edited; }
    const Palette& saved() const { return m_saved; }
    bool unsaved() const { return m_edited != m_saved; }
    const QDir& dir() const { return m_dir; }

    void edit(const Palette& palette);
    // Writes palette.ini and a copy into history.
    bool save();
    void revert() { edit(m_saved); }
    // palette.ini changed on disk. Unsaved edits are kept, and Revert goes to the new file.
    void load(const Palette& palette);
    // Shows on the panel where `tone` is used, or stops with nullopt. The edit is unchanged.
    void showTone(std::optional<int> tone);

signals:
    // The edited palette or the saved one changed.
    void changed();
    // save() wrote a copy into history.
    void wrote();
    // What the panel draws changed.
    void liveChanged();

private:
    void apply();

    Palette& m_live;
    QDir m_dir;
    Palette m_edited;
    Palette m_saved;
    std::optional<int> m_shown;
};
