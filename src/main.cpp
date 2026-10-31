#include <QApplication>
#include <QAudioOutput>
#include <QCoreApplication>
#include <QPointer>
#include <QScreen>
#include <QPoint>
#include <QDialog>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMediaPlayer>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QFrame>
#include <QDir>
#include <QDebug>
#include <QByteArray>
#include <QSize>
#include <QRect>
#include <QStringList>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <vector>

struct MidiEvent {
    double timeSeconds = 0.0;
    int instrumentIndex = 0;
    int pitch = 0;
};

struct DialogData {
    QString title;
    QString message;
    QStringList buttons;
    QSize size;
    QString icon;
};

static const std::map<int, DialogData> DIALOGS = {
    {24, {
        QStringLiteral("NVIDIA Setup Error"),
        QStringLiteral(
            "Setup detected that the operating system in use is not Windows 2000 or XP.\n\n"
            "This setup program and its associated drivers are designed to run only on Windows 2000 or XP."
        ),
        {QStringLiteral("OK")},
        QSize(400, 210),
        QStringLiteral("⛔")
    }},

    {31, {
        QStringLiteral("曖昧さ回避"),
        QStringLiteral("曖昧さ回避"),
        {QStringLiteral("OK"), QStringLiteral("Cancel")},
        QSize(420, 180),
        QStringLiteral("⚠")
    }},

    {40, {
        QStringLiteral("曖昧さ回避"),
        QStringLiteral("Do you want to save 曖昧さ回避.txt?"),
        {QStringLiteral("Yes"), QStringLiteral("No"), QStringLiteral("Cancel")},
        QSize(420, 160),
        QStringLiteral("❔")
    }},

    {26, {
        QStringLiteral("タスクが正常に失敗しました"),
        QStringLiteral("タスクが正常に失敗しました"),
        {QStringLiteral("OK")},
        QSize(270, 155),
        QStringLiteral("⚠")
    }},

    {25, {
        QStringLiteral("エラー"),
        QStringLiteral("タスクが正常に失敗しました"),
        {QStringLiteral("OK")},
        QSize(300, 155),
        QStringLiteral("❌")
    }},

    {35, {
        QStringLiteral("エクスプローラー"),
        QStringLiteral("Windows Explorer has stopped working."),
        {QStringLiteral("Close the program")},
        QSize(390, 165),
        QStringLiteral("❌")
    }},

    {29, {
        QStringLiteral("エラー"),
        QStringLiteral("エラーが発生しました。"),
        {QStringLiteral("OK")},
        QSize(320, 155),
        QStringLiteral("❌")
    }},

    {30, {
        QStringLiteral("エラこ"),
        QStringLiteral("エラーが発生しました。"),
        {QStringLiteral("OK")},
        QSize(320, 155),
        QStringLiteral("❌")
    }},

    {32, {
        QStringLiteral("エラー"),
        QStringLiteral("予期しないエラーが発生しました。"),
        {QStringLiteral("OK")},
        QSize(350, 155),
        QStringLiteral("⛔")
    }},

    {38, {
        QStringLiteral("インフォ"),
        QStringLiteral("通知があります。"),
        {QStringLiteral("OK")},
        QSize(300, 150),
        QStringLiteral("ℹ")
    }},

    {37, {
        QStringLiteral("バッテリー"),
        QStringLiteral("バッテリー残量が少なくなっています。"),
        {QStringLiteral("OK")},
        QSize(340, 150),
        QStringLiteral("🔋")
    }},

    {27, {
        QStringLiteral("あまりにも多くのエラー笑"),
        QStringLiteral("あまりにも多くのエラー笑"),
        {QStringLiteral("OK")},
        QSize(350, 155),
        QStringLiteral("⚠")
    }},

    {39, {
        QStringLiteral("曖昧さ回避"),
        QStringLiteral("曖昧さ回避 \"C:\\WINDOWS\""),
        {QStringLiteral("OK"), QStringLiteral("Yes"), QStringLiteral("Cancel")},
        QSize(420, 170),
        QStringLiteral("⚠")
    }},

    {33, {
        QStringLiteral("ワオ"),
        QStringLiteral("ワオ！"),
        {QStringLiteral("OK")},
        QSize(280, 145),
        QStringLiteral("ℹ")
    }},

    {34, {
        QStringLiteral("インフォ"),
        QStringLiteral("予期しない問題が発生しました。"),
        {QStringLiteral("OK"), QStringLiteral("Yes"), QStringLiteral("Cancel")},
        QSize(380, 160),
        QStringLiteral("❌")
    }}
};


