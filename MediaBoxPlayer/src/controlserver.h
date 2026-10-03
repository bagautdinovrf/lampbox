#pragma once

#include <QByteArray>
#include <QHostAddress>
#include <QObject>
#include <QTcpServer>

namespace MediaBox {

class PlayerEngine;

// Newline-delimited UTF-8 JSON; every request authenticates independently.
class ControlServer final : public QObject
{
public:
    ControlServer(PlayerEngine *engine, QByteArray token, QObject *parent = nullptr);
    bool listen(const QHostAddress &address, quint16 port, QString *error);
    quint16 port() const { return m_server.serverPort(); }

private:
    void acceptConnections();
    QByteArray dispatch(const QByteArray &line);

    PlayerEngine *m_engine;
    QByteArray m_token;
    QTcpServer m_server;
    int m_connections = 0;
};

// Creates a random token atomically on first start; never logs its contents.
bool loadControlToken(const QString &dataDirectory, QByteArray *token, QString *error);

} // namespace MediaBox
