#include "database/database.hpp"

#include <QCoreApplication>
#include <QTemporaryDir>

#include <iostream>

namespace {
bool require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "Falha: " << message << '\n';
    }
    return condition;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QTemporaryDir temporaryDirectory;
    if (!require(temporaryDirectory.isValid(), "diretório temporário inválido")) {
        return 1;
    }

    const QString path = temporaryDirectory.filePath("persistence.db");
    const QDateTime firstTimestamp = QDateTime::currentDateTimeUtc().addSecs(-1);
    const QDateTime secondTimestamp = QDateTime::currentDateTimeUtc();
    QString error;

    {
        Database database(path);
        if (!require(database.open(&error), "não abriu o banco")
            || !require(database.rooms().contains("geral"), "sala geral não foi criada")
            || !require(database.createAccount("alice", "", &error), "não criou conta")
            || !require(database.verifyPassword("alice", ""), "senha vazia não foi validada")
            || !require(!database.verifyPassword("alice", "errada"), "senha incorreta foi aceita")
            || !require(database.createRoom("jogos", &error), "não criou sala")
            || !require(database.saveRoomMessage(
                            "jogos", "alice", "primeira", firstTimestamp, &error),
                        "não salvou primeira mensagem")
            || !require(database.saveRoomMessage(
                            "jogos", "alice", "segunda", secondTimestamp, &error),
                        "não salvou segunda mensagem")) {
            std::cerr << error.toStdString() << '\n';
            return 1;
        }
    }

    {
        Database reopened(path);
        if (!require(reopened.open(&error), "não reabriu o banco")
            || !require(reopened.accountExists("alice"), "conta não persistiu")
            || !require(reopened.verifyPassword("alice", ""), "hash não persistiu")
            || !require(reopened.rooms().contains("jogos"), "sala não persistiu")) {
            std::cerr << error.toStdString() << '\n';
            return 1;
        }

        const QVector<StoredMessage> history = reopened.recentRoomMessages("jogos");
        if (!require(history.size() == 2, "quantidade incorreta no histórico")
            || !require(history[0].text == "primeira", "ordem inicial incorreta")
            || !require(history[1].text == "segunda", "ordem final incorreta")) {
            return 1;
        }
    }

    return 0;
}
