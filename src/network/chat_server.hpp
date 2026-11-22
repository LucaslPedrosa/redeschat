#pragma once

#include "database/database.hpp"

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QSet>

#include <functional>

class QTcpServer;
class QTcpSocket;
class QJsonObject;

struct ClientSession {
    QByteArray receiveBuffer;
    bool protocolReady = false;
    QString username;
    QSet<QString> joinedRooms;
};

struct FileTransfer {
    QTcpSocket *sender = nullptr;
    QTcpSocket *recipient = nullptr;
    QString name;
    qint64 size = 0;
    qint64 received = 0;
};

class ChatServer final : public QObject
{
public:
    using EventHandler = std::function<void(const QString &)>;
    using ClientCountHandler = std::function<void(int)>;

    explicit ChatServer(QObject *parent = nullptr, const QString &databasePath = {});

    bool start(quint16 port, QString *errorMessage = nullptr);
    void stop();

    bool isListening() const;
    quint16 port() const;
    int clientCount() const;

    void setEventHandler(EventHandler handler);
    void setClientCountHandler(ClientCountHandler handler);

private:
    void acceptPendingConnections();
    void processIncomingMessages(QTcpSocket *client, const QString &peer);
    void handleMessage(QTcpSocket *client, const QJsonObject &message, const QString &peer);
    void handleRegister(QTcpSocket *client, const QJsonObject &message, const QString &peer);
    void handleLogin(QTcpSocket *client, const QJsonObject &message, const QString &peer);
    void handleGeneralMessage(QTcpSocket *client, const QJsonObject &message);
    void handlePrivateMessage(QTcpSocket *client, const QJsonObject &message);
    void handleCreateRoom(QTcpSocket *client, const QJsonObject &message);
    void handleJoinRoom(QTcpSocket *client, const QJsonObject &message);
    void handleLeaveRoom(QTcpSocket *client, const QJsonObject &message);
    void handleRoomMessage(QTcpSocket *client, const QJsonObject &message);
    void handleFileOffer(QTcpSocket *client, const QJsonObject &message);
    void handleFileResponse(QTcpSocket *client, const QJsonObject &message);
    void handleFileChunk(QTcpSocket *client, const QJsonObject &message);
    void handleFileComplete(QTcpSocket *client, const QJsonObject &message);
    void cancelTransfersFor(QTcpSocket *client);
    bool usernameIsOnline(const QString &username) const;
    QTcpSocket *clientForUsername(const QString &username) const;
    void completeAuthentication(QTcpSocket *client, const QString &username);
    void broadcast(const QJsonObject &message, QTcpSocket *excludedClient = nullptr);
    void broadcastToRoom(const QString &room, const QJsonObject &message,
                         QTcpSocket *excludedClient = nullptr);
    void broadcastUserList();
    void sendRoomList(QTcpSocket *client = nullptr);
    void sendRoomHistory(QTcpSocket *client, const QString &room);
    void sendMessage(QTcpSocket *client, const QJsonObject &message);
    void reportEvent(const QString &message) const;
    void reportClientCount() const;

    QTcpServer *server_;
    Database database_;
    QHash<QTcpSocket *, ClientSession> clients_;
    QHash<QString, QSet<QTcpSocket *>> rooms_;
    QHash<QString, FileTransfer> transfers_;
    EventHandler eventHandler_;
    ClientCountHandler clientCountHandler_;
};
