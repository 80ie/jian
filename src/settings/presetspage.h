#pragma once

#include <QWidget>

class PaletteEditor;
class QListWidget;
class QPushButton;

// The presets in palettes/, each drawn as its strip: load one into the editor, save the edited
// palette as one, delete one.
class PresetsPage : public QWidget {
    Q_OBJECT

public:
    PresetsPage(PaletteEditor& editor, QWidget* parent);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void refresh();
    QString selected() const;
    void load();
    void saveAs();
    void save(const QString& name);
    void remove();

    PaletteEditor& m_editor;
    QListWidget* m_list;
    QPushButton* m_load;
    QPushButton* m_delete;
};
