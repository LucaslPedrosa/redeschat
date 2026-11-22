#include "network/chat_server.hpp"
#include "protocol/message_protocol.hpp"

#include <QHostAddress>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QTcpServer>
#include <QTcpSocket>

namespace {
constexpr qint64 maximumFileSize = 10 * 1024 * 1024;
}

ChatServer::ChatServer(QObject *parent, const QString &databasePath)
    : QObject(parent), server_(new QTcpServer(this)), database_(databasePath)
{
    rooms_.insert("geral", {});
    connect(server_, &QTcpServer::newConnection, this, [this] { acceptPendingConnections(); });
}

bool ChatServer::start(quint16 port, QString *errorMessage)
{
    if (server_->isListening()) {
        return true;
    }

    if (!database_.open(errorMessage)) {
        return false;
    }
    for (const QString &room : database_.rooms()) {
        if (!rooms_.contains(room)) {
            rooms_.insert(room, {});
        }
    }

    if (!server_->listen(QHostAddress::AnyIPv4, port)) {
        if (errorMessage != nullptr) {
            *errorMessage = server_->errorString();
        }
        return false;
    }

    reportEvent(QString("Servidor iniciado na porta %1; banco: %2")
                    .arg(server_->serverPort())
                    .arg(database_.path()));
    return true;
}

void ChatServer::stop()
{
    if (!server_->isListening()) {
        return;
    }

    server_->close();
    const auto clients = clients_.keys();
    clients_.clear();
    for (auto &members : rooms_) {
        members.clear();
    }
    for (QTcpSocket *client : clients) {
        client->disconnect(this);
        client->abort();
        client->deleteLater();
    }

    reportClientCount();
    reportEvent("Servidor encerrado");
}

bool ChatServer::isListening() const
{
    return server_->isListening();
}

quint16 ChatServer::port() const
{
    return server_->serverPort();
}

int ChatServer::clientCount() const
{
    return clients_.size();
}

void ChatServer::setEventHandler(EventHandler handler)
{
    eventHandler_ = std::move(handler);
}

void ChatServer::setClientCountHandler(ClientCountHandler handler)
{
    clientCountHandler_ = std::move(handler);
}

void ChatServer::acceptPendingConnections()
{
    while (server_->hasPendingConnections()) {
        QTcpSocket *client = server_->nextPendingConnection();
        if (client == nullptr) {
            continue;
        }

        clients_.insert(client, ClientSession{});
        const QString peer = QString("%1:%2")
                                 .arg(client->peerAddress().toString())
                                 .arg(client->peerPort());
        reportEvent(QString("Cliente conectado: %1").arg(peer));
        reportClientCount();

        connect(client, &QTcpSocket::readyRead, this, [this, client, peer] {
            processIncomingMessages(client, peer);
        });

        connect(client, &QTcpSocket::disconnected, this, [this, client, peer] {
            cancelTransfersFor(client);
            const ClientSession session = clients_.value(client);
            const QString username = session.username;
            for (const QString &room : session.joinedRooms) {
                rooms_[room].remove(client);
                if (!username.isEmpty()) {
                    broadcastToRoom(room, {
                                              {"type", "room_user_left"},
                                              {"room", room},
                                              {"username", username},
                                          });
                }
            }
            clients_.remove(client);
            reportEvent(QString("Cliente desconectado: %1").arg(peer));
            reportClientCount();
            if (!username.isEmpty()) {
                broadcast({
                    {"type", "user_left"},
                    {"username", username},
                });
                broadcastUserList();
                reportEvent(QString("Usuário saiu: %1").arg(username));
            }
            client->deleteLater();
        });
    }
}

