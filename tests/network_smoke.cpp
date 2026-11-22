#include "network/chat_client.hpp"
#include "network/chat_server.hpp"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QTimer>

#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) {
        std::cerr << "Falha ao criar diretório temporário\n";
        return 1;
    }
    ChatServer server(nullptr, temporaryDirectory.filePath("chat.db"));
    ChatClient firstClient;
    ChatClient secondClient;
    ChatClient thirdClient;
    QString errorMessage;
    bool firstAuthenticated = false;
    bool duplicateRejected = false;
    bool secondAuthenticated = false;
    bool userListReceived = false;
    bool broadcastReceived = false;
    bool privateReceived = false;
    bool privateEchoed = false;
    bool offlineRejected = false;
    bool secondJoinedRoom = false;
    bool firstJoinedRoom = false;
    bool thirdAuthenticated = false;
    bool roomMessageReceived = false;
    bool roomMessageLeaked = false;
    bool historyReceived = false;
    bool roomIsolationVerified = false;
    bool secondLeftRoom = false;
    bool roomMessageSent = false;

    if (!server.start(0, &errorMessage)) {
        std::cerr << "Falha ao iniciar servidor: " << errorMessage.toStdString() << '\n';
        return 1;
    }

    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &application, [&] {
        std::cerr << "Timeout: auth=" << firstAuthenticated << secondAuthenticated
                  << thirdAuthenticated << " duplicate=" << duplicateRejected
                  << " general=" << broadcastReceived << " private=" << privateReceived
                  << privateEchoed << offlineRejected << " rooms=" << firstJoinedRoom
                  << secondJoinedRoom << roomMessageReceived << roomIsolationVerified
                  << secondLeftRoom << '\n';
        application.exit(1);
    });

    const auto finishWhenComplete = [&] {
        if (firstAuthenticated && duplicateRejected && secondAuthenticated
            && userListReceived && broadcastReceived && privateReceived
            && privateEchoed && offlineRejected && roomMessageReceived
            && roomIsolationVerified && secondLeftRoom && historyReceived) {
            application.exit(0);
        }
    };

    const auto sendRoomMessageWhenReady = [&] {
        if (firstJoinedRoom && secondJoinedRoom && thirdAuthenticated && !roomMessageSent) {
            roomMessageSent = true;
            firstClient.sendRoomMessage("jogos", "partida");
        }
    };

    firstClient.setProtocolHandler([&] {
        firstClient.registerAccount("alice", "");
    });
    firstClient.setAuthenticationHandler(
        [&](bool success, const QString &, const QString &) {
            firstAuthenticated = success;
            if (success) {
                secondClient.connectToServer("127.0.0.1", server.port());
            }
            finishWhenComplete();
        });
    firstClient.setUserListHandler([&](const QStringList &users) {
        userListReceived = userListReceived
                           || (users.contains("alice") && users.contains("bob"));
        finishWhenComplete();
    });
    firstClient.setChatMessageHandler(
        [&](const QString &sender, const QString &text, const QString &) {
            broadcastReceived = sender == "bob" && text == "ola";
            finishWhenComplete();
        });
    firstClient.setPrivateMessageHandler(
        [&](const QString &sender, const QString &recipient, const QString &text,
            const QString &) {
            privateReceived = sender == "bob" && recipient == "alice" && text == "segredo";
            finishWhenComplete();
        });
    firstClient.setRoomListHandler([&](const QStringList &rooms) {
        if (rooms.contains("jogos") && !firstJoinedRoom) {
            firstClient.joinRoom("jogos");
        }
    });
    firstClient.setRoomMembershipHandler([&](const QString &room, bool joined) {
        if (room == "jogos" && joined) {
            firstJoinedRoom = true;
            sendRoomMessageWhenReady();
        }
    });

    secondClient.setProtocolHandler([&] {
        secondClient.registerAccount("alice", "outra senha");
    });
    secondClient.setAuthenticationHandler(
        [&](bool success, const QString &, const QString &) {
            if (!success && !duplicateRejected) {
                duplicateRejected = true;
                secondClient.registerAccount("bob", "senha");
            } else if (success) {
                secondAuthenticated = true;
                secondClient.sendGeneralMessage("ola");
                secondClient.sendPrivateMessage("alice", "segredo");
                secondClient.sendPrivateMessage("dave", "offline");
                secondClient.createRoom("jogos");
                thirdClient.connectToServer("127.0.0.1", server.port());
            }
            finishWhenComplete();
        });
    secondClient.setPrivateMessageHandler(
        [&](const QString &sender, const QString &recipient, const QString &text,
            const QString &) {
            privateEchoed = sender == "bob" && recipient == "alice" && text == "segredo";
            finishWhenComplete();
        });
    secondClient.setPrivateMessageResultHandler([&](bool success, const QString &message) {
        offlineRejected = !success && message.contains("dave");
        finishWhenComplete();
    });
    secondClient.setRoomMembershipHandler([&](const QString &room, bool joined) {
        if (room != "jogos") {
            return;
        }
        if (joined) {
            secondJoinedRoom = true;
            sendRoomMessageWhenReady();
        } else {
            secondLeftRoom = true;
            finishWhenComplete();
        }
    });
    secondClient.setRoomMessageHandler(
        [&](const QString &room, const QString &sender, const QString &text,
            const QString &) {
            roomMessageReceived = room == "jogos" && sender == "alice" && text == "partida";
            secondClient.leaveRoom("jogos");
            QTimer::singleShot(50, &application, [&] {
                roomIsolationVerified = !roomMessageLeaked;
                finishWhenComplete();
            });
        });

    thirdClient.setProtocolHandler([&] {
        thirdClient.registerAccount("carol", "");
    });
    thirdClient.setAuthenticationHandler(
        [&](bool success, const QString &, const QString &) {
            thirdAuthenticated = success;
            sendRoomMessageWhenReady();
        });
    thirdClient.setRoomMessageHandler(
        [&](const QString &room, const QString &sender, const QString &text, const QString &) {
            if (room == "geral" && sender == "bob" && text == "ola") {
                historyReceived = true;
            }
            if (room == "jogos" && text == "partida") {
                roomMessageLeaked = true;
            }
            finishWhenComplete();
        });

    timeout.start(5000);
    firstClient.connectToServer("127.0.0.1", server.port());
    return application.exec();
}
