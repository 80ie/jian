#include "settings/historypage.h"
#include "palette/library.h"
#include "settings/paletteeditor.h"
#include "settings/rampstrip.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

HistoryPage::HistoryPage(PaletteEditor& editor, QWidget* parent)
    : QWidget(parent)
    , m_editor(editor)
    , m_list(new QListWidget(this))
    , m_restore(new QPushButton(u"Restore"_s, this))
{
    m_list->setIconSize(stripSize);
    auto* buttons = new QHBoxLayout;
    buttons->addWidget(m_restore);
    buttons->addStretch();
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_list, 1);
    layout->addLayout(buttons);

    connect(m_list, &QListWidget::currentRowChanged, this,
        [this](int row) { m_restore->setEnabled(row >= 0); });
    connect(m_list, &QListWidget::itemActivated, this, &HistoryPage::restore);
    connect(m_restore, &QPushButton::clicked, this, &HistoryPage::restore);
    connect(&m_editor, &PaletteEditor::wrote, this, &HistoryPage::refresh);
    refresh();
}

void HistoryPage::showEvent(QShowEvent*) { refresh(); }

void HistoryPage::refresh()
{
    m_list->clear();
    m_times.clear();
    for (const QDateTime& time : historyTimes(m_editor.dir()))
        if (const std::optional<Palette> palette = loadHistory(m_editor.dir(), time)) {
            m_times.append(time);
            m_list->addItem(new QListWidgetItem(
                paletteStrip(*palette, stripSize), time.toString(u"yyyy-MM-dd  HH:mm:ss"_s)));
        }
    m_restore->setEnabled(false);
}

void HistoryPage::restore()
{
    const int row = m_list->currentRow();
    if (row < 0)
        return;
    if (const std::optional<Palette> palette = loadHistory(m_editor.dir(), m_times[row]))
        m_editor.edit(*palette);
}
