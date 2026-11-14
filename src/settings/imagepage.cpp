#include "settings/imagepage.h"
#include "palette/extract.h"
#include "settings/paletteeditor.h"
#include "settings/rampstrip.h"

#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QImageReader>
#include <QLabel>
#include <QListWidget>
#include <QMimeData>
#include <QPainter>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

#include <span>

using namespace Qt::StringLiterals;

namespace {

constexpr QSize pictureSize(320, 180);

// The image's colors side by side, each as wide as its share of the image.
QPixmap colorsStrip(std::span<const ImageColor> colors, QSize size)
{
    QPixmap picture(size);
    picture.fill(Qt::transparent);
    QPainter p(&picture);
    double at = 0;
    for (const ImageColor& color : colors) {
        const int left = qRound(at * size.width());
        at += color.share;
        p.fillRect(left, 0, qRound(at * size.width()) - left, size.height(), color.color);
    }
    return picture;
}

QString colorsList(std::span<const ImageColor> colors)
{
    QStringList lines;
    for (const ImageColor& color : colors)
        lines.append(u"%1  %2%"_s.arg(color.color.name()).arg(color.share * 100, 0, 'f', 1));
    return lines.join(u'\n');
}

QString imageFilter()
{
    QStringList patterns;
    for (const QByteArray& format : QImageReader::supportedImageFormats())
        patterns.append(u"*."_s + QString::fromLatin1(format));
    return u"Images (%1)"_s.arg(patterns.join(u' '));
}

QLabel* wrapped(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setWordWrap(true);
    return label;
}

} // namespace

ImagePage::ImagePage(PaletteEditor& editor, QWidget* parent)
    : QWidget(parent)
    , m_editor(editor)
    , m_source(new QLabel(u"or drag-and-drop."_s, this))
    , m_picture(new QLabel(this))
    , m_colors(new QLabel(this))
    , m_candidates(new QListWidget(this))
{
    setAcceptDrops(true);
    auto* openButton = new QPushButton(u"Open image…"_s, this);
    auto* top = new QHBoxLayout;
    top->addWidget(openButton);
    top->addWidget(m_source, 1);
    m_candidates->setIconSize(stripSize);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(m_picture);
    layout->addWidget(wrapped(u"Distribution:"_s, this));
    layout->addWidget(m_colors);
    layout->addWidget(wrapped(u"Candidates:", this));
    layout->addWidget(m_candidates, 1);

    connect(openButton, &QPushButton::clicked, this, &ImagePage::choose);
    connect(m_candidates, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        m_editor.edit(m_palettes[std::size_t(m_candidates->row(item))]);
    });
}

void ImagePage::open(const QString& path)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        m_source->setText(u"%1: %2"_s.arg(QFileInfo(path).fileName(), reader.errorString()));
        return;
    }
    m_source->setText(path);
    m_picture->setPixmap(QPixmap::fromImage(
        image.scaled(pictureSize, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    const std::vector<ImageColor> colors = imageColors(image);
    m_colors->setPixmap(colorsStrip(colors, stripSize));
    m_colors->setToolTip(colorsList(colors));
    m_palettes = candidates(colors);
    m_candidates->clear();
    for (const Palette& palette : m_palettes)
        m_candidates->addItem(new QListWidgetItem(
            paletteStrip(palette, stripSize), u"face %1"_s.arg(palette.face.name())));
}

void ImagePage::dragEnterEvent(QDragEnterEvent* event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty() && urls.front().isLocalFile())
        event->acceptProposedAction();
}

void ImagePage::dropEvent(QDropEvent* event)
{
    open(event->mimeData()->urls().front().toLocalFile());
}

void ImagePage::choose()
{
    auto* dialog = new QFileDialog(this, u"Open image"_s,
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation), imageFilter());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setFileMode(QFileDialog::ExistingFile);
    connect(dialog, &QFileDialog::fileSelected, this, &ImagePage::open);
    dialog->open();
}
