#pragma once

#include "media/player.h"
#include "media/pulse.h"
#include "widgets/medialayout.h"
#include "widgets/mediapaint.h"
#include "widgets/shelfwidget.h"

#include <QImage>
#include <QTimer>

#include <optional>
#include <vector>

class CoverArt;
class Mpris;
class WindowModel;
struct Palette;

// A media player's controls and the default sink's volume: the player, its cover, a scrolling
// title, a trackbar, transport, volume and the output. Without a window model it shows what its
// setters give it and talks to nothing.
class Media : public ShelfWidget {
    Q_OBJECT

public:
    // `source` is the key of the player the controls keep to, or empty to follow whichever
    // started playing last.
    Media(const Palette& palette, const QString& source, WindowModel* windows, QWidget* parent);

    QString id() const override;
    int heightForWidth(int width) const override;
    void setPlayers(const std::vector<Player>& players);
    void setSinks(const Sinks& sinks);
    void setArt(const QImage& art);

signals:
    // The user chose a player key, or automatic as an empty one.
    void sourceChosen(const QString& source);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    const Player* player() const;
    bool showsTrack() const;
    MediaLayout layout() const;
    QString titleText() const;
    void refresh(int heightBefore);
    void restartTimers();
    void setSource(const QString& source);
    void chooseSource(const QRect& field);
    void chooseOutput(const QRect& field);
    void dragVolume(int x);
    void act(Transport which);

    const Palette& m_palette;
    QString m_source;
    std::vector<Player> m_players;
    Sinks m_sinks;
    QImage m_art;
    Mpris* m_mpris = nullptr;
    Pulse* m_pulse = nullptr;
    CoverArt* m_cover = nullptr;
    // What the mouse holds: a transport button, the volume thumb, or a place on the trackbar.
    std::optional<Transport> m_pressed;
    std::optional<double> m_volume;
    std::optional<std::chrono::microseconds> m_seek;
    QString m_title;
    int m_scroll = 0;
    int m_span = 0;
    QTimer m_marquee;
    QTimer m_progress;
};
