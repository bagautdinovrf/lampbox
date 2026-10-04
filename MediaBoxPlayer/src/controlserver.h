#pragma once

#include <QByteArray>
#include <QHostAddress>
#include <QObject>
#include <QJsonObject>
#include <QTcpServer>
#include <functional>

namespace MediaBox {

class PlayerEngine;

// Newline-delimited UTF-8 JSON; every request authenticates independently.
class ControlServer final : public QObject
{
public:
    ControlServer(PlayerEngine *engine, QByteArray token, QObject *parent = nullptr);
    ControlServer(std::function<QJsonObject(const QJsonObject &)> handler,
                  QByteArray token, QObject *parent = nullptr);
    bool listen(const QHostAddress &address, quint16 port, QString *error);
    quint16 port() const { return m_server.serverPort(); }

private:
    void acceptConnections();
    QByteArray dispatch(const QByteArray &line);

    std::function<QJsonObject(const QJsonObject &)> m_handler;
    QByteArray m_token;
    QTcpServer m_server;
    int m_connections = 0;
};

// Loads the token from the configured directory or creates a private random token.
bool loadControlToken(const QString &dataDirectory, QByteArray *token, QString *error);

} // namespace MediaBox
