#include "database/database.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

namespace {
constexpr int passwordHashIterations = 10000;
}

Database::Database(QString path)
    : path_(path.isEmpty() ? defaultPath() : std::move(path)),
      connectionName_(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

Database::~Database()
{
    if (database_.isValid()) {
        database_.close();
        database_ = {};
    }
    QSqlDatabase::removeDatabase(connectionName_);
}

bool Database::open(QString *errorMessage)
{
    if (database_.isOpen()) {
        return true;
    }

    const QFileInfo fileInfo(path_);
    if (!QDir().mkpath(fileInfo.absolutePath())) {
        setError(errorMessage, "Não foi possível criar o diretório do banco");
        return false;
    }

    database_ = QSqlDatabase::addDatabase("QSQLITE", connectionName_);
    database_.setDatabaseName(path_);
    if (!database_.open()) {
        setError(errorMessage, database_.lastError().text());
        return false;
    }

    QSqlQuery query(database_);
    if (!query.exec("PRAGMA foreign_keys = ON")
        || !query.exec("PRAGMA journal_mode = WAL")
        || !query.exec("CREATE TABLE IF NOT EXISTS users ("
                       "username TEXT PRIMARY KEY, "
                       "password_hash BLOB NOT NULL, "
                       "salt BLOB NOT NULL)")
        || !query.exec("CREATE TABLE IF NOT EXISTS rooms ("
                       "name TEXT PRIMARY KEY, "
                       "created_at TEXT NOT NULL)")
        || !query.exec("CREATE TABLE IF NOT EXISTS room_messages ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                       "room TEXT NOT NULL REFERENCES rooms(name), "
                       "sender TEXT NOT NULL REFERENCES users(username), "
                       "content TEXT NOT NULL, "
                       "created_at TEXT NOT NULL)")
        || !query.exec("CREATE INDEX IF NOT EXISTS idx_room_messages_room_id "
                       "ON room_messages(room, id DESC)")) {
        setError(errorMessage, query.lastError().text());
        return false;
    }

    query.prepare("INSERT OR IGNORE INTO rooms(name, created_at) VALUES(?, ?)");
    query.addBindValue("geral");
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    if (!query.exec()) {
        setError(errorMessage, query.lastError().text());
        return false;
    }
    return true;
}

QString Database::path() const
{
    return path_;
}

bool Database::accountExists(const QString &username) const
{
    QSqlQuery query(database_);
    query.prepare("SELECT 1 FROM users WHERE username = ?");
    query.addBindValue(username);
    return query.exec() && query.next();
}

bool Database::createAccount(
    const QString &username, const QString &password, QString *errorMessage)
{
    QByteArray salt(16, Qt::Uninitialized);
    for (char &byte : salt) {
        byte = static_cast<char>(QRandomGenerator::global()->generate() & 0xff);
    }

    QSqlQuery query(database_);
    query.prepare("INSERT INTO users(username, password_hash, salt) VALUES(?, ?, ?)");
    query.addBindValue(username);
    query.addBindValue(derivePasswordHash(password, salt));
    query.addBindValue(salt);
    if (!query.exec()) {
        setError(errorMessage, query.lastError().text());
        return false;
    }
    return true;
}

bool Database::verifyPassword(const QString &username, const QString &password) const
{
    QSqlQuery query(database_);
    query.prepare("SELECT password_hash, salt FROM users WHERE username = ?");
    query.addBindValue(username);
    if (!query.exec() || !query.next()) {
        return false;
    }
    const QByteArray expected = query.value(0).toByteArray();
    const QByteArray salt = query.value(1).toByteArray();
    return derivePasswordHash(password, salt) == expected;
}

QStringList Database::rooms() const
{
    QStringList result;
    QSqlQuery query("SELECT name FROM rooms ORDER BY name", database_);
    while (query.next()) {
        result.append(query.value(0).toString());
    }
    return result;
}

bool Database::createRoom(const QString &room, QString *errorMessage)
{
    QSqlQuery query(database_);
    query.prepare("INSERT INTO rooms(name, created_at) VALUES(?, ?)");
    query.addBindValue(room);
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    if (!query.exec()) {
        setError(errorMessage, query.lastError().text());
        return false;
    }
    return true;
}

bool Database::saveRoomMessage(
    const QString &room, const QString &sender, const QString &text,
    const QDateTime &timestamp, QString *errorMessage)
{
    QSqlQuery query(database_);
    query.prepare("INSERT INTO room_messages(room, sender, content, created_at) "
                  "VALUES(?, ?, ?, ?)");
    query.addBindValue(room);
    query.addBindValue(sender);
    query.addBindValue(text);
    query.addBindValue(timestamp.toUTC().toString(Qt::ISODateWithMs));
    if (!query.exec()) {
        setError(errorMessage, query.lastError().text());
        return false;
    }
    return true;
}

QVector<StoredMessage> Database::recentRoomMessages(const QString &room, int limit) const
{
    QVector<StoredMessage> result;
    QSqlQuery query(database_);
    query.prepare("SELECT sender, content, created_at FROM ("
                  "SELECT id, sender, content, created_at FROM room_messages "
                  "WHERE room = ? ORDER BY id DESC LIMIT ?) ORDER BY id ASC");
    query.addBindValue(room);
    query.addBindValue(limit);
    if (!query.exec()) {
        return result;
    }
    while (query.next()) {
        result.append({
            query.value(0).toString(),
            query.value(1).toString(),
            QDateTime::fromString(query.value(2).toString(), Qt::ISODate),
        });
    }
    return result;
}

QString Database::defaultPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + "/redeschat.db";
}

QByteArray Database::derivePasswordHash(const QString &password, const QByteArray &salt)
{
    const QByteArray passwordBytes = password.toUtf8();
    QByteArray hash = QCryptographicHash::hash(salt + passwordBytes, QCryptographicHash::Sha256);
    for (int iteration = 1; iteration < passwordHashIterations; ++iteration) {
        hash = QCryptographicHash::hash(hash + salt + passwordBytes, QCryptographicHash::Sha256);
    }
    return hash;
}

void Database::setError(QString *destination, const QString &message)
{
    if (destination != nullptr) {
        *destination = message;
    }
}