// Minimal Standard MIDI File reader.
// Extracts note-on events and applies the global tempo map.
namespace MidiReader {

struct RawNote {
    quint64 tick;
    int instrumentIndex;
    int pitch;
};

struct Tempo {
    quint64 tick;
    quint32 microsPerQuarter;
    int order;
};

static quint16 readU16(const QByteArray &b, qsizetype &p) {
    if (p + 2 > b.size())
        throw std::runtime_error("Unexpected end of MIDI file");

    quint16 v = (quint8(b[p]) << 8) | quint8(b[p + 1]);
    p += 2;

    return v;
}

static quint32 readU32(const QByteArray &b, qsizetype &p) {
    if (p + 4 > b.size())
        throw std::runtime_error("Unexpected end of MIDI file");

    quint32 v =
        (quint32(quint8(b[p])) << 24) |
        (quint32(quint8(b[p + 1])) << 16) |
        (quint32(quint8(b[p + 2])) << 8) |
        quint32(quint8(b[p + 3]));

    p += 4;
    return v;
}

static quint32 readVarLen(
    const QByteArray &b,
    qsizetype &p,
    qsizetype end
) {
    quint32 value = 0;

    for (int i = 0; i < 4; ++i) {
        if (p >= end)
            throw std::runtime_error("Invalid MIDI variable-length quantity");

        quint8 c = quint8(b[p++]);

        value = (value << 7) | (c & 0x7f);

        if (!(c & 0x80))
            return value;
    }

    throw std::runtime_error("MIDI variable-length quantity is too long");
}

static std::vector<MidiEvent> load(const QString &path) {
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error(
            QString("Cannot open MIDI file: %1")
                .arg(path)
                .toStdString()
        );
    }

    QByteArray bytes = file.readAll();
    qsizetype p = 0;

    if (bytes.size() < 14 || bytes.mid(0, 4) != "MThd")
        throw std::runtime_error("Invalid Standard MIDI File header");

    p = 4;

    quint32 headerLength = readU32(bytes, p);

    if (headerLength < 6 || p + headerLength > bytes.size())
        throw std::runtime_error("Invalid MIDI header length");

    quint16 format = readU16(bytes, p);
    quint16 trackCount = readU16(bytes, p);
    quint16 division = readU16(bytes, p);

    p = 8 + headerLength;

    if (format > 1)
        throw std::runtime_error("MIDI format 2 is not supported");

    if (division & 0x8000)
        throw std::runtime_error("SMPTE-time MIDI files are not supported");

    if (division == 0)
        throw std::runtime_error("Invalid MIDI ticks-per-quarter-note value");

    std::vector<RawNote> rawNotes;
    std::vector<Tempo> tempos;

    int tempoOrder = 0;

    std::map<int, int> instrumentIndices;
    int nextInstrumentIndex = 0;

