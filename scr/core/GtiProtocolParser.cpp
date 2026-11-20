#include "GtiProtocolParser.h"
#include <QDebug>
#include <cstring>
#include <cmath>

GtiProtocolParser::GtiProtocolParser(QObject *parent) : QObject(parent)
{
}

void GtiProtocolParser::feedData(const QByteArray &data, int port)
{
    m_buffers[port].append(data);
    while (tryParsePacket(m_buffers[port], port)) {}
}

bool GtiProtocolParser::tryParsePacket(QByteArray &buffer, int port)
{
    if (buffer.size() < 2) return false;

    uint16_t packetLen = readU16LE(buffer, 0);
    if (packetLen < 3 || packetLen > 60000) {
        buffer.clear();
        return false;
    }

    if (buffer.size() < packetLen) return false;

    QByteArray payload = buffer.mid(2, packetLen - 2);
    buffer.remove(0, packetLen);

    if (payload.size() == 1) return true; // Heartbeat

    uint8_t type = uint8_t(payload[0]);
    if (type == 0x53) {
        parseConfig(payload, port);
        return true;
    }
    if (type == 0x4D || type == 0x44) {   // 'M' = архивный срез, 'D' = живой срез
        parseData(payload, port);
        return true;
    }

    return true;
}

void GtiProtocolParser::parseConfig(const QByteArray &payload, int port)
{
    QVector<uint16_t> sensorIds;
    for (int i = 3; i + 1 < payload.size(); i += 2) {
        uint16_t id = readU16LE(payload, i);
        if (id == 0) break;
        sensorIds.append(id);
    }

    m_configs[port] = sensorIds;
    emit configReceived(port, sensorIds);
}

void GtiProtocolParser::parseData(const QByteArray &payload, int port)
{
    if (!m_configs.contains(port)) return;

    const QVector<uint16_t> &sensorIds = m_configs[port];
    int expectedSize = 1 + 8 * (sensorIds.size() + 1);
    if (payload.size() < expectedSize) return;

    double timestamp = readF64LE(payload, 1);
    QMap<uint16_t, double> values;

    int offset = 9;
    for (uint16_t id : sensorIds) {
        if (offset + 8 > payload.size()) break;
        double value = readF64LE(payload, offset);
        offset += 8;

        if (!std::isnan(value) && !std::isinf(value) && value > -2147483647.0 && std::fabs(value) < 1e12) {
            values[id] = value;
        }
    }

    emit dataParsed(port, values, timestamp);
}

uint16_t GtiProtocolParser::readU16LE(const QByteArray &data, int offset)
{
    return uint16_t(uint8_t(data[offset]) | (uint8_t(data[offset + 1]) << 8));
}

double GtiProtocolParser::readF64LE(const QByteArray &data, int offset)
{
    uint64_t bits = 0;
    for (int i = 0; i < 8; ++i) {
        bits |= uint64_t(uint8_t(data[offset + i])) << (i * 8);
    }
    double value;
    std::memcpy(&value, &bits, sizeof(double));
    return value;
}
