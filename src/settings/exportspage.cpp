#include "settings/exportspage.h"
#include "palette/export.h"
#include "panel/config.h"
#include "settings/paletteeditor.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <utility>

using namespace Qt::StringLiterals;

ExportsPage::ExportsPage(PaletteEditor& editor, Config& config, QString configPath, QWidget* parent)
    : QWidget(parent)
    , m_editor(editor)
    , m_config(config)
    , m_configPath(std::move(configPath))
    , m_targets(new QTreeWidget(this))
    , m_onSave(new QCheckBox(u"Export on save"_s, this))
    , m_state(new QLabel(this))
    , m_export(new QPushButton(u"Export"_s, this))
{
    m_targets->setHeaderLabels({ u"Target"_s, u"Destination"_s });
    m_targets->setRootIsDecorated(false);
    m_targets->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    auto* buttons = new QHBoxLayout;
    buttons->addWidget(m_onSave);
    buttons->addWidget(m_state, 1);
    buttons->addWidget(m_export);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_targets, 1);
    layout->addLayout(buttons);

    connect(m_targets, &QTreeWidget::itemChanged, this, &ExportsPage::saveEnabled);
    connect(m_onSave, &QCheckBox::toggled, this, [this](bool on) {
        m_config.exportOnSave = on;
        saveConfig(m_config, m_configPath);
    });
    connect(m_export, &QPushButton::clicked, this, [this] { exportTo(m_editor.edited()); });
    connect(&m_editor, &PaletteEditor::wrote, this, [this] {
        if (m_config.exportOnSave)
            exportTo(m_editor.saved());
    });
    installTargets(m_editor.dir());
    refresh();
}

void ExportsPage::showEvent(QShowEvent*) { refresh(); }

void ExportsPage::refresh()
{
    const QSignalBlocker blockers[] { QSignalBlocker(m_targets), QSignalBlocker(m_onSave) };
    m_onSave->setChecked(m_config.exportOnSave);
    m_targets->clear();
    for (const QString& name : targetNames(m_editor.dir()))
        if (const std::optional<ExportTarget> target = loadTarget(m_editor.dir(), name)) {
            auto* item = new QTreeWidgetItem(m_targets, { name, target->destination });
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(0, target->enabled ? Qt::Checked : Qt::Unchecked);
        }
    updateExport();
}

void ExportsPage::saveEnabled(QTreeWidgetItem* item)
{
    const QString name = item->text(0);
    std::optional<ExportTarget> target = loadTarget(m_editor.dir(), name);
    if (!target)
        return;
    target->enabled = item->checkState(0) == Qt::Checked;
    saveTarget(*target, m_editor.dir(), name);
    updateExport();
}

void ExportsPage::updateExport()
{
    bool any = false;
    for (int i = 0; i < m_targets->topLevelItemCount(); ++i)
        any = any || m_targets->topLevelItem(i)->checkState(0) == Qt::Checked;
    m_export->setEnabled(any);
}

void ExportsPage::exportTo(const Palette& palette)
{
    QStringList failed;
    const QDateTime now = QDateTime::currentDateTime();
    for (const QString& name : targetNames(m_editor.dir())) {
        const std::optional<ExportTarget> target = loadTarget(m_editor.dir(), name);
        if (!target || !target->enabled)
            continue;
        if (!exportTarget(palette, m_editor.dir(), name, now))
            failed.append(name);
        else if (!target->command.isEmpty())
            QProcess::startDetached(u"sh"_s, { u"-c"_s, target->command });
    }
    m_state->setText(failed.isEmpty() ? u"Exported"_s : u"Failed: "_s + failed.join(u", "_s));
}
