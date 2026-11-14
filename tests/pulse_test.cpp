#include "media/pulse.h"

#include <QTest>

using namespace Qt::StringLiterals;

namespace {

// As pactl 17 prints them, cut to the fields read.
constexpr QByteArrayView sinks = R"([{"index":77,"state":"RUNNING","name":"easyeffects_sink",
"description":"Easy Effects Sink","mute":false,"volume":{"front-left":{"value":65536,
"value_percent":"100%","db":"0.00 dB"},"front-right":{"value":65536,"value_percent":"100%",
"db":"0.00 dB"}}},{"index":4705,"state":"SUSPENDED","name":"alsa_output.pci.analog-stereo",
"description":"Built-in Audio Analog Stereo","mute":true,"volume":{"front-left":{"value":29491,
"value_percent":"45%","db":"-20.81 dB"},"front-right":{"value":32768,"value_percent":"50%",
"db":"-18.06 dB"}}}])";

constexpr QByteArrayView info = R"json({"server_name":"PulseAudio (on PipeWire 1.6.11)",
"default_sink_name":"alsa_output.pci.analog-stereo","default_source_name":"alsa_input.pci"})json";

} // namespace

class PulseTest : public QObject {
    Q_OBJECT

private slots:
    void sinkList()
    {
        const std::optional<std::vector<Output>> outputs = parseSinks(sinks.toByteArray());
        QVERIFY(outputs);
        QCOMPARE(outputs->size(), 2u);
        QCOMPARE(
            (*outputs)[0], (Output { u"easyeffects_sink"_s, u"Easy Effects Sink"_s, 1.0, false }));
        QCOMPARE((*outputs)[1].description, u"Built-in Audio Analog Stereo"_s);
        QCOMPARE((*outputs)[1].volume, 0.5);
        QVERIFY((*outputs)[1].muted);

        QCOMPARE(parseSinks("[]"), std::vector<Output>());
        QCOMPARE(parseSinks("Connection failure: Connection refused"), std::nullopt);
    }

    void defaultSink()
    {
        QCOMPARE(parseDefaultSink(info.toByteArray()), u"alsa_output.pci.analog-stereo"_s);
        QCOMPARE(parseDefaultSink(""), QString());

        Sinks s { *parseSinks(sinks.toByteArray()), parseDefaultSink(info.toByteArray()) };
        QCOMPARE(s.current(), &s.outputs[1]);
        s.defaultName = u"gone"_s;
        QCOMPARE(s.current(), nullptr);
    }

    void events_data()
    {
        QTest::addColumn<QByteArray>("line");
        QTest::addColumn<bool>("affects");
        QTest::newRow("sink") << "Event 'change' on sink #4705\n"_ba << true;
        QTest::newRow("new sink") << "Event 'new' on sink #4710\n"_ba << true;
        QTest::newRow("server") << "Event 'change' on server #4294967295\n"_ba << true;
        QTest::newRow("stream") << "Event 'change' on sink-input #2201\n"_ba << false;
        QTest::newRow("source") << "Event 'change' on source #4706\n"_ba << false;
        QTest::newRow("client") << "Event 'remove' on client #4927\n"_ba << false;
    }

    void events()
    {
        QFETCH(QByteArray, line);
        QFETCH(bool, affects);
        QCOMPARE(affectsSinks(line), affects);
    }
};

QTEST_GUILESS_MAIN(PulseTest)
#include "pulse_test.moc"
