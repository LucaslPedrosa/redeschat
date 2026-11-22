#include "network/chat_client.hpp"
#include "protocol/message_protocol.hpp"

#include <QAbstractSocket>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonObject>
#include <QJsonArray>
#include <QTcpSocket>
#include <QTimer>
#include <QUuid>

namespace {
constexpr int connectionTimeoutMilliseconds = 5000;
constexpr qint64 maximumFileSize = 10 * 1024 * 1024;
constexpr qsizetype fileChunkSize = 32 * 1024;
}

ChatClient::ChatClient(QObject *parent)
    : QObject(parent),
      socket_(new QTcpSocket(this)),
      connectionTimer_(new QTimer(this))
{
    connectionTimer_->setSingleShot(true);

    connect(socket_, &QTcpSocket::connected, this, [this] {
        connectionTimer_->stop();
        if (statusHandler_) {
            statusHandler_(QString("TCP conectado a %1:%2; negociando protocolo...")
                               .arg(socket_->peerAddress().toString())
                               .arg(socket_->peerPort()));
        }
        if (connectionHandler_) {
            connectionHandler_(true);
        }

        const QByteArray hello = MessageProtocol::encode({
            {"type", "hello"},
            {"protocolVersion", MessageProtocol::currentVersion},
        });
        socket_->write(hello);
    });

    connect(socket_, &QTcpSocket::readyRead, this, [this] { processIncomingMessages(); });

    connect(socket_, &QTcpSocket::disconnected, this, [this] {
        connectionTimer_->stop();
        receiveBuffer_.clear();
        outgoingFiles_.clear();
        incomingFiles_.clear();
        if (statusHandler_) {
            statusHandler_("Desconectado do servidor");
        }
        if (connectionHandler_) {
            connectionHandler_(false);
        }
    });

    connect(socket_, &QTcpSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) {
                connectionTimer_->stop();
                if (statusHandler_) {
                    statusHandler_(QString("Erro de conexão: %1").arg(socket_->errorString()));
                }
                if (connectionHandler_) {
                    connectionHandler_(false);
                }
            });

    connect(connectionTimer_, &QTimer::timeout, this, [this] {
        if (socket_->state() == QAbstractSocket::ConnectingState) {
            socket_->abort();
            if (statusHandler_) {
                statusHandler_("Tempo limite de conexão excedido");
            }
            if (connectionHandler_) {
                connectionHandler_(false);
            }
        }
    });
}

void ChatClient::connectToServer(const QString &host, quint16 port)
{
    socket_->abort();
    if (statusHandler_) {
        statusHandler_(QString("Conectando a %1:%2...").arg(host).arg(port));
    }
    socket_->connectToHost(host, port);
    connectionTimer_->start(connectionTimeoutMilliseconds);
}

void ChatClient::disconnectFromServer()
{
    connectionTimer_->stop();
    if (socket_->state() == QAbstractSocket::ConnectingState) {
        socket_->abort();
        if (statusHandler_) {
            statusHandler_("Conexão cancelada");
        }
        if (connectionHandler_) {
            connectionHandler_(false);
        }
        return;
    }

    socket_->disconnectFromHost();
}

void ChatClient::registerAccount(const QString &username, const QString &password)
{
    socket_->write(MessageProtocol::encode({
        {"type", "register"},
        {"username", username},
        {"password", password},
    }));
}

void ChatClient::login(const QString &username, const QString &password)
{
    socket_->write(MessageProtocol::encode({
        {"type", "login"},
        {"username", username},
        {"password", password},
    }));
}

void ChatClient::sendGeneralMessage(const QString &text)
{
    socket_->write(MessageProtocol::encode({
        {"type", "general_message"},
        {"text", text},
    }));
}

void ChatClient::sendPrivateMessage(const QString &recipient, const QString &text)
{
    socket_->write(MessageProtocol::encode({
        {"type", "private_message"},
        {"recipient", recipient},
        {"text", text},
    }));
}

