#pragma once

#include "widgets/clocklayout.h"
#include "widgets/shelfwidget.h"

#include <QDateTime>
#include <QTimer>

struct Palette;

// Whether a QDateTime format shows seconds, which decides how often the clock changes.
bool showsSeconds(QStringView format);
// How long from `now` until a clock that shows seconds, or only minutes, next changes.
int msecsToNextTick(const QTime& now, bool seconds);
// The format tokens, each beside what it gives at `now`, in columns for a fixed-pitch font.
QString syntaxSheet(const QDateTime& now);

// The time at the left end and the date at the right, straight on the panel face.
class Clock : public ShelfWidget {
    Q_OBJECT

public:
    Clock(const Palette& palette, const QString& timeFormat, const QString& dateFormat,
        const ClockSizes& sizes, QWidget* parent);

    QString id() const override;
    int heightForWidth(int width) const override;
    // The two formats, each with a preview and its size.
    QWidget* settingsPage() override;
    // Shows `time` until the next tick. Ticks follow the wall clock while the clock is visible.
    void setTime(const QDateTime& time);
    void setFormats(const QString& timeFormat, const QString& dateFormat);
    void setSizes(const ClockSizes& sizes);

signals:
    // setFormats() changed them.
    void formatsChosen(const QString& timeFormat, const QString& dateFormat);
    void sizesChosen(const ClockSizes& sizes);

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void tick();

    const Palette& m_palette;
    QString m_timeFormat;
    QString m_dateFormat;
    ClockSizes m_sizes;
    bool m_seconds = false;
    QString m_time;
    QString m_date;
    QTimer m_timer;
};