void ChatServer::processIncomingMessages(QTcpSocket *client, const QString &peer)
{
    auto iterator = clients_.find(client);
    if (iterator == clients_.end()) {
        return;
    }

    iterator.value().receiveBuffer.append(client->readAll());
    while (true) {
        QJsonObject message;
        QString errorMessage;
        const auto result = MessageProtocol::tryDecode(
            iterator.value().receiveBuffer, message, errorMessage);
        if (result == MessageProtocol::DecodeResult::NeedMoreData) {
            return;
        }
        if (result == MessageProtocol::DecodeResult::InvalidFrame) {
            reportEvent(QString("Protocolo inválido de %1: %2").arg(peer, errorMessage));
            client->disconnectFromHost();
            return;
        }
        handleMessage(client, message, peer);
    }
}

void ChatServer::handleMessage(
    QTcpSocket *client, const QJsonObject &message, const QString &peer)
{
    const QString type = message.value("type").toString();
    if (type == "hello") {
        const int version = message.value("protocolVersion").toInt(-1);
        if (version != MessageProtocol::currentVersion) {
            sendMessage(client, {
                                    {"type", "error"},
                                    {"code", "unsupported_protocol"},
                                    {"message", "Versão de protocolo incompatível"},
                                });
            reportEvent(QString("Versão de protocolo incompatível de %1").arg(peer));
            client->disconnectFromHost();
            return;
        }

        sendMessage(client, {
                                {"type", "hello_ack"},
                                {"protocolVersion", MessageProtocol::currentVersion},
                            });
        clients_[client].protocolReady = true;
        reportEvent(QString("Protocolo confirmado com %1").arg(peer));
        return;
    }

    const auto iterator = clients_.constFind(client);
    if (iterator == clients_.constEnd() || !iterator.value().protocolReady) {
        sendMessage(client, {
                                {"type", "error"},
                                {"code", "protocol_not_ready"},
                                {"message", "Handshake ainda não concluído"},
                            });
        return;
    }

    if (type == "register") {
        handleRegister(client, message, peer);
        return;
    }
    if (type == "login") {
        handleLogin(client, message, peer);
        return;
    }
    if (iterator.value().username.isEmpty()) {
        sendMessage(client, {
                                {"type", "error"},
                                {"code", "not_authenticated"},
                                {"message", "Autenticação necessária"},
                            });
        return;
    }
    if (type == "general_message") {
        handleGeneralMessage(client, message);
        return;
    }
    if (type == "private_message") {
        handlePrivateMessage(client, message);
        return;
    }
    if (type == "list_users") {
        broadcastUserList();
        return;
    }
    if (type == "create_room") {
        handleCreateRoom(client, message);
        return;
    }
    if (type == "join_room") {
        handleJoinRoom(client, message);
        return;
    }
    if (type == "leave_room") {
        handleLeaveRoom(client, message);
        return;
    }
    if (type == "room_message") {
        handleRoomMessage(client, message);
        return;
    }
    if (type == "list_rooms") {
        sendRoomList(client);
        return;
    }
    if (type == "file_offer") {
        handleFileOffer(client, message);
        return;
    }
    if (type == "file_response") {
        handleFileResponse(client, message);
        return;
    }
    if (type == "file_chunk") {
        handleFileChunk(client, message);
        return;
    }
    if (type == "file_complete") {
        handleFileComplete(client, message);
        return;
    }

    sendMessage(client, {
                            {"type", "error"},
                            {"code", "unknown_message_type"},
                            {"message", QString("Tipo de mensagem desconhecido: %1").arg(type)},
                        });
    reportEvent(QString("Mensagem desconhecida de %1: %2").arg(peer, type));
}

