#pragma once

#include <QByteArrayView>
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QTimer>

#include <optional>
#include <vector>

struct Output {
    QString name;
    QString description;
    // 1 is 100%, as far as the panel goes.
    double volume = 0;
    bool muted = false;

    bool operator==(const Output&) const = default;
};

// The sinks the sound server has, and which one is the default.
struct Sinks {
    std::vector<Output> outputs;
    QString defaultName;

    // The default sink, or nullptr if there is none.
    const Output* current() const;
    bool operator==(const Sinks&) const = default;
};

// The sinks in the output of `pactl -f json list sinks`, or nothing if it is not that.
std::optional<std::vector<Output>> parseSinks(const QByteArray& json);
// The default sink named in the output of `pactl -f json info`.
QString parseDefaultSink(const QByteArray& json);
// Whether a line from `pactl subscribe` reports a change to a sink or to the server, which holds
// the default.
bool affectsSinks(QByteArrayView line);

// The sound server through pactl: its sinks, kept current by `pactl subscribe`, and the changes
// the widget makes to them. Every call is a subprocess, none waited for.
class Pulse : public QObject {
    Q_OBJECT

public:
    explicit Pulse(QObject* parent);
    ~Pulse() override;

    const Sinks& sinks() const { return m_sinks; }
    // Each of these acts on the default sink, and shows at once.
    void setVolume(double volume);
    void toggleMute();
    void setDefault(const QString& name);

signals:
    void changed();

private:
    void subscribe();
    void read();
    void readDone();
    void write(const QStringList& arguments);
    void writeNext();
    Output* current();

    Sinks m_sinks;
    QProcess m_subscription;
    QTimer m_settle;
    QTimer m_retry;
    int m_retryMs;
    QProcess m_info;
    QProcess m_list;
    // A change came in while a read was under way.
    bool m_stale = false;
    QProcess m_writer;
    std::vector<QStringList> m_writes;
};
