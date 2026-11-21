#pragma once
#include <QObject>
#include <QMap>
#include <QVector>
#include <QVariantList>
#include <QTimer>
#include <QMutex>
#include <QSet>
#include <cstdint>

struct Sample { double t; double v; };

class DataStore : public QObject
{
    Q_OBJECT
public:
    explicit DataStore(QObject *parent = nullptr);

    void ingest(const QMap<uint16_t, double> &values, double delphiTime);

    double latest(uint16_t id) const;
    bool   has(uint16_t id) const;
    QVector<Sample> snapshot(uint16_t id) const;

    Q_INVOKABLE double value(int id) const { return latest(uint16_t(id)); }
    Q_INVOKABLE QString serverTimeString() const;
    Q_INVOKABLE double secondsSinceUpdate() const;
    Q_INVOKABLE double lastEpoch() const;
    Q_INVOKABLE QVariantList rangeOf(int id);
    void setTrackedSensors(const QVariantList &ids);

signals:
    void frameReady();

private:
    mutable QMutex m_mutex;
    QMap<uint16_t, QVector<Sample>> m_data;
    QMap<uint16_t, double> m_latest;
    double m_serverTime = 0.0;
    double m_lastWall = 0.0;
    double m_lastEpoch = 0.0;
    QSet<int> m_tracked;
    bool m_dirty = false;
    QTimer m_frameTimer;
    int m_capacity = 20000;
    double m_keepSeconds = 345600;
};
