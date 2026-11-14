#include "settings/layoutpage.h"
#include "panel/config.h"
#include "panel/order.h"
#include "panel/panel.h"
#include "widgets/catalog.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

#include <utility>

using namespace Qt::StringLiterals;

LayoutPage::LayoutPage(Panel& panel, Config& config, QString configPath, QWidget* parent)
    : QWidget(parent)
    , m_panel(panel)
    , m_config(config)
    , m_configPath(std::move(configPath))
    , m_left(new QRadioButton(u"Left"_s, this))
    , m_right(new QRadioButton(u"Right"_s, this))
    , m_width(new QSpinBox(this))
    , m_widgets(new QListWidget(this))
    , m_up(new QPushButton(u"Up"_s, this))
    , m_down(new QPushButton(u"Down"_s, this))
    , m_remove(new QPushButton(u"Remove"_s, this))
    , m_add(new QToolButton(this))
    , m_edit(new QPushButton(u"Edit widgets on the panel"_s, this))
{
    auto* sides = new QHBoxLayout;
    sides->addWidget(m_left);
    sides->addWidget(m_right);
    sides->addStretch();
    m_width->setRange(narrowestPanel, widestPanel);
    m_width->setSuffix(u" px"_s);
    m_width->setKeyboardTracking(false);
    auto* form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    form->addRow(u"Side"_s, sides);
    form->addRow(u"Width"_s, m_width);

    m_add->setText(u"Add"_s);
    m_add->setPopupMode(QToolButton::InstantPopup);
    m_add->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_add->setMenu(new QMenu(m_add));
    auto* buttons = new QVBoxLayout;
    for (QWidget* button : std::initializer_list<QWidget*> { m_up, m_down, m_remove, m_add })
        buttons->addWidget(button);
    buttons->addStretch();
    auto* order = new QHBoxLayout;
    order->addWidget(m_widgets, 1);
    order->addLayout(buttons);
    m_edit->setCheckable(true);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(new QLabel(u"Widgets, top to bottom:"_s, this));
    layout->addLayout(order, 1);
    layout->addWidget(m_edit);

    connect(m_left, &QRadioButton::toggled, this, [this](bool on) {
        if (on)
            setSide(Qt::LeftEdge);
    });
    connect(m_right, &QRadioButton::toggled, this, [this](bool on) {
        if (on)
            setSide(Qt::RightEdge);
    });
    connect(m_width, &QSpinBox::valueChanged, this, [this](int width) {
        m_config.width = width;
        m_panel.setWidth(width);
        save();
    });
    connect(m_widgets, &QListWidget::currentRowChanged, this, &LayoutPage::updateButtons);
    connect(m_up, &QPushButton::clicked, this, [this] { move(-1); });
    connect(m_down, &QPushButton::clicked, this, [this] { move(1); });
    connect(m_remove, &QPushButton::clicked, this, [this] {
        m_panel.remove(selected());
        reordered();
    });
    connect(m_add->menu(), &QMenu::triggered, this, [this](QAction* action) {
        m_panel.put(action->data().toString(), QString());
        reordered();
    });
    connect(m_edit, &QPushButton::toggled, &m_panel, &Panel::setEditing);
    connect(&m_panel, &Panel::arranged, this, &LayoutPage::refresh);
    connect(&m_panel, &Panel::widgetsChanged, this, &LayoutPage::refresh);
    connect(&m_panel, &Panel::widthDragged, this, &LayoutPage::refresh);
    connect(&m_panel, &Panel::sideSwitched, this, &LayoutPage::refresh);
    connect(&m_panel, &Panel::editingChanged, this, &LayoutPage::refresh);
    refresh();
}

void LayoutPage::refresh()
{
    const QString kept = selected();
    const QStringList order = m_panel.order();
    const QSignalBlocker blockers[] { QSignalBlocker(m_left), QSignalBlocker(m_right),
        QSignalBlocker(m_width), QSignalBlocker(m_widgets), QSignalBlocker(m_edit) };
    m_left->setChecked(m_panel.side() == Qt::LeftEdge);
    m_right->setChecked(m_panel.side() == Qt::RightEdge);
    m_width->setValue(m_panel.width());
    m_edit->setChecked(m_panel.editing());

    m_widgets->clear();
    const QStringList popped = m_panel.poppedOut();
    for (const QString& id : order) {
        QString text = widgetName(id);
        if (!m_panel.widget(id))
            text += u" (cannot run here)"_s;
        else if (popped.contains(id))
            text += u" (in a window)"_s;
        auto* item = new QListWidgetItem(text, m_widgets);
        item->setData(Qt::UserRole, id);
    }
    m_widgets->setCurrentRow(int(order.indexOf(kept)));
    updateButtons();

    m_add->menu()->clear();
    for (const QString& id : unplaced(m_panel.offered(), order))
        m_add->menu()->addAction(widgetName(id))->setData(id);
    m_add->setEnabled(!m_add->menu()->isEmpty());
}

void LayoutPage::updateButtons()
{
    const int row = m_widgets->currentRow();
    m_up->setEnabled(row > 0);
    m_down->setEnabled(row >= 0 && row + 1 < m_widgets->count());
    m_remove->setEnabled(row >= 0);
}

QString LayoutPage::selected() const
{
    const QListWidgetItem* item = m_widgets->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void LayoutPage::setSide(Qt::Edge side)
{
    m_config.side = side;
    m_panel.setSide(side);
    save();
}

// Moving down is moving the next widget in front of this one.
void LayoutPage::move(int by)
{
    const QStringList order = m_panel.order();
    const qsizetype at = order.indexOf(selected());
    if (by < 0)
        m_panel.put(order[at], order[at - 1]);
    else
        m_panel.put(order[at + 1], order[at]);
    reordered();
}

void LayoutPage::reordered()
{
    m_config.widgets = m_panel.order();
    m_config.poppedOut = m_panel.poppedOut();
    save();
    refresh();
}

void LayoutPage::save() { saveConfig(m_config, m_configPath); }
