#include "settings/settingswindow.h"
#include "panel/panel.h"
#include "panel/windowclass.h"
#include "settings/historypage.h"
#include "settings/imagepage.h"
#include "settings/layoutpage.h"
#include "settings/paletteeditor.h"
#include "settings/palettepage.h"
#include "settings/presetspage.h"
#include "widgets/catalog.h"
#include "widgets/shelfwidget.h"

#include <QCloseEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace {

constexpr int pageListWidth = 140;

} // namespace

SettingsWindow::SettingsWindow(Panel& panel, Config& config, PaletteEditor& palette)
    : m_panel(panel)
    , m_palette(palette)
    , m_pages(new QListWidget(this))
    , m_stack(new QStackedWidget(this))
    , m_state(new QLabel(this))
    , m_revert(new QPushButton(u"Revert"_s, this))
    , m_save(new QPushButton(u"Save"_s, this))
{
    setWindowTitle(u"Shelf settings[*]"_s);
    addPage(u"Layout"_s,
        new LayoutPage(panel, config, palette.dir().filePath(u"config.ini"_s), m_stack));
    addPage(u"Palette"_s, new PalettePage(palette, m_stack));
    addPage(u"From image"_s, new ImagePage(palette, m_stack));
    addPage(u"Presets"_s, new PresetsPage(palette, m_stack));
    addPage(u"History"_s, new HistoryPage(palette, m_stack));
    m_ownPages = m_pages->count();
    makeWidgetPages();
    m_pages->setFixedWidth(pageListWidth);

    auto* pages = new QHBoxLayout;
    pages->addWidget(m_pages);
    pages->addWidget(m_stack, 1);
    auto* footer = new QHBoxLayout;
    footer->addWidget(m_state, 1);
    footer->addWidget(m_revert);
    footer->addWidget(m_save);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(pages, 1);
    layout->addLayout(footer);

    connect(m_pages, &QListWidget::currentRowChanged, m_stack, &QStackedWidget::setCurrentIndex);
    connect(m_revert, &QPushButton::clicked, &m_palette, &PaletteEditor::revert);
    connect(m_save, &QPushButton::clicked, this, [this] {
        if (!m_palette.save())
            m_state->setText(u"palette.ini could not be written."_s);
    });
    connect(&m_palette, &PaletteEditor::changed, this, &SettingsWindow::refreshState);
    connect(&m_panel, &Panel::widgetsChanged, this, &SettingsWindow::makeWidgetPages);
    m_pages->setCurrentRow(0);
    refreshState();
}

// Qt has made the window by now and has yet to map it.
void SettingsWindow::showEvent(QShowEvent*)
{
    setWindowClass(winId(), QGuiApplication::applicationName() + u"-settings"_s);
    makeWidgetPages();
}

void SettingsWindow::closeEvent(QCloseEvent* event)
{
    if (!m_palette.unsaved())
        return;
    event->ignore();
    auto* box = new QMessageBox(QMessageBox::Question, u"Shelf settings"_s,
        u"The palette on the panel has changes that are not saved."_s,
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this);
    box->setAttribute(Qt::WA_DeleteOnClose);
    connect(box, &QMessageBox::buttonClicked, this, [this, box](QAbstractButton* button) {
        const QMessageBox::StandardButton choice = box->standardButton(button);
        if (choice == QMessageBox::Cancel || (choice == QMessageBox::Save && !m_palette.save()))
            return;
        if (choice == QMessageBox::Discard)
            m_palette.revert();
        close();
    });
    box->open();
}

void SettingsWindow::addPage(const QString& title, QWidget* page)
{
    m_pages->addItem(title);
    m_stack->addWidget(page);
}

void SettingsWindow::makeWidgetPages()
{
    const QListWidgetItem* current = m_pages->currentItem();
    const QString shown = current ? current->text() : QString();
    while (m_pages->count() > m_ownPages) {
        delete m_pages->takeItem(m_ownPages);
        delete m_stack->widget(m_ownPages);
    }
    for (const QString& id : m_panel.order())
        if (ShelfWidget* widget = m_panel.widget(id))
            if (QWidget* page = widget->settingsPage())
                addPage(widgetName(id), page);
    const QList<QListWidgetItem*> found = m_pages->findItems(shown, Qt::MatchExactly);
    m_pages->setCurrentRow(found.isEmpty() ? 0 : m_pages->row(found.front()));
}

void SettingsWindow::refreshState()
{
    const bool unsaved = m_palette.unsaved();
    m_state->setText(unsaved ? u"The palette on the panel has unsaved changes."_s
                             : u"The palette is as saved in palette.ini."_s);
    m_revert->setEnabled(unsaved);
    m_save->setEnabled(unsaved);
    setWindowModified(unsaved);
}
