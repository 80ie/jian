#pragma once

#include <QDateTime>
#include <QList>
#include <QWidget>

class PaletteEditor;
class QListWidget;
class QPushButton;

// The copies of palette.ini that each save left in history/, newest first, each drawn as its
// strip. Restoring one puts it in the editor.
class HistoryPage : public QWidget {
    Q_OBJECT

public:
    HistoryPage(PaletteEditor& editor, QWidget* parent);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void refresh();
    void restore();

    PaletteEditor& m_editor;
    QListWidget* m_list;
    QPushButton* m_restore;
    QList<QDateTime> m_times;
};
