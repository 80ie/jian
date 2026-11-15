#pragma once

#include <QWidget>

class PaletteEditor;
class QCheckBox;
class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
struct Config;
struct Palette;

// The export targets of spec 9, each enabled or not and where it writes, with Export and
// Export on save.
class ExportsPage : public QWidget {
    Q_OBJECT

public:
    ExportsPage(PaletteEditor& editor, Config& config, QString configPath, QWidget* parent);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void refresh();
    void saveEnabled(QTreeWidgetItem* item);
    void updateExport();
    // Writes `palette` to every enabled target and runs each one's command.
    void exportTo(const Palette& palette);

    PaletteEditor& m_editor;
    Config& m_config;
    QString m_configPath;
    QTreeWidget* m_targets;
    QCheckBox* m_onSave;
    QLabel* m_state;
    QPushButton* m_export;
};
