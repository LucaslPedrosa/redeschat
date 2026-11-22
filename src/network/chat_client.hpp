#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QStringList>

#include <functional>

class QTcpSocket;
class QTimer;

struct OutgoingFile {
    QString name;
    QByteArray data;
    QByteArray sha256;
};

struct IncomingFile {
    QString sender;
    QString name;
    QString savePath;
    qint64 size = 0;
    QByteArray sha256;
    QByteArray data;
};

class ChatClient final : public QObject
{
public:
    using StatusHandler = std::function<void(const QString &)>;
    using ConnectionHandler = std::function<void(bool)>;
    using ProtocolHandler = std::function<void()>;
    using AuthenticationHandler = std::function<void(bool, const QString &, const QString &)>;
    using ChatMessageHandler = std::function<void(const QString &, const QString &, const QString &)>;
    using PrivateMessageHandler =
        std::function<void(const QString &, const QString &, const QString &, const QString &)>;
    using PrivateMessageResultHandler = std::function<void(bool, const QString &)>;
    using PresenceHandler = std::function<void(const QString &, bool)>;
    using UserListHandler = std::function<void(const QStringList &)>;
    using RoomListHandler = std::function<void(const QStringList &)>;
    using RoomMembershipHandler = std::function<void(const QString &, bool)>;
    using RoomMessageHandler =
        std::function<void(const QString &, const QString &, const QString &, const QString &)>;
    using RoomPresenceHandler = std::function<void(const QString &, const QString &, bool)>;
    using FileOfferHandler =
        std::function<void(const QString &, const QString &, const QString &, qint64)>;
    using FileProgressHandler = std::function<void(const QString &, qint64, qint64)>;
    using FileResultHandler = std::function<void(bool, const QString &)>;

    explicit ChatClient(QObject *parent = nullptr);

    void connectToServer(const QString &host, quint16 port);
    void disconnectFromServer();
    void registerAccount(const QString &username, const QString &password);
    void login(const QString &username, const QString &password);
    void sendGeneralMessage(const QString &text);
    void sendPrivateMessage(const QString &recipient, const QString &text);
    void requestUserList();
    void createRoom(const QString &room);
    void joinRoom(const QString &room);
    void leaveRoom(const QString &room);
    void sendRoomMessage(const QString &room, const QString &text);
    void requestRoomList();
    bool offerFile(const QString &recipient, const QString &path, QString *errorMessage = nullptr);
    void respondToFile(const QString &transferId, bool accepted, const QString &savePath = {});
    bool isConnected() const;

    void setStatusHandler(StatusHandler handler);
    void setConnectionHandler(ConnectionHandler handler);
    void setProtocolHandler(ProtocolHandler handler);
    void setAuthenticationHandler(AuthenticationHandler handler);
    void setChatMessageHandler(ChatMessageHandler handler);
    void setPrivateMessageHandler(PrivateMessageHandler handler);
    void setPrivateMessageResultHandler(PrivateMessageResultHandler handler);
    void setPresenceHandler(PresenceHandler handler);
    void setUserListHandler(UserListHandler handler);
    void setRoomListHandler(RoomListHandler handler);
    void setRoomMembershipHandler(RoomMembershipHandler handler);
    void setRoomMessageHandler(RoomMessageHandler handler);
    void setRoomPresenceHandler(RoomPresenceHandler handler);
    void setFileOfferHandler(FileOfferHandler handler);
    void setFileProgressHandler(FileProgressHandler handler);
    void setFileResultHandler(FileResultHandler handler);

private:
    void processIncomingMessages();

    QTcpSocket *socket_;
    QTimer *connectionTimer_;
    QByteArray receiveBuffer_;
    StatusHandler statusHandler_;
    ConnectionHandler connectionHandler_;
    ProtocolHandler protocolHandler_;
    AuthenticationHandler authenticationHandler_;
    ChatMessageHandler chatMessageHandler_;
    PrivateMessageHandler privateMessageHandler_;
    PrivateMessageResultHandler privateMessageResultHandler_;
    PresenceHandler presenceHandler_;
    UserListHandler userListHandler_;
    RoomListHandler roomListHandler_;
    RoomMembershipHandler roomMembershipHandler_;
    RoomMessageHandler roomMessageHandler_;
    RoomPresenceHandler roomPresenceHandler_;
    FileOfferHandler fileOfferHandler_;
    FileProgressHandler fileProgressHandler_;
    FileResultHandler fileResultHandler_;
    QHash<QString, OutgoingFile> outgoingFiles_;
    QHash<QString, IncomingFile> incomingFiles_;
};
