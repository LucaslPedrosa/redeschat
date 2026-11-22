#pragma once

#include <QDateTime>
#include <QSqlDatabase>
#include <QStringList>
#include <QVector>

struct StoredMessage {
    QString sender;
    QString text;
    QDateTime timestamp;
};

class Database final
{
public:
    explicit Database(QString path = {});
    ~Database();

    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    bool open(QString *errorMessage = nullptr);
    QString path() const;

    bool accountExists(const QString &username) const;
    bool createAccount(const QString &username, const QString &password,
                       QString *errorMessage = nullptr);
    bool verifyPassword(const QString &username, const QString &password) const;

    QStringList rooms() const;
    bool createRoom(const QString &room, QString *errorMessage = nullptr);

    bool saveRoomMessage(const QString &room, const QString &sender, const QString &text,
                         const QDateTime &timestamp, QString *errorMessage = nullptr);
    QVector<StoredMessage> recentRoomMessages(const QString &room, int limit = 100) const;

    static QString defaultPath();

private:
    static QByteArray derivePasswordHash(const QString &password, const QByteArray &salt);
    static void setError(QString *destination, const QString &message);

    QString path_;
    QString connectionName_;
    QSqlDatabase database_;
};
