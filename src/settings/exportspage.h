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
    void exportTo(const Palette& palette);

    PaletteEditor& m_editor;
    Config& m_config;
    QString m_configPath;
    QTreeWidget* m_targets;
    QCheckBox* m_onSave;
    QLabel* m_state;
    QPushButton* m_export;
};
