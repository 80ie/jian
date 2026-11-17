#pragma once

#include "media/pulse.h"
#include "widgets/shelfwidget.h"
#include "widgets/soundlayout.h"

#include <optional>

class WindowModel;
struct Palette;

// The default sink's volume and the choice of output. Without a window model it shows what
// setSinks gives it and talks to nothing.
class Sound : public ShelfWidget {
    Q_OBJECT

public:
    Sound(const Palette& palette, WindowModel* windows, QWidget* parent);

    QString id() const override;
    int heightForWidth(int width) const override;
    void setSinks(const Sinks& sinks);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    SoundLayout layout() const;
    void chooseOutput(const QRect& field);
    void dragVolume(int x);

    const Palette& m_palette;
    Sinks m_sinks;
    Pulse* m_pulse = nullptr;
    // The volume under the held thumb.
    std::optional<double> m_volume;
};
