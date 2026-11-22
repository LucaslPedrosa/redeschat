#include "protocol/message_protocol.hpp"

#include <QJsonDocument>
#include <QtEndian>

#include <cstring>

QByteArray MessageProtocol::encode(const QJsonObject &message)
{
    const QByteArray payload = QJsonDocument(message).toJson(QJsonDocument::Compact);
    if (payload.isEmpty() || payload.size() > maximumPayloadSize) {
        return {};
    }

    QByteArray frame;
    frame.resize(static_cast<qsizetype>(headerSize) + payload.size());
    qToBigEndian(static_cast<quint32>(payload.size()), frame.data());
    std::memcpy(frame.data() + headerSize, payload.constData(), static_cast<std::size_t>(payload.size()));
    return frame;
}

MessageProtocol::DecodeResult MessageProtocol::tryDecode(
    QByteArray &buffer, QJsonObject &message, QString &errorMessage)
{
    if (buffer.size() < static_cast<qsizetype>(headerSize)) {
        return DecodeResult::NeedMoreData;
    }

    const auto payloadSize = qFromBigEndian<quint32>(buffer.constData());
    if (payloadSize == 0 || payloadSize > maximumPayloadSize) {
        errorMessage = QString("Tamanho de mensagem inválido: %1 bytes").arg(payloadSize);
        buffer.clear();
        return DecodeResult::InvalidFrame;
    }

    const qsizetype frameSize = static_cast<qsizetype>(headerSize) + payloadSize;
    if (buffer.size() < frameSize) {
        return DecodeResult::NeedMoreData;
    }

    QJsonParseError parseError;
    const QByteArray payload = buffer.mid(headerSize, payloadSize);
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    buffer.remove(0, frameSize);

    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        errorMessage = QString("JSON inválido: %1").arg(parseError.errorString());
        return DecodeResult::InvalidFrame;
    }

    message = document.object();
    errorMessage.clear();
    return DecodeResult::MessageReady;
}
