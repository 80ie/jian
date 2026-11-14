#pragma once

#include <QWidget>

class Panel;
class PaletteEditor;
class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;
struct Config;

// The settings window of spec 10: the pages listed beside the one shown, and under them whether
// the palette has unsaved changes, with Revert and Save. The pages edit the objects the panel
// draws from.
class SettingsWindow : public QWidget {
    Q_OBJECT

public:
    SettingsWindow(Panel& panel, Config& config, PaletteEditor& palette);

protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void addPage(const QString& title, QWidget* page);
    // One page for each placed widget that has settings, after the window's own.
    void makeWidgetPages();
    void refreshState();

    Panel& m_panel;
    PaletteEditor& m_palette;
    QListWidget* m_pages;
    QStackedWidget* m_stack;
    QLabel* m_state;
    QPushButton* m_revert;
    QPushButton* m_save;
    int m_ownPages = 0;
};
