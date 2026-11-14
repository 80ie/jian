#pragma once

#include <QWidget>

class Panel;
class QListWidget;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QToolButton;
struct Config;

// Where the panel sits and what it holds: its side, its width and its widgets in order, with
// edit mode a button away. A change goes to the panel at once and into config.ini.
class LayoutPage : public QWidget {
    Q_OBJECT

public:
    LayoutPage(Panel& panel, Config& config, QString configPath, QWidget* parent);

private:
    void refresh();
    void updateButtons();
    QString selected() const;
    void setSide(Qt::Edge side);
    void move(int by);
    // The panel's widgets changed from here; config.ini follows.
    void reordered();
    void save();

    Panel& m_panel;
    Config& m_config;
    QString m_configPath;
    QRadioButton* m_left;
    QRadioButton* m_right;
    QSpinBox* m_width;
    QListWidget* m_widgets;
    QPushButton* m_up;
    QPushButton* m_down;
    QPushButton* m_remove;
    QToolButton* m_add;
    QPushButton* m_edit;
};