void ChatClient::requestUserList()
{
    socket_->write(MessageProtocol::encode({{{"type", "list_users"}}}));
}

void ChatClient::createRoom(const QString &room)
{
    socket_->write(MessageProtocol::encode({{"type", "create_room"}, {"room", room}}));
}

void ChatClient::joinRoom(const QString &room)
{
    socket_->write(MessageProtocol::encode({{"type", "join_room"}, {"room", room}}));
}

void ChatClient::leaveRoom(const QString &room)
{
    socket_->write(MessageProtocol::encode({{"type", "leave_room"}, {"room", room}}));
}

void ChatClient::sendRoomMessage(const QString &room, const QString &text)
{
    socket_->write(MessageProtocol::encode({
        {"type", "room_message"},
        {"room", room},
        {"text", text},
    }));
}

void ChatClient::requestRoomList()
{
    socket_->write(MessageProtocol::encode({{{"type", "list_rooms"}}}));
}

bool ChatClient::offerFile(
    const QString &recipient, const QString &path, QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage != nullptr) {
            *errorMessage = file.errorString();
        }
        return false;
    }
    if (file.size() > maximumFileSize) {
        if (errorMessage != nullptr) {
            *errorMessage = "O arquivo excede o limite de 10 MiB";
        }
        return false;
    }

    const QByteArray data = file.readAll();
    const QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    const QString transferId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString name = QFileInfo(path).fileName();
    outgoingFiles_.insert(transferId, {name, data, hash});
    socket_->write(MessageProtocol::encode({
        {"type", "file_offer"},
        {"transferId", transferId},
        {"recipient", recipient},
        {"name", name},
        {"size", data.size()},
        {"sha256", QString::fromLatin1(hash.toHex())},
    }));
    return true;
}

void ChatClient::respondToFile(
    const QString &transferId, bool accepted, const QString &savePath)
{
    auto iterator = incomingFiles_.find(transferId);
    if (iterator == incomingFiles_.end()) {
        return;
    }
    if (accepted) {
        iterator->savePath = savePath;
    } else {
        incomingFiles_.erase(iterator);
    }
    socket_->write(MessageProtocol::encode({
        {"type", "file_response"},
        {"transferId", transferId},
        {"accepted", accepted},
    }));
}

bool ChatClient::isConnected() const
{
    return socket_->state() == QAbstractSocket::ConnectedState;
}

void ChatClient::setStatusHandler(StatusHandler handler)
{
    statusHandler_ = std::move(handler);
}

void ChatClient::setConnectionHandler(ConnectionHandler handler)
{
    connectionHandler_ = std::move(handler);
}

void ChatClient::setProtocolHandler(ProtocolHandler handler)
{
    protocolHandler_ = std::move(handler);
}

void ChatClient::setAuthenticationHandler(AuthenticationHandler handler)
{
    authenticationHandler_ = std::move(handler);
}

void ChatClient::setChatMessageHandler(ChatMessageHandler handler)
{
    chatMessageHandler_ = std::move(handler);
}

void ChatClient::setPrivateMessageHandler(PrivateMessageHandler handler)
{
    privateMessageHandler_ = std::move(handler);
}

void ChatClient::setPrivateMessageResultHandler(PrivateMessageResultHandler handler)
{
    privateMessageResultHandler_ = std::move(handler);
}

void ChatClient::setPresenceHandler(PresenceHandler handler)
{
    presenceHandler_ = std::move(handler);
}

void ChatClient::setUserListHandler(UserListHandler handler)
{
    userListHandler_ = std::move(handler);
}

void ChatClient::setRoomListHandler(RoomListHandler handler)
{
    roomListHandler_ = std::move(handler);
}

void ChatClient::setRoomMembershipHandler(RoomMembershipHandler handler)
{
    roomMembershipHandler_ = std::move(handler);
}

void ChatClient::setRoomMessageHandler(RoomMessageHandler handler)
{
    roomMessageHandler_ = std::move(handler);
}

