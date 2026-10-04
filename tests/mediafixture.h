#pragma once

#include <QByteArray>
#include <QString>

// Small in-file metadata fixtures for TagLib. Playback tests use fake backends.
namespace MediaFixture {
inline QByteArray uint32(quint32 value, bool synchsafe = false)
{
    QByteArray result(4, '\0');
    const int width = synchsafe ? 7 : 8;
    for (int i = 3; i >= 0; --i) {
        result[i] = char(value & (synchsafe ? 127 : 255));
        value >>= width;
    }
    return result;
}

inline QByteArray mp3(const QString &title, const QString &artist, const QString &album,
                      const QString &genre, int year, int seconds)
{
    QByteArray tags;
    const auto text = [&tags](const char *id, const QString &value) {
        const QByteArray data = QByteArray(1, char(3)) + value.toUtf8();
        tags += QByteArray(id, 4) + uint32(quint32(data.size()), true) + QByteArray(2, '\0') + data;
    };
    text("TIT2", title); text("TPE1", artist); text("TALB", album);
    text("TCON", genre); text("TDRC", QString::number(year));
    QByteArray result = QByteArray("ID3\x04\0\0", 6) + uint32(quint32(tags.size()), true) + tags;
    // MPEG-1 Layer III, 128 kb/s. Round up so whole-second duration is stable.
    const int frames = qMax(2, (seconds * 16000 + 416) / 417);
    const QByteArray frame = QByteArray::fromHex("fffb9064") + QByteArray(413, '\0');
    result.reserve(result.size() + frames * frame.size());
    for (int i = 0; i < frames; ++i) result += frame;
    return result;
}

inline QByteArray atom(const QByteArray &name, const QByteArray &contents)
{
    return uint32(quint32(contents.size() + 8)) + name + contents;
}

inline QByteArray mp4(const QString &title, const QString &artist, const QString &album,
                      const QString &genre, int year, int seconds)
{
    QByteArray tags;
    const auto text = [&tags](const char *id, const QString &value) {
        tags += atom(QByteArray::fromHex(id), atom("data", uint32(1) + uint32(0) + value.toUtf8()));
    };
    text("a96e616d", title); text("a9415254", artist); text("a9616c62", album);
    text("a967656e", genre); text("a9646179", QString::number(year));
    const auto metadata = atom("udta", atom("meta", uint32(0) + atom("ilst", tags)));
    const auto handler = atom("hdlr", QByteArray(8, '\0') + "soun" + QByteArray(12, '\0'));
    const auto duration = atom("mdhd", QByteArray(12, '\0') + uint32(1000)
        + uint32(quint32(seconds * 1000)) + QByteArray(4, '\0'));
    const auto track = atom("trak", atom("mdia", handler + duration));
    return atom("ftyp", QByteArray("isom") + uint32(0) + "isommp42")
        + atom("moov", metadata + track) + atom("mdat", {});
}
}
