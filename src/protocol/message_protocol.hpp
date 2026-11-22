#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace MessageProtocol {
constexpr quint32 headerSize = sizeof(quint32);
constexpr quint32 maximumPayloadSize = 1024 * 1024;
constexpr int currentVersion = 1;

enum class DecodeResult {
    NeedMoreData,
    MessageReady,
    InvalidFrame
};

QByteArray encode(const QJsonObject &message);
DecodeResult tryDecode(QByteArray &buffer, QJsonObject &message, QString &errorMessage);
}