void ChatServer::handleRegister(
    QTcpSocket *client, const QJsonObject &message, const QString &peer)
{
    const QString username = message.value("username").toString();
    const QString password = message.value("password").toString();

    if (username.isEmpty()) {
        sendMessage(client, {
                                {"type", "register_result"},
                                {"success", false},
                                {"message", "O apelido não pode ser vazio"},
                            });
        return;
    }
    if (database_.accountExists(username)) {
        sendMessage(client, {
                                {"type", "register_result"},
                                {"success", false},
                                {"message", "Este apelido já foi cadastrado"},
                            });
        return;
    }

    QString databaseError;
    if (!database_.createAccount(username, password, &databaseError)) {
        sendMessage(client, {
                                {"type", "register_result"},
                                {"success", false},
                                {"message", QString("Falha ao salvar conta: %1").arg(databaseError)},
                            });
        return;
    }
    sendMessage(client, {
                            {"type", "register_result"},
                            {"success", true},
                            {"username", username},
                            {"message", "Conta criada e autenticada"},
                        });
    completeAuthentication(client, username);
    reportEvent(QString("Usuário cadastrado: %1 (%2)").arg(username, peer));
}

void ChatServer::handleLogin(
    QTcpSocket *client, const QJsonObject &message, const QString &peer)
{
    const QString username = message.value("username").toString();
    const QString password = message.value("password").toString();
    QString failure;

    if (!database_.verifyPassword(username, password)) {
        failure = "Apelido ou senha incorretos";
    } else if (usernameIsOnline(username)) {
        failure = "Este usuário já está conectado";
    }

    if (!failure.isEmpty()) {
        sendMessage(client, {
                                {"type", "login_result"},
                                {"success", false},
                                {"message", failure},
                            });
        return;
    }

    sendMessage(client, {
                            {"type", "login_result"},
                            {"success", true},
                            {"username", username},
                            {"message", "Autenticado com sucesso"},
                        });
    completeAuthentication(client, username);
    reportEvent(QString("Login de %1 (%2)").arg(username, peer));
}

void ChatServer::handleGeneralMessage(QTcpSocket *client, const QJsonObject &message)
{
    const QString username = clients_.value(client).username;
    if (username.isEmpty()) {
        sendMessage(client, {
                                {"type", "error"},
                                {"code", "not_authenticated"},
                                {"message", "Autenticação necessária"},
                            });
        return;
    }

    const QString text = message.value("text").toString();
    if (text.isEmpty()) {
        return;
    }

    const QDateTime timestamp = QDateTime::currentDateTimeUtc();
    QString databaseError;
    if (!database_.saveRoomMessage("geral", username, text, timestamp, &databaseError)) {
        sendMessage(client, {{"type", "error"},
                             {"code", "database_error"},
                             {"message", databaseError}});
        return;
    }

    broadcast({
        {"type", "general_message"},
        {"sender", username},
        {"text", text},
        {"timestamp", timestamp.toString(Qt::ISODateWithMs)},
    });
    reportEvent(QString("Mensagem de %1 na sala geral").arg(username));
}

