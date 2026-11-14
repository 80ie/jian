#pragma once

#include "palette/palette.h"

#include <QWidget>

#include <vector>

class PaletteEditor;
class QLabel;
class QListWidget;

// Palettes proposed from an image (spec 8.3): the image, the colors it is made of, and the
// candidates built on them. Picking a candidate puts it in the editor.
class ImagePage : public QWidget {
    Q_OBJECT

public:
    ImagePage(PaletteEditor& editor, QWidget* parent);

    void open(const QString& path);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void choose();

    PaletteEditor& m_editor;
    QLabel* m_source;
    QLabel* m_picture;
    QLabel* m_colors;
    QListWidget* m_candidates;
    std::vector<Palette> m_palettes;
};
