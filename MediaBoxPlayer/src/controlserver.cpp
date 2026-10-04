#include "controlserver.h"
#include "playerengine.h"
#include "schedulecore/schedulev1.h"

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

#ifdef Q_OS_WIN
#include <QScopeGuard>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <Aclapi.h>
#include <Sddl.h>
#include <io.h>
#endif

namespace MediaBox {
namespace {
constexpr qint64 MaxRequestBytes = 1024 * 1024;
constexpr qint64 MaxPendingReplyBytes = 8 * 1024 * 1024;
constexpr int MaxConnections = 16;

#ifdef Q_OS_WIN
bool protectControlToken(QSaveFile &file, QString *error)
{
    const auto fail = [error](DWORD code, const char *operation) {
        *error = QStringLiteral("Cannot restrict control.token permissions: %1 (Windows error %2).")
                     .arg(QString::fromLatin1(operation)).arg(code);
        return false;
    };
    HANDLE processToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &processToken))
        return fail(GetLastError(), "OpenProcessToken");
    const auto closeToken = qScopeGuard([&] { CloseHandle(processToken); });
    DWORD tokenSize = 0;
    GetTokenInformation(processToken, TokenUser, nullptr, 0, &tokenSize);
    if (!tokenSize)
        return fail(GetLastError(), "GetTokenInformation size");
    QByteArray tokenInformation(tokenSize, Qt::Uninitialized);
    if (!GetTokenInformation(processToken, TokenUser, tokenInformation.data(), tokenSize, &tokenSize))
        return fail(GetLastError(), "GetTokenInformation");
    const auto user = reinterpret_cast<TOKEN_USER *>(tokenInformation.data());
    LPWSTR userSid = nullptr;
    if (!ConvertSidToStringSidW(user->User.Sid, &userSid))
        return fail(GetLastError(), "ConvertSidToStringSidW");
    const auto freeSid = qScopeGuard([&] { LocalFree(userSid); });
    const QString sddl = QStringLiteral("D:P(A;;FA;;;SY)(A;;FA;;;BA)(A;;FA;;;%1)")
                             .arg(QString::fromWCharArray(userSid));
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            reinterpret_cast<LPCWSTR>(sddl.utf16()), SDDL_REVISION_1, &descriptor, nullptr))
        return fail(GetLastError(), "ConvertStringSecurityDescriptorToSecurityDescriptorW");
    const auto freeDescriptor = qScopeGuard([&] { LocalFree(descriptor); });
    PACL acl = nullptr;
    BOOL present = FALSE;
    BOOL defaulted = FALSE;
    if (!GetSecurityDescriptorDacl(descriptor, &present, &acl, &defaulted) || !present || !acl)
        return fail(ERROR_INVALID_SECURITY_DESCR, "GetSecurityDescriptorDacl");

    const int descriptorNumber = file.handle();
    if (descriptorNumber < 0)
        return fail(ERROR_INVALID_HANDLE, "QSaveFile::handle");
    const auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(descriptorNumber));
    // QSaveFile keeps its temporary file non-shared on Windows. Reopen only
    // security access to that same file and protect it before writing secrets;
    // no other process can read the temporary file before this DACL is set.
    // SetSecurityInfo also needs to read the existing descriptor when protecting
    // its DACL; WRITE_DAC alone makes that call fail with ERROR_ACCESS_DENIED.
    const HANDLE securityHandle = ReOpenFile(handle, READ_CONTROL | WRITE_DAC,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0);
    if (securityHandle == INVALID_HANDLE_VALUE)
        return fail(GetLastError(), "ReOpenFile");
    const auto closeSecurityHandle = qScopeGuard([&] { CloseHandle(securityHandle); });
    const DWORD result = SetSecurityInfo(securityHandle, SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, nullptr, nullptr, acl, nullptr);
    return result == ERROR_SUCCESS || fail(result, "SetSecurityInfo");
}
#endif

bool readControlTokenFile(const QString &path, QByteArray *token, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("Cannot read control.token: %1").arg(file.errorString());
        return false;
    }
    *token = file.read(66).trimmed();
    if (!file.atEnd() || token->size() != 64) {
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

bool writeControlTokenFile(const QString &path, const QByteArray &token, QString *error)
{
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)
        || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        *error = QStringLiteral("Cannot create control.token: %1").arg(file.errorString());
        return false;
    }
#ifdef Q_OS_WIN
    if (!protectControlToken(file, error))
        return false;
#endif
    if (file.write(token + '\n') != token.size() + 1 || !file.commit()) {
        *error = QStringLiteral("Cannot create control.token: %1").arg(file.errorString());
        return false;
    }
    return true;
}

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
    : ControlServer([engine](const QJsonObject &request) { return engine->execute(request); },
                    std::move(token), parent)
{
}

ControlServer::ControlServer(std::function<QJsonObject(const QJsonObject &)> handler,
                             QByteArray token, QObject *parent)
    : QObject(parent), m_handler(std::move(handler)), m_token(std::move(token))
{
    Q_ASSERT(m_handler);
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
    QJsonObject request;
    if (!ScheduleV1::strictJsonObject(line, &request).isEmpty())
        return serialize(failure(QStringLiteral("invalid_json"),
                                 QStringLiteral("Expected a JSON object.")));
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
        response = m_handler(request);
    }
    if (!id.isUndefined())
        response.insert(QStringLiteral("id"), id);
    return serialize(response);
}

bool loadControlToken(const QString &dataDirectory, QByteArray *token, QString *error)
{
    const QString path = QDir(dataDirectory).filePath(QStringLiteral("control.token"));
    if (QFileInfo::exists(path))
        return readControlTokenFile(path, token, error);
    QByteArray random;
    random.reserve(32);
    for (int i = 0; i < 8; ++i) {
        const quint32 word = QRandomGenerator::system()->generate();
        random.append(reinterpret_cast<const char *>(&word), sizeof(word));
    }
    *token = random.toHex();
    return writeControlTokenFile(path, *token, error);
}

} // namespace MediaBox