void ChatServer::handlePrivateMessage(QTcpSocket *client, const QJsonObject &message)
{
    const QString sender = clients_.value(client).username;
    if (sender.isEmpty()) {
        sendMessage(client, {
                                {"type", "error"},
                                {"code", "not_authenticated"},
                                {"message", "Autenticação necessária"},
                            });
        return;
    }

    const QString recipient = message.value("recipient").toString();
    const QString text = message.value("text").toString();
    if (recipient.isEmpty() || text.isEmpty()) {
        sendMessage(client, {
                                {"type", "private_message_result"},
                                {"success", false},
                                {"message", "Informe destinatário e mensagem"},
                            });
        return;
    }

    QTcpSocket *recipientClient = clientForUsername(recipient);
    if (recipientClient == nullptr) {
        sendMessage(client, {
                                {"type", "private_message_result"},
                                {"success", false},
                                {"message", QString("Usuário não conectado: %1").arg(recipient)},
                            });
        return;
    }

    const QJsonObject deliveredMessage{
        {"type", "private_message"},
        {"sender", sender},
        {"recipient", recipient},
        {"text", text},
        {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
    };
    sendMessage(recipientClient, deliveredMessage);
    if (recipientClient != client) {
        sendMessage(client, deliveredMessage);
    }
    reportEvent(QString("Mensagem privada de %1 para %2").arg(sender, recipient));
}

void ChatServer::handleCreateRoom(QTcpSocket *client, const QJsonObject &message)
{
    const QString room = message.value("room").toString();
    if (room.isEmpty()) {
        sendMessage(client, {
                                {"type", "room_result"},
                                {"success", false},
                                {"message", "O nome da sala não pode ser vazio"},
                            });
        return;
    }
    if (rooms_.contains(room)) {
        sendMessage(client, {
                                {"type", "room_result"},
                                {"success", false},
                                {"message", "Esta sala já existe"},
                            });
        return;
    }

    QString databaseError;
    if (!database_.createRoom(room, &databaseError)) {
        sendMessage(client, {
                                {"type", "room_result"},
                                {"success", false},
                                {"message", QString("Falha ao criar sala: %1").arg(databaseError)},
                            });
        return;
    }

    rooms_.insert(room, {client});
    clients_[client].joinedRooms.insert(room);
    sendMessage(client, {
                            {"type", "room_joined"},
                            {"room", room},
                            {"created", true},
                        });
    sendRoomList();
    reportEvent(QString("Sala criada: %1 por %2")
                    .arg(room, clients_.value(client).username));
}

void ChatServer::handleJoinRoom(QTcpSocket *client, const QJsonObject &message)
{
    const QString room = message.value("room").toString();
    if (!rooms_.contains(room)) {
        sendMessage(client, {
                                {"type", "room_result"},
                                {"success", false},
                                {"message", "Sala inexistente"},
                            });
        return;
    }
    if (clients_[client].joinedRooms.contains(room)) {
        sendMessage(client, {{"type", "room_joined"}, {"room", room}});
        return;
    }

    rooms_[room].insert(client);
    clients_[client].joinedRooms.insert(room);
    sendMessage(client, {{"type", "room_joined"}, {"room", room}});
    sendRoomHistory(client, room);
    broadcastToRoom(room, {
                              {"type", "room_user_joined"},
                              {"room", room},
                              {"username", clients_.value(client).username},
                          }, client);
}

void ChatServer::handleLeaveRoom(QTcpSocket *client, const QJsonObject &message)
{
    const QString room = message.value("room").toString();
    if (room == "geral") {
        sendMessage(client, {
                                {"type", "room_result"},
                                {"success", false},
                                {"message", "Não é possível sair da sala geral"},
                            });
        return;
    }
    if (!clients_[client].joinedRooms.remove(room)) {
        return;
    }

    rooms_[room].remove(client);
    sendMessage(client, {{"type", "room_left"}, {"room", room}});
    broadcastToRoom(room, {
                              {"type", "room_user_left"},
                              {"room", room},
                              {"username", clients_.value(client).username},
                          });
}

void ChatServer::handleRoomMessage(QTcpSocket *client, const QJsonObject &message)
{
    const QString room = message.value("room").toString();
    const QString text = message.value("text").toString();
    if (!clients_[client].joinedRooms.contains(room)) {
        sendMessage(client, {
                                {"type", "room_result"},
                                {"success", false},
                                {"message", "Entre na sala antes de enviar mensagens"},
                            });
        return;
    }
    if (text.isEmpty()) {
        return;
    }

    const QString sender = clients_.value(client).username;
    const QDateTime timestamp = QDateTime::currentDateTimeUtc();
    QString databaseError;
    if (!database_.saveRoomMessage(room, sender, text, timestamp, &databaseError)) {
        sendMessage(client, {{"type", "error"},
                             {"code", "database_error"},
                             {"message", databaseError}});
        return;
    }

    broadcastToRoom(room, {
                              {"type", "room_message"},
                              {"room", room},
                              {"sender", sender},
                              {"text", text},
                              {"timestamp", timestamp.toString(Qt::ISODateWithMs)},
                          });
    reportEvent(QString("Mensagem de %1 na sala %2")
                    .arg(sender, room));
}

void ChatServer::handleFileOffer(QTcpSocket *client, const QJsonObject &message)
{
    const QString transferId = message.value("transferId").toString();
    const QString recipientName = message.value("recipient").toString();
    const QString name = message.value("name").toString();
    const qint64 size = message.value("size").toInteger(-1);
    QTcpSocket *recipient = clientForUsername(recipientName);

    QString failure;
    if (transferId.isEmpty() || transfers_.contains(transferId)) {
        failure = "Identificador de transferência inválido";
    } else if (name.isEmpty() || size < 0 || size > maximumFileSize) {
        failure = "Arquivo inválido ou maior que 10 MiB";
    } else if (recipient == nullptr || recipient == client) {
        failure = "Destinatário indisponível";
    }

    if (!failure.isEmpty()) {
        sendMessage(client, {
                                {"type", "file_response"},
                                {"transferId", transferId},
                                {"accepted", false},
                                {"message", failure},
                            });
        return;
    }

    transfers_.insert(transferId, {client, recipient, name, size, 0});
    sendMessage(recipient, {
                               {"type", "file_offer"},
                               {"transferId", transferId},
                               {"sender", clients_.value(client).username},
                               {"name", name},
                               {"size", size},
                               {"sha256", message.value("sha256").toString()},
                           });
    reportEvent(QString("Arquivo oferecido por %1 para %2: %3")
                    .arg(clients_.value(client).username, recipientName, name));
}

void ChatServer::handleFileResponse(QTcpSocket *client, const QJsonObject &message)
{
    const QString transferId = message.value("transferId").toString();
    auto iterator = transfers_.find(transferId);
    if (iterator == transfers_.end() || iterator->recipient != client) {
        return;
    }

    const bool accepted = message.value("accepted").toBool();
    sendMessage(iterator->sender, {
                                      {"type", "file_response"},
                                      {"transferId", transferId},
                                      {"accepted", accepted},
                                      {"message", accepted ? "Transferência aceita" : "Transferência recusada"},
                                  });
    if (!accepted) {
        transfers_.erase(iterator);
    }
}

void ChatServer::handleFileChunk(QTcpSocket *client, const QJsonObject &message)
{
    const QString transferId = message.value("transferId").toString();
    auto iterator = transfers_.find(transferId);
    if (iterator == transfers_.end() || iterator->sender != client) {
        return;
    }

    const QByteArray data = QByteArray::fromBase64(message.value("data").toString().toLatin1());
    const qint64 offset = message.value("offset").toInteger(-1);
    if (data.isEmpty() || offset != iterator->received
        || iterator->received + data.size() > iterator->size) {
        sendMessage(client, {
                                {"type", "file_result"},
                                {"transferId", transferId},
                                {"success", false},
                                {"message", "Bloco de arquivo inválido"},
                            });
        sendMessage(iterator->recipient, {
                                            {"type", "file_result"},
                                            {"transferId", transferId},
                                            {"success", false},
                                            {"message", "Transferência interrompida"},
                                        });
        transfers_.erase(iterator);
        return;
    }

    iterator->received += data.size();
    sendMessage(iterator->recipient, message);
}

void ChatServer::handleFileComplete(QTcpSocket *client, const QJsonObject &message)
{
    const QString transferId = message.value("transferId").toString();
    auto iterator = transfers_.find(transferId);
    if (iterator == transfers_.end() || iterator->sender != client) {
        return;
    }

    if (iterator->received != iterator->size) {
        sendMessage(client, {{"type", "file_result"}, {"transferId", transferId},
                             {"success", false}, {"message", "Tamanho recebido incorreto"}});
        transfers_.erase(iterator);
        return;
    }

    sendMessage(iterator->recipient, message);
    sendMessage(iterator->sender, {
                                      {"type", "file_result"},
                                      {"transferId", transferId},
                                      {"success", true},
                                      {"message", "Arquivo enviado"},
                                  });
    transfers_.erase(iterator);
}

void ChatServer::cancelTransfersFor(QTcpSocket *client)
{
    for (auto iterator = transfers_.begin(); iterator != transfers_.end();) {
        if (iterator->sender == client || iterator->recipient == client) {
            QTcpSocket *other = iterator->sender == client ? iterator->recipient : iterator->sender;
            sendMessage(other, {
                                   {"type", "file_result"},
                                   {"transferId", iterator.key()},
                                   {"success", false},
                                   {"message", "O outro usuário desconectou"},
                               });
            iterator = transfers_.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

bool ChatServer::usernameIsOnline(const QString &username) const
{
    for (const ClientSession &session : clients_) {
        if (session.username == username) {
            return true;
        }
    }
    return false;
}

QTcpSocket *ChatServer::clientForUsername(const QString &username) const
{
    for (auto iterator = clients_.cbegin(); iterator != clients_.cend(); ++iterator) {
        if (iterator.value().username == username) {
            return iterator.key();
        }
    }
    return nullptr;
}

void ChatServer::completeAuthentication(QTcpSocket *client, const QString &username)
{
    clients_[client].username = username;
    clients_[client].joinedRooms.insert("geral");
    rooms_["geral"].insert(client);
    broadcast({
        {"type", "user_joined"},
        {"username", username},
    }, client);
    broadcastUserList();
    sendMessage(client, {{"type", "room_joined"}, {"room", "geral"}});
    sendRoomHistory(client, "geral");
    sendRoomList(client);
}

void ChatServer::broadcast(const QJsonObject &message, QTcpSocket *excludedClient)
{
    for (auto iterator = clients_.cbegin(); iterator != clients_.cend(); ++iterator) {
        if (iterator.key() != excludedClient && !iterator.value().username.isEmpty()) {
            sendMessage(iterator.key(), message);
        }
    }
}

void ChatServer::broadcastToRoom(
    const QString &room, const QJsonObject &message, QTcpSocket *excludedClient)
{
    for (QTcpSocket *member : rooms_.value(room)) {
        if (member != excludedClient) {
            sendMessage(member, message);
        }
    }
}

void ChatServer::broadcastUserList()
{
    QStringList usernames;
    for (const ClientSession &session : clients_) {
        if (!session.username.isEmpty()) {
            usernames.append(session.username);
        }
    }
    usernames.sort(Qt::CaseSensitive);

    QJsonArray users;
    for (const QString &username : usernames) {
        users.append(username);
    }
    broadcast({
        {"type", "user_list"},
        {"users", users},
    });
}

void ChatServer::sendRoomList(QTcpSocket *client)
{
    QStringList names = rooms_.keys();
    names.sort(Qt::CaseSensitive);
    QJsonArray rooms;
    for (const QString &name : names) {
        rooms.append(name);
    }
    const QJsonObject message{{"type", "room_list"}, {"rooms", rooms}};
    if (client != nullptr) {
        sendMessage(client, message);
    } else {
        broadcast(message);
    }
}

void ChatServer::sendRoomHistory(QTcpSocket *client, const QString &room)
{
    QJsonArray messages;
    for (const StoredMessage &stored : database_.recentRoomMessages(room)) {
        messages.append(QJsonObject{
            {"sender", stored.sender},
            {"text", stored.text},
            {"timestamp", stored.timestamp.toUTC().toString(Qt::ISODateWithMs)},
        });
    }
    sendMessage(client, {
                            {"type", "room_history"},
                            {"room", room},
                            {"messages", messages},
                        });
}

void ChatServer::sendMessage(QTcpSocket *client, const QJsonObject &message)
{
    const QByteArray frame = MessageProtocol::encode(message);
    if (!frame.isEmpty()) {
        client->write(frame);
    }
}

void ChatServer::reportEvent(const QString &message) const
{
    if (eventHandler_) {
        eventHandler_(message);
    }
}

void ChatServer::reportClientCount() const
{
    if (clientCountHandler_) {
        clientCountHandler_(clients_.size());
    }
}
