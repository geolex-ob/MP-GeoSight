#pragma once
#include <QObject>
#include <QByteArray>
#include <QVector>
#include <QMap>

class GtiProtocolParser : public QObject
{
    Q_OBJECT

public:
    explicit GtiProtocolParser(QObject *parent = nullptr);

    void feedData(const QByteArray &data, int port);

signals:
    void configReceived(int port, const QVector<uint16_t> &sensorIds);
    void dataParsed(int port, const QMap<uint16_t, double> &values, double timestamp);

private:
    bool tryParsePacket(QByteArray &buffer, int port);
    void parseConfig(const QByteArray &payload, int port);
    void parseData(const QByteArray &payload, int port);

    static uint16_t readU16LE(const QByteArray &data, int offset);
    static double readF64LE(const QByteArray &data, int offset);

    QMap<int, QByteArray> m_buffers;
    QMap<int, QVector<uint16_t>> m_configs;
};