    for (int track = 0; track < trackCount; ++track) {
        if (p + 8 > bytes.size() || bytes.mid(p, 4) != "MTrk")
            throw std::runtime_error("MIDI track chunk is missing");

        p += 4;

        quint32 trackLength = readU32(bytes, p);
        qsizetype trackEnd = p + trackLength;

        if (trackEnd > bytes.size())
            throw std::runtime_error("Truncated MIDI track");

        quint64 tick = 0;
        quint8 runningStatus = 0;

        int programs[16] = {};

        std::map<std::tuple<int, int>, std::deque<quint64>> activeNotes;

        while (p < trackEnd) {
            tick += readVarLen(bytes, p, trackEnd);

            if (p >= trackEnd)
                break;

            quint8 first = quint8(bytes[p]);
            quint8 status;

            bool hasStatusByte = (first & 0x80) != 0;

            if (hasStatusByte) {
                status = first;
                ++p;

                if (status < 0xf0) {
                    runningStatus = status;
                } else if (
                    status != 0xf8 &&
                    status != 0xfa &&
                    status != 0xfb &&
                    status != 0xfc &&
                    status != 0xfe
                ) {
                    runningStatus = 0;
                }
            } else {
                if (!runningStatus)
                    throw std::runtime_error("Invalid MIDI running status");

                status = runningStatus;
            }

            if (status == 0xff) {
                if (p >= trackEnd)
                    throw std::runtime_error("Truncated MIDI meta event");

                quint8 type = quint8(bytes[p++]);
                quint32 len = readVarLen(bytes, p, trackEnd);

                if (p + len > trackEnd)
                    throw std::runtime_error("Truncated MIDI meta payload");

                if (type == 0x51 && len == 3) {
                    quint32 us =
                        (quint32(quint8(bytes[p])) << 16) |
                        (quint32(quint8(bytes[p + 1])) << 8) |
                        quint32(quint8(bytes[p + 2]));

                    if (us > 0)
                        tempos.push_back({tick, us, tempoOrder++});
                }

                p += len;

                if (type == 0x2f) {
                    p = trackEnd;
                    break;
                }

                continue;
            }

            if (status == 0xf0 || status == 0xf7) {
                quint32 len = readVarLen(bytes, p, trackEnd);

                if (p + len > trackEnd)
                    throw std::runtime_error("Truncated MIDI SysEx event");

                p += len;
                continue;
            }

            if (status >= 0xf0) {
                int n =
                    (status == 0xf1 || status == 0xf3) ? 1 :
                    (status == 0xf2 ? 2 : 0);

                p += n;

                if (p > trackEnd)
                    throw std::runtime_error("Truncated MIDI system event");

                continue;
            }

            int command = status & 0xf0;
            int channel = status & 0x0f;

            int dataBytes =
                (command == 0xc0 || command == 0xd0) ? 1 : 2;

            if (p + dataBytes > trackEnd)
                throw std::runtime_error("Truncated MIDI channel event");

            int d1 = quint8(bytes[p++]) & 0x7f;
            int d2 = dataBytes == 2
                ? (quint8(bytes[p++]) & 0x7f)
                : 0;

            if (command == 0xc0) {
                programs[channel] = d1;
            } else if (command == 0x90 && d2 > 0) {
                int key = channel * 128 + programs[channel];

                auto it = instrumentIndices.find(key);

                if (it == instrumentIndices.end()) {
                    it = instrumentIndices.emplace(
                        key,
                        nextInstrumentIndex++
                    ).first;
                }

                rawNotes.push_back({
                    tick,
                    it->second,
                    d1
                });

                activeNotes[{channel, d1}].push_back(tick);

            } else if (command == 0x80 || (command == 0x90 && d2 == 0)) {
                auto it = activeNotes.find({channel, d1});

                if (it != activeNotes.end() && !it->second.empty())
                    it->second.pop_front();
            }
        }

        p = trackEnd;
    }

    if (rawNotes.empty())
        return {};

    if (tempos.empty())
        tempos.push_back({0, 500000, -1});

    std::stable_sort(
        tempos.begin(),
        tempos.end(),
        [](const Tempo &a, const Tempo &b) {
            if (a.tick != b.tick)
                return a.tick < b.tick;

            return a.order < b.order;
        }
    );

