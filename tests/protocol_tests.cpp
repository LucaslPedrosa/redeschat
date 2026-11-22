#include "protocol/message_protocol.hpp"

#include <QJsonObject>
#include <QtEndian>

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

int main()
{
    const QJsonObject first{{"type", "message"}, {"text", "ola"}};
    const QJsonObject second{{"type", "list_users"}};
    const QByteArray firstFrame = MessageProtocol::encode(first);
    const QByteArray secondFrame = MessageProtocol::encode(second);

    if (!require(!firstFrame.isEmpty(), "codificação produziu frame vazio")) {
        return 1;
    }

    QByteArray fragmented = firstFrame.left(3);
    QJsonObject decoded;
    QString error;
    if (!require(MessageProtocol::tryDecode(fragmented, decoded, error)
                     == MessageProtocol::DecodeResult::NeedMoreData,
                 "cabeçalho fragmentado deveria aguardar mais dados")) {
        return 1;
    }

    fragmented.append(firstFrame.mid(3));
    if (!require(MessageProtocol::tryDecode(fragmented, decoded, error)
                     == MessageProtocol::DecodeResult::MessageReady,
                 "frame completo não foi decodificado")
        || !require(decoded == first, "JSON decodificado é diferente do original")) {
        return 1;
    }

    QByteArray combined = firstFrame + secondFrame;
    if (!require(MessageProtocol::tryDecode(combined, decoded, error)
                     == MessageProtocol::DecodeResult::MessageReady,
                 "primeiro frame concatenado não foi decodificado")
        || !require(decoded == first, "primeiro frame concatenado está incorreto")
        || !require(MessageProtocol::tryDecode(combined, decoded, error)
                        == MessageProtocol::DecodeResult::MessageReady,
                    "segundo frame concatenado não foi decodificado")
        || !require(decoded == second, "segundo frame concatenado está incorreto")
        || !require(combined.isEmpty(), "sobraram bytes após dois frames")) {
        return 1;
    }

    QByteArray oversized(MessageProtocol::headerSize, Qt::Uninitialized);
    qToBigEndian(MessageProtocol::maximumPayloadSize + 1, oversized.data());
    if (!require(MessageProtocol::tryDecode(oversized, decoded, error)
                     == MessageProtocol::DecodeResult::InvalidFrame,
                 "frame acima do limite deveria ser rejeitado")) {
        return 1;
    }

    return 0;
}
