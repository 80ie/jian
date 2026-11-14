#include "media/pulse.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <csignal>
#include <utility>

#include <sys/prctl.h>

using namespace Qt::StringLiterals;

namespace {

// PA_VOLUME_NORM: 100% on the scale pactl and its percentages use.
constexpr double fullVolume = 0x10000;
// A burst of events, as one change of volume brings, wants one read.
constexpr int settleMs = 30;
constexpr int firstRetryMs = 2000;
constexpr int lastRetryMs = 64'000;

const QString pactl = u"pactl"_s;
const QString defaultSink = u"@DEFAULT_SINK@"_s;

double loudest(const QJsonObject& channels)
{
    double volume = 0;
    for (const QJsonValue& channel : channels)
        volume = std::max(volume, channel[u"value"].toDouble() / fullVolume);
    return volume;
}

} // namespace

const Output* Sinks::current() const
{
    const auto output = std::ranges::find(outputs, defaultName, &Output::name);
    return output == outputs.end() ? nullptr : &*output;
}

std::optional<std::vector<Output>> parseSinks(const QByteArray& json)
{
    const QJsonDocument document = QJsonDocument::fromJson(json);
    if (!document.isArray())
        return std::nullopt;
    std::vector<Output> outputs;
    for (const QJsonValue& sink : document.array())
        outputs.push_back({ sink[u"name"].toString(), sink[u"description"].toString(),
            loudest(sink[u"volume"].toObject()), sink[u"mute"].toBool() });
    return outputs;
}

QString parseDefaultSink(const QByteArray& json)
{
    return QJsonDocument::fromJson(json)[u"default_sink_name"].toString();
}

bool affectsSinks(QByteArrayView line)
{
    return line.contains("' on sink #") || line.contains("' on server");
}

Pulse::Pulse(QObject* parent)
    : QObject(parent)
    , m_retryMs(firstRetryMs)
{
    m_settle.setSingleShot(true);
    m_settle.setInterval(settleMs);
    connect(&m_settle, &QTimer::timeout, this, &Pulse::read);
    m_retry.setSingleShot(true);
    connect(&m_retry, &QTimer::timeout, this, &Pulse::subscribe);

    m_subscription.setProgram(pactl);
    m_subscription.setArguments({ u"subscribe"_s });
    // A panel killed outright would otherwise leave one behind for every restart.
    m_subscription.setChildProcessModifier([] { prctl(PR_SET_PDEATHSIG, SIGTERM); });
    connect(&m_subscription, &QProcess::readyReadStandardOutput, this, [this] {
        while (m_subscription.canReadLine())
            if (affectsSinks(m_subscription.readLine()))
                m_settle.start();
    });
    // The subscription ends with the server, which pipewire-pulse does when it restarts.
    connect(&m_subscription, &QProcess::finished, this, [this] {
        if (m_retryMs == firstRetryMs)
            qWarning("media: lost the sound server, trying again");
        m_retry.start(m_retryMs);
        m_retryMs = std::min(2 * m_retryMs, lastRetryMs);
    });
    connect(&m_subscription, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            qWarning("media: %s", qUtf8Printable(m_subscription.errorString()));
    });

    for (QProcess* reader : { &m_info, &m_list })
        connect(reader, &QProcess::finished, this, &Pulse::readDone);
    m_writer.setProcessChannelMode(QProcess::ForwardedChannels);
    connect(&m_writer, &QProcess::finished, this, &Pulse::writeNext);
    connect(&m_writer, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            m_writes.clear();
    });
    subscribe();
}

Pulse::~Pulse()
{
    for (QProcess* process : { &m_subscription, &m_info, &m_list, &m_writer })
        process->disconnect(this);
}

void Pulse::setVolume(double volume)
{
    volume = std::clamp(volume, 0.0, 1.0);
    write({ u"set-sink-volume"_s, defaultSink, QString::number(qRound(volume * fullVolume)) });
    if (Output* output = current()) {
        output->volume = volume;
        emit changed();
    }
}

void Pulse::toggleMute() { write({ u"set-sink-mute"_s, defaultSink, u"toggle"_s }); }

void Pulse::setDefault(const QString& name)
{
    write({ u"set-default-sink"_s, name });
    m_sinks.defaultName = name;
    emit changed();
}

void Pulse::subscribe()
{
    m_subscription.start();
    read();
}

void Pulse::read()
{
    if (m_info.state() != QProcess::NotRunning || m_list.state() != QProcess::NotRunning) {
        m_stale = true;
        return;
    }
    m_info.start(pactl, { u"-f"_s, u"json"_s, u"info"_s });
    m_list.start(pactl, { u"-f"_s, u"json"_s, u"list"_s, u"sinks"_s });
}

void Pulse::readDone()
{
    if (m_info.state() != QProcess::NotRunning || m_list.state() != QProcess::NotRunning)
        return;
    std::optional<std::vector<Output>> outputs = parseSinks(m_list.readAllStandardOutput());
    const QString defaultName = parseDefaultSink(m_info.readAllStandardOutput());
    if (outputs && !defaultName.isEmpty()) {
        m_retryMs = firstRetryMs;
        Sinks sinks { std::move(*outputs), defaultName };
        if (sinks != m_sinks) {
            m_sinks = std::move(sinks);
            emit changed();
        }
    }
    if (std::exchange(m_stale, false))
        read();
}

// Writes run one at a time, in order. A volume makes one still queued moot, so that a drag
// across the slider does not queue a write for every pixel.
void Pulse::write(const QStringList& arguments)
{
    if (!m_writes.empty() && m_writes.back().first() == u"set-sink-volume"
        && arguments.first() == u"set-sink-volume")
        m_writes.back() = arguments;
    else
        m_writes.push_back(arguments);
    writeNext();
}

void Pulse::writeNext()
{
    if (m_writer.state() != QProcess::NotRunning || m_writes.empty())
        return;
    m_writer.start(pactl, m_writes.front());
    m_writes.erase(m_writes.begin());
}

Output* Pulse::current()
{
    const auto output = std::ranges::find(m_sinks.outputs, m_sinks.defaultName, &Output::name);
    return output == m_sinks.outputs.end() ? nullptr : &*output;
}