    std::vector<Tempo> compact;

    for (const auto &t : tempos) {
        if (!compact.empty() && compact.back().tick == t.tick)
            compact.back() = t;
        else
            compact.push_back(t);
    }

    if (compact.front().tick != 0)
        compact.insert(compact.begin(), {0, 500000, -2});

    std::vector<std::pair<quint64, double>> timeAtTempo;

    double seconds = 0.0;

    for (size_t i = 0; i < compact.size(); ++i) {
        if (i > 0) {
            quint64 delta = compact[i].tick - compact[i - 1].tick;

            seconds +=
                (double(delta) * compact[i - 1].microsPerQuarter) /
                (double(division) * 1000000.0);
        }

        timeAtTempo.push_back({compact[i].tick, seconds});
    }

    auto tickToSeconds = [&](quint64 target) {
        auto it = std::upper_bound(
            timeAtTempo.begin(),
            timeAtTempo.end(),
            target,
            [](quint64 value, const auto &entry) {
                return value < entry.first;
            }
        );

        if (it == timeAtTempo.begin())
            return 0.0;

        --it;

        size_t idx = size_t(it - timeAtTempo.begin());

        return it->second +
            (double(target - it->first) * compact[idx].microsPerQuarter) /
            (double(division) * 1000000.0);
    };

    std::vector<MidiEvent> events;
    events.reserve(rawNotes.size());

    for (const auto &n : rawNotes) {
        events.push_back({
            tickToSeconds(n.tick),
            n.instrumentIndex,
            n.pitch
        });
    }

    std::stable_sort(
        events.begin(),
        events.end(),
        [](const MidiEvent &a, const MidiEvent &b) {
            if (a.timeSeconds != b.timeSeconds)
                return a.timeSeconds < b.timeSeconds;

            return a.instrumentIndex < b.instrumentIndex;
        }
    );

    return events;
}

} // namespace MidiReader


class PositionManager {
public:
    PositionManager(int width, int height)
        : sw(width), sh(height) {
        resetInitial();
    }

    void resetInitial() {
        m24x = 100;
        m24y = 100;

        m40x = 100;
        m40y = 100;

        m29x = 100;
        m29y = 100;

        m30x = 1820;
        m30y = 100;

        m35y = 200;
        m33x = sw - 40;

        m27x = 1620;
        m27y = 540;

        m26x = 960;
        m26y = 340;

        m34x = 175;
    }

    void resetAfterClear() {
        m24x = 100;
        m24y = 100;

        m26x = sw / 2;
        m26y = sh / 2 - 200;

        m40x = sw / 2;
        m40y = sh / 2;

        m27x = sw - 300;
        m27y = sh / 2;

        m29x = 100;
        m29y = 100;

        m30x = sw - 100;
        m30y = 100;

        m35y = 200;
    }

    QPoint getPosition(int pitch) {
        int cx = sw / 2;
        int cy = sh / 2;

        if (pitch == 24) {
            m24x += 16;
            m24y += 16;
            return {m24x, m24y};
        }

        if (pitch == 31)
            return {cx, cy};

        if (pitch == 40) {
            QPoint p(m40x, m40y);
            m40x += 16;
            m40y += 16;
            return p;
        }

        if (pitch == 26) {
            QPoint p(m26x, m26y);
            m26x += 16;
            m26y += 16;
            return p;
        }

        if (pitch == 25)
            return {cx, cy + 200};

        if (pitch == 35) {
            QPoint p(cx, m35y);
            m35y += 16;
            return p;
        }

        if (pitch == 29) {
            m29x += 16;
            m29y += 16;
            return {m29x, m29y};
        }

        if (pitch == 30) {
            m30x -= 16;
            m30y += 16;
            return {m30x, m30y};
        }

        if (pitch == 32) {
            QPoint p(cx, m35y);
            m35y += 16;
            return p;
        }

        if (pitch == 38 || pitch == 37)
            return {cx, cy};

        if (pitch == 27 || pitch == 39) {
            QPoint p(m27x, m27y);
            m27x -= 16;
            m27y += 16;
            return p;
        }

        if (pitch == 33) {
            QPoint p(m33x, sh - 80);
            m33x -= 10;
            return p;
        }

        if (pitch == 34) {
            QPoint p(m34x, cy - 36);
            m34x += 10;
            return p;
        }

        return {cx, cy};
    }

private:
    int sw, sh;

