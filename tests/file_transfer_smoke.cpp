#include "network/chat_client.hpp"
#include "network/chat_server.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QTimer>

#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) {
        return 1;
    }

    QByteArray expected;
    expected.reserve(90000);
    for (int index = 0; index < 90000; ++index) {
        expected.append(static_cast<char>(index % 251));
    }

    const QString sourcePath = directory.filePath("origem.bin");
    const QString destinationPath = directory.filePath("destino.bin");
    QFile source(sourcePath);
    if (!source.open(QIODevice::WriteOnly) || source.write(expected) != expected.size()) {
        return 1;
    }
    source.close();

    ChatServer server(nullptr, directory.filePath("chat.db"));
    ChatClient sender;
    ChatClient recipient;
    QString error;
    bool senderCompleted = false;
    bool recipientCompleted = false;

    if (!server.start(0, &error)) {
        std::cerr << error.toStdString() << '\n';
        return 1;
    }

    const auto finish = [&] {
        if (!senderCompleted || !recipientCompleted) {
            return;
        }
        QFile received(destinationPath);
        if (!received.open(QIODevice::ReadOnly) || received.readAll() != expected) {
            std::cerr << "Conteúdo recebido é diferente do original\n";
            application.exit(1);
            return;
        }
        application.exit(0);
    };

    sender.setProtocolHandler([&] { sender.registerAccount("alice", ""); });
    sender.setAuthenticationHandler([&](bool success, const QString &, const QString &) {
        if (success) {
            recipient.connectToServer("127.0.0.1", server.port());
        }
    });
    sender.setFileResultHandler([&](bool success, const QString &) {
        senderCompleted = success;
        finish();
    });

    recipient.setProtocolHandler([&] { recipient.registerAccount("bob", ""); });
    recipient.setAuthenticationHandler([&](bool success, const QString &, const QString &) {
        if (success && !sender.offerFile("bob", sourcePath, &error)) {
            std::cerr << error.toStdString() << '\n';
            application.exit(1);
        }
    });
    recipient.setFileOfferHandler(
        [&](const QString &transferId, const QString &, const QString &, qint64) {
            recipient.respondToFile(transferId, true, destinationPath);
        });
    recipient.setFileResultHandler([&](bool success, const QString &) {
        recipientCompleted = success;
        finish();
    });

    QTimer::singleShot(5000, &application, [&] {
        std::cerr << "Timeout na transferência de arquivo\n";
        application.exit(1);
    });
    sender.connectToServer("127.0.0.1", server.port());
    return application.exec();
}
