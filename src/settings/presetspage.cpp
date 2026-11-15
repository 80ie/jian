#include "settings/presetspage.h"
#include "palette/library.h"
#include "settings/paletteeditor.h"
#include "settings/rampstrip.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <functional>

using namespace Qt::StringLiterals;

namespace {

// Asks `question`, and runs `then` on yes.
void confirm(QWidget* parent, const QString& question, const std::function<void()>& then)
{
    auto* box = new QMessageBox(
        QMessageBox::Question, u"Presets"_s, question, QMessageBox::Yes | QMessageBox::No, parent);
    box->setAttribute(Qt::WA_DeleteOnClose);
    QObject::connect(box, &QMessageBox::accepted, parent, then);
    box->open();
}

} // namespace

PresetsPage::PresetsPage(PaletteEditor& editor, QWidget* parent)
    : QWidget(parent)
    , m_editor(editor)
    , m_list(new QListWidget(this))
    , m_load(new QPushButton(u"Load"_s, this))
    , m_delete(new QPushButton(u"Delete…"_s, this))
{
    m_list->setIconSize(stripSize);
    auto* saveAs = new QPushButton(u"Save as…"_s, this);
    auto* buttons = new QHBoxLayout;
    buttons->addWidget(m_load);
    buttons->addWidget(saveAs);
    buttons->addStretch();
    buttons->addWidget(m_delete);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_list, 1);
    layout->addLayout(buttons);

    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        m_load->setEnabled(row >= 0);
        m_delete->setEnabled(row >= 0);
    });
    connect(m_list, &QListWidget::itemActivated, this, &PresetsPage::load);
    connect(m_load, &QPushButton::clicked, this, &PresetsPage::load);
    connect(saveAs, &QPushButton::clicked, this, &PresetsPage::saveAs);
    connect(m_delete, &QPushButton::clicked, this, &PresetsPage::remove);
    refresh();
}

void PresetsPage::showEvent(QShowEvent*) { refresh(); }

void PresetsPage::refresh()
{
    const QString kept = selected();
    m_list->clear();
    for (const QString& name : presetNames(m_editor.dir()))
        if (const std::optional<Palette> palette = loadPreset(m_editor.dir(), name))
            m_list->addItem(new QListWidgetItem(paletteStrip(*palette, stripSize), name));
    const QList<QListWidgetItem*> found = m_list->findItems(kept, Qt::MatchExactly);
    m_list->setCurrentRow(found.isEmpty() ? -1 : m_list->row(found.front()));
    m_load->setEnabled(!found.isEmpty());
    m_delete->setEnabled(!found.isEmpty());
}

QString PresetsPage::selected() const
{
    const QListWidgetItem* item = m_list->currentItem();
    return item ? item->text() : QString();
}

void PresetsPage::load()
{
    if (const std::optional<Palette> palette = loadPreset(m_editor.dir(), selected()))
        m_editor.edit(*palette);
}

void PresetsPage::saveAs()
{
    auto* dialog = new QInputDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(u"Save as preset"_s);
    dialog->setLabelText(u"Name:"_s);
    dialog->setTextValue(selected());
    connect(dialog, &QInputDialog::textValueSelected, this, [this](const QString& name) {
        if (!isPresetName(name))
            return;
        if (presetNames(m_editor.dir()).contains(name))
            confirm(this, u"Replace “%1”?"_s.arg(name), [this, name] { save(name); });
        else
            save(name);
    });
    dialog->open();
}

void PresetsPage::save(const QString& name)
{
    if (savePreset(m_editor.edited(), m_editor.dir(), name)) {
        refresh();
        m_list->setCurrentItem(m_list->findItems(name, Qt::MatchExactly).value(0));
    }
}

void PresetsPage::remove()
{
    const QString name = selected();
    confirm(this, u"Delete “%1”?"_s.arg(name), [this, name] {
        deletePreset(m_editor.dir(), name);
        refresh();
    });
}