void ChatClient::setRoomPresenceHandler(RoomPresenceHandler handler)
{
    roomPresenceHandler_ = std::move(handler);
}

void ChatClient::setFileOfferHandler(FileOfferHandler handler)
{
    fileOfferHandler_ = std::move(handler);
}

void ChatClient::setFileProgressHandler(FileProgressHandler handler)
{
    fileProgressHandler_ = std::move(handler);
}

void ChatClient::setFileResultHandler(FileResultHandler handler)
{
    fileResultHandler_ = std::move(handler);
}

void ChatClient::processIncomingMessages()
{
    receiveBuffer_.append(socket_->readAll());
    while (true) {
        QJsonObject message;
        QString errorMessage;
        const auto result = MessageProtocol::tryDecode(receiveBuffer_, message, errorMessage);
        if (result == MessageProtocol::DecodeResult::NeedMoreData) {
            return;
        }
        if (result == MessageProtocol::DecodeResult::InvalidFrame) {
            if (statusHandler_) {
                statusHandler_(QString("Resposta inválida do servidor: %1").arg(errorMessage));
            }
            socket_->disconnectFromHost();
            return;
        }

        const QString type = message.value("type").toString();
        if (type == "hello_ack") {
            if (statusHandler_) {
                statusHandler_(QString("Protocolo v%1 confirmado; conexão pronta")
                                   .arg(message.value("protocolVersion").toInt()));
            }
            if (protocolHandler_) {
                protocolHandler_();
            }
        } else if (type == "register_result" || type == "login_result") {
            if (authenticationHandler_) {
                authenticationHandler_(
                    message.value("success").toBool(),
                    message.value("username").toString(),
                    message.value("message").toString());
            }
        } else if (type == "general_message" && chatMessageHandler_) {
            chatMessageHandler_(
                message.value("sender").toString(),
                message.value("text").toString(),
                message.value("timestamp").toString());
        } else if (type == "private_message" && privateMessageHandler_) {
            privateMessageHandler_(
                message.value("sender").toString(),
                message.value("recipient").toString(),
                message.value("text").toString(),
                message.value("timestamp").toString());
        } else if (type == "private_message_result" && !message.value("success").toBool()) {
            const QString resultMessage = message.value("message").toString();
            if (privateMessageResultHandler_) {
                privateMessageResultHandler_(false, resultMessage);
            }
            if (statusHandler_) {
                statusHandler_(QString("Mensagem privada: %1").arg(resultMessage));
            }
        } else if ((type == "user_joined" || type == "user_left") && presenceHandler_) {
            presenceHandler_(message.value("username").toString(), type == "user_joined");
        } else if (type == "user_list" && userListHandler_) {
            QStringList users;
            for (const QJsonValue &value : message.value("users").toArray()) {
                users.append(value.toString());
            }
            userListHandler_(users);
        } else if (type == "room_list" && roomListHandler_) {
            QStringList rooms;
            for (const QJsonValue &value : message.value("rooms").toArray()) {
                rooms.append(value.toString());
            }
            roomListHandler_(rooms);
        } else if ((type == "room_joined" || type == "room_left") && roomMembershipHandler_) {
            roomMembershipHandler_(message.value("room").toString(), type == "room_joined");
        } else if (type == "room_message" && roomMessageHandler_) {
            roomMessageHandler_(
                message.value("room").toString(),
                message.value("sender").toString(),
                message.value("text").toString(),
                message.value("timestamp").toString());
        } else if (type == "room_history" && roomMessageHandler_) {
            const QString room = message.value("room").toString();
            for (const QJsonValue &value : message.value("messages").toArray()) {
                const QJsonObject stored = value.toObject();
                roomMessageHandler_(
                    room,
                    stored.value("sender").toString(),
                    stored.value("text").toString(),
                    stored.value("timestamp").toString());
            }
        } else if ((type == "room_user_joined" || type == "room_user_left")
                   && roomPresenceHandler_) {
            roomPresenceHandler_(
                message.value("room").toString(),
                message.value("username").toString(),
                type == "room_user_joined");
        } else if (type == "room_result" && !message.value("success").toBool()) {
            if (statusHandler_) {
                statusHandler_(QString("Sala: %1").arg(message.value("message").toString()));
            }
        } else if (type == "file_offer") {
            const QString transferId = message.value("transferId").toString();
            IncomingFile incoming{
                message.value("sender").toString(),
                message.value("name").toString(),
                {},
                message.value("size").toInteger(),
                QByteArray::fromHex(message.value("sha256").toString().toLatin1()),
                {},
            };
            incomingFiles_.insert(transferId, incoming);
            if (fileOfferHandler_) {
                fileOfferHandler_(transferId, incoming.sender, incoming.name, incoming.size);
            }
        } else if (type == "file_response") {
            const QString transferId = message.value("transferId").toString();
            auto iterator = outgoingFiles_.find(transferId);
            if (iterator == outgoingFiles_.end()) {
                continue;
            }
            if (!message.value("accepted").toBool()) {
                if (fileResultHandler_) {
                    fileResultHandler_(false, message.value("message").toString());
                }
                outgoingFiles_.erase(iterator);
                continue;
            }

            const qint64 total = iterator->data.size();
            for (qsizetype offset = 0; offset < iterator->data.size(); offset += fileChunkSize) {
                const QByteArray chunk = iterator->data.mid(offset, fileChunkSize);
                socket_->write(MessageProtocol::encode({
                    {"type", "file_chunk"},
                    {"transferId", transferId},
                    {"offset", offset},
                    {"data", QString::fromLatin1(chunk.toBase64())},
                }));
                if (fileProgressHandler_) {
                    fileProgressHandler_(iterator->name, offset + chunk.size(), total);
                }
            }
            socket_->write(MessageProtocol::encode({
                {"type", "file_complete"},
                {"transferId", transferId},
            }));
        } else if (type == "file_chunk") {
            const QString transferId = message.value("transferId").toString();
            auto iterator = incomingFiles_.find(transferId);
            if (iterator == incomingFiles_.end()) {
                continue;
            }
            const qint64 offset = message.value("offset").toInteger(-1);
            const QByteArray chunk = QByteArray::fromBase64(
                message.value("data").toString().toLatin1());
            if (offset != iterator->data.size()) {
                if (fileResultHandler_) {
                    fileResultHandler_(false, "Blocos recebidos fora de ordem");
                }
                incomingFiles_.erase(iterator);
                continue;
            }
            iterator->data.append(chunk);
            if (fileProgressHandler_) {
                fileProgressHandler_(iterator->name, iterator->data.size(), iterator->size);
            }
        } else if (type == "file_complete") {
            const QString transferId = message.value("transferId").toString();
            auto iterator = incomingFiles_.find(transferId);
            if (iterator == incomingFiles_.end()) {
                continue;
            }
            const bool valid = iterator->data.size() == iterator->size
                               && QCryptographicHash::hash(
                                      iterator->data, QCryptographicHash::Sha256)
                                      == iterator->sha256;
            QFile output(iterator->savePath);
            const bool saved = valid && output.open(QIODevice::WriteOnly)
                               && output.write(iterator->data) == iterator->data.size();
            if (fileResultHandler_) {
                fileResultHandler_(saved, saved ? QString("Arquivo salvo em %1").arg(iterator->savePath)
                                                : QString("Falha ao validar ou salvar %1").arg(iterator->name));
            }
            incomingFiles_.erase(iterator);
        } else if (type == "file_result") {
            const QString transferId = message.value("transferId").toString();
            outgoingFiles_.remove(transferId);
            incomingFiles_.remove(transferId);
            if (fileResultHandler_) {
                fileResultHandler_(message.value("success").toBool(),
                                   message.value("message").toString());
            }
        } else if (type == "error" && statusHandler_) {
            statusHandler_(QString("Erro do servidor: %1").arg(message.value("message").toString()));
        }
    }
}