    int m24x, m24y;
    int m40x, m40y;
    int m29x, m29y;
    int m30x, m30y;
    int m35y, m33x;
    int m27x, m27y;
    int m26x, m26y;
    int m34x;
};


class ErrorDialog final : public QDialog {
public:
    explicit ErrorDialog(int pitch, QWidget *parent = nullptr)
        : QDialog(parent) {

        const auto it = DIALOGS.find(pitch);

        if (it == DIALOGS.end())
            return;

        const DialogData &data = it->second;

        setWindowTitle(data.title);
        setWindowFlag(Qt::Window, true);
        setModal(false);
        setAttribute(Qt::WA_DeleteOnClose);
        setFixedSize(data.size);

        auto *layout = new QVBoxLayout(this);

        layout->setContentsMargins(18, 16, 18, 12);
        layout->setSpacing(12);

        auto *content = new QHBoxLayout;
        content->setSpacing(14);

        auto *iconLabel = new QLabel(data.icon);

        iconLabel->setFixedWidth(42);
        iconLabel->setAlignment(Qt::AlignTop);
        iconLabel->setStyleSheet("font-size: 28px;");

        auto *messageLabel = new QLabel(data.message);

        messageLabel->setWordWrap(true);
        messageLabel->setAlignment(
            Qt::AlignLeft | Qt::AlignVCenter
        );

        content->addWidget(iconLabel);
        content->addWidget(messageLabel, 1);

        layout->addLayout(content);
        layout->addStretch();

        auto *separator = new QFrame;

        separator->setFrameShape(QFrame::HLine);
        separator->setFrameShadow(QFrame::Sunken);

        layout->addWidget(separator);

        auto *buttonLayout = new QHBoxLayout;

        buttonLayout->addStretch();

        for (const QString &text : data.buttons) {
            auto *button = new QPushButton(text);

            button->setMinimumWidth(76);

            connect(
                button,
                &QPushButton::clicked,
                this,
                &QDialog::close
            );

            buttonLayout->addWidget(button);
        }

        layout->addLayout(buttonLayout);
    }
};


class CrazyErrorWindow final : public QMainWindow {
public:
    CrazyErrorWindow() {
        setWindowTitle(QStringLiteral("Crazy Error V2 - Linux"));
        resize(420, 180);

        QScreen *screen = QApplication::primaryScreen();

        QRect geometry = screen
            ? screen->geometry()
            : QRect(0, 0, 1920, 1080);

        screenWidth = geometry.width();
        screenHeight = geometry.height();

        positions = std::make_unique<PositionManager>(
            screenWidth,
            screenHeight
        );

        player = new QMediaPlayer(this);
        audioOutput = new QAudioOutput(this);

        player->setAudioOutput(audioOutput);
        audioOutput->setVolume(1.0);

        connect(
            player,
            &QMediaPlayer::errorOccurred,
            this,
            [this](QMediaPlayer::Error, const QString &error) {
                qWarning() << "Audio error:" << error;
            }
        );

        timer = new QTimer(this);
        timer->setInterval(4);

        connect(
            timer,
            &QTimer::timeout,
            this,
            [this] {
                processEvents();
            }
        );

        QTimer::singleShot(3000, this, [this] {
            start();
        });
    }

private:
    int screenWidth = 1920;
    int screenHeight = 1080;

