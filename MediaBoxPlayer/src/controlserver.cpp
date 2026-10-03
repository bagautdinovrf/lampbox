#include "controlserver.h"
#include "playerengine.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QTcpSocket>
#include <QTimer>

#include <utility>

namespace MediaBox {
namespace {
constexpr qint64 MaxRequestBytes = 1024 * 1024;
constexpr qint64 MaxPendingReplyBytes = 8 * 1024 * 1024;
constexpr int MaxConnections = 16;

QJsonObject failure(const QString &code, const QString &message)
{
    return {{QStringLiteral("ok"), false},
            {QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), code},
                                                {QStringLiteral("message"), message}}}};
}

QByteArray serialize(QJsonObject response)
{
    response.insert(QStringLiteral("protocolVersion"), 1);
    return QJsonDocument(response).toJson(QJsonDocument::Compact) + '\n';
}

bool matchesToken(const QByteArray &expected, const QByteArray &actual)
{
    if (expected.size() != actual.size())
        return false;
    unsigned char difference = 0;
    for (qsizetype i = 0; i < expected.size(); ++i)
        difference |= static_cast<unsigned char>(expected.at(i) ^ actual.at(i));
    return difference == 0;
}
} // namespace

ControlServer::ControlServer(PlayerEngine *engine, QByteArray token, QObject *parent)
    : QObject(parent), m_engine(engine), m_token(std::move(token))
{
    m_server.setMaxPendingConnections(MaxConnections);
    connect(&m_server, &QTcpServer::newConnection, this, &ControlServer::acceptConnections);
}

bool ControlServer::listen(const QHostAddress &address, quint16 port, QString *error)
{
    if (m_server.listen(address, port))
        return true;
    *error = m_server.errorString();
    return false;
}

void ControlServer::acceptConnections()
{
    while (m_server.hasPendingConnections()) {
        QTcpSocket *socket = m_server.nextPendingConnection();
        if (m_connections >= MaxConnections) {
            socket->abort();
            socket->deleteLater();
            continue;
        }
        ++m_connections;
        socket->setReadBufferSize(MaxRequestBytes + 1);
        auto *idle = new QTimer(socket);
        idle->setSingleShot(true);
        idle->setInterval(30000);
        connect(idle, &QTimer::timeout, socket, &QTcpSocket::abort);
        connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
            --m_connections;
            socket->deleteLater();
        });
        connect(socket, &QTcpSocket::readyRead, this, [this, socket, idle] {
            idle->start();
            while (socket->canReadLine()) {
                const QByteArray line = socket->readLine(MaxRequestBytes + 1);
                if (!line.endsWith('\n') || line.size() > MaxRequestBytes) {
                    socket->write(serialize(failure(QStringLiteral("request_too_large"),
                                                   QStringLiteral("Request exceeds 1 MiB."))));
                    socket->disconnectFromHost();
                    return;
                }
                // Bound queued output before executing another mutating command.
                if (socket->bytesToWrite() > MaxPendingReplyBytes) {
                    socket->abort();
                    return;
                }
                socket->write(dispatch(line));
            }
            if (socket->bytesAvailable() >= MaxRequestBytes) {
                socket->write(serialize(failure(QStringLiteral("request_too_large"),
                                               QStringLiteral("Request exceeds 1 MiB."))));
                socket->disconnectFromHost();
            }
        });
        idle->start();
    }
}

QByteArray ControlServer::dispatch(const QByteArray &line)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return serialize(failure(QStringLiteral("invalid_json"),
                                 QStringLiteral("Expected a JSON object.")));

    QJsonObject request = document.object();
    const QJsonValue id = request.value(QStringLiteral("id"));
    if (!id.isUndefined() && !id.isString() && !id.isDouble() && !id.isNull())
        return serialize(failure(QStringLiteral("invalid_request"),
                                 QStringLiteral("id must be a string, number or null.")));

    QJsonObject response;
    if (!matchesToken(m_token, request.value(QStringLiteral("token")).toString().toUtf8())) {
        response = failure(QStringLiteral("unauthorized"), QStringLiteral("Invalid control token."));
    } else if (request.value(QStringLiteral("protocolVersion")) != QJsonValue(1)) {
        response = failure(QStringLiteral("unsupported_protocol"),
                           QStringLiteral("protocolVersion must be 1."));
    } else {
        request.remove(QStringLiteral("token"));
        request.remove(QStringLiteral("protocolVersion"));
        response = m_engine->execute(request);
    }
    if (!id.isUndefined())
        response.insert(QStringLiteral("id"), id);
    return serialize(response);
}

bool loadControlToken(const QString &dataDirectory, QByteArray *token, QString *error)
{
    const QString path = QDir(dataDirectory).filePath(QStringLiteral("control.token"));
    QFile existing(path);
    if (QFileInfo::exists(path)) {
        if (!existing.open(QIODevice::ReadOnly)) {
            *error = QStringLiteral("Cannot read control.token: %1").arg(existing.errorString());
            return false;
        }
        *token = existing.read(66).trimmed();
        if (!existing.atEnd() || token->size() != 64) {
            *error = QStringLiteral("control.token must contain 64 hexadecimal characters.");
            return false;
        }
        for (const char byte : *token) {
            if (!((byte >= '0' && byte <= '9') || (byte >= 'a' && byte <= 'f'))) {
                *error = QStringLiteral("control.token must contain lowercase hexadecimal characters.");
                return false;
            }
        }
        return true;
    }

    QByteArray random;
    random.reserve(32);
    for (int i = 0; i < 8; ++i) {
        const quint32 word = QRandomGenerator::system()->generate();
        random.append(reinterpret_cast<const char *>(&word), sizeof(word));
    }
    *token = random.toHex();
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)
        || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)
        || file.write(*token + '\n') != token->size() + 1 || !file.commit()) {
        *error = QStringLiteral("Cannot create control.token: %1").arg(file.errorString());
        return false;
    }
    return true;
}

} // namespace MediaBox