    std::unique_ptr<PositionManager> positions;

    std::vector<MidiEvent> events;
    size_t eventIndex = 0;

    std::vector<QPointer<ErrorDialog>> activeDialogs;

    QElapsedTimer clock;

    QTimer *timer = nullptr;
    QMediaPlayer *player = nullptr;
    QAudioOutput *audioOutput = nullptr;


    QString midiPath() const {
        return QDir(
            QCoreApplication::applicationDirPath()
        ).filePath("CRAZY-ERROR-170BPM.mid");
    }

    QString musicPath() const {
        return QDir(
            QCoreApplication::applicationDirPath()
        ).filePath("CRAZY-ERROR-170BPM.wav");
    }


    void startAudio() {
        const QString path = musicPath();

        if (!QFileInfo::exists(path)) {
            qWarning() << "Music file not found. Continuing without music.";
            return;
        }

        player->stop();
        player->setSource(QUrl::fromLocalFile(path));
        player->play();
    }

    void stopAudio() {
        if (player)
            player->stop();
    }


    void start() {
        if (timer->isActive())
            return;

        try {
            events = MidiReader::load(midiPath());
        } catch (const std::exception &e) {
            qCritical() << "MIDI error:" << e.what();
            QCoreApplication::quit();
            return;
        }

        eventIndex = 0;

        positions->resetInitial();

        startAudio();

        clock.start();
        timer->start();
    }


    void showDialog(int pitch) {
        auto it = DIALOGS.find(pitch);

        if (it == DIALOGS.end())
            return;

        QPoint p = positions->getPosition(pitch);

        auto *dialog = new ErrorDialog(pitch, this);

        if (pitch == 24) {
            dialog->move(p);
        } else if (pitch == 27) {
            dialog->move(
                p.x() - dialog->width(),
                p.y() - dialog->height()
            );
        } else {
            dialog->move(
                p.x() - dialog->width() / 2,
                p.y() - dialog->height() / 2
            );
        }

        activeDialogs.push_back(dialog);

        connect(
            dialog,
            &QDialog::finished,
            this,
            [this, dialog](int) {
                removeDialog(dialog);
            }
        );

        connect(
            dialog,
            &QObject::destroyed,
            this,
            [this, dialog] {
                removeDialog(dialog);
            }
        );

        dialog->show();
    }


    void removeDialog(ErrorDialog *dialog) {
        activeDialogs.erase(
            std::remove_if(
                activeDialogs.begin(),
                activeDialogs.end(),
                [dialog](const QPointer<ErrorDialog> &p) {
                    return p.isNull() || p.data() == dialog;
                }
            ),
            activeDialogs.end()
        );
    }


    void closeAllDialogs() {
        auto copy = activeDialogs;

        activeDialogs.clear();

        for (const auto &ptr : copy) {
            if (ptr)
                ptr->close();
        }
    }


    void handlePitch(int pitch) {
        if (pitch == 0) {
            closeAllDialogs();
            positions->resetAfterClear();
            return;
        }

        if (pitch == 41) {
            QApplication::beep();
            return;
        }

        showDialog(pitch);
    }


    void processEvents() {
        const double elapsed =
            double(clock.nsecsElapsed()) / 1000000000.0;

        const double lookAhead = elapsed + 0.08;

        while (eventIndex < events.size()) {
            const MidiEvent &event = events[eventIndex];

            if (event.timeSeconds > lookAhead)
                break;

            ++eventIndex;

            handlePitch(event.pitch);
        }

        if (eventIndex >= events.size()) {
    timer->stop();
    stopAudio();
    closeAllDialogs();

    QCoreApplication::quit();
}
    }
};


int main(int argc, char *argv[]) {
    qputenv("QT_QPA_PLATFORM", "xcb");

    QApplication app(argc, argv);

    QCoreApplication::setApplicationName("CrazyError");

    CrazyErrorWindow window;

    return app.exec();
}