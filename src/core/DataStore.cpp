#include "DataStore.h"
#include <QDateTime>
#include <QTimeZone>
#include <QMutexLocker>
#include <algorithm>

DataStore::DataStore(QObject *parent) : QObject(parent)
{
    m_frameTimer.setInterval(500);
    connect(&m_frameTimer, &QTimer::timeout, this, [this]{
        if (m_dirty) { m_dirty = false; emit frameReady(); }
    });
    m_frameTimer.start();
}

void DataStore::ingest(const QMap<uint16_t, double> &values, double delphiTime)
{
    const double now = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    // Delphi TDateTime -> unix-секунды (эпоха 1899-12-30 = 25569 суток)
    const double t = (delphiTime - 25569.0) * 86400.0;

    QMutexLocker locker(&m_mutex);
    m_serverTime = delphiTime;
    m_lastWall = now;
    m_lastEpoch = t;

    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        auto &buf = m_data[it.key()];
        buf.append({t, it.value()});

        if (buf.size() > m_capacity + 512)
            buf.remove(0, buf.size() - m_capacity);

        const double cutoff = t - m_keepSeconds;
        if (buf.size() > 64 && buf.first().t < cutoff) {
            auto lb = std::lower_bound(buf.begin(), buf.end(), cutoff,
                                       [](const Sample &s, double t){ return s.t < t; });
            const int idx = int(lb - buf.begin());
            if (idx > 0) buf.remove(0, idx);
        }

        m_latest[it.key()] = it.value();
    }
    m_dirty = true;
}

double DataStore::latest(uint16_t id) const
{
    QMutexLocker locker(&m_mutex);
    return m_latest.value(id, 0.0);
}

bool DataStore::has(uint16_t id) const
{
    QMutexLocker locker(&m_mutex);
    return m_latest.contains(id);
}

QVector<Sample> DataStore::snapshot(uint16_t id) const
{
    QMutexLocker locker(&m_mutex);
    auto it = m_data.constFind(id);
    return it == m_data.constEnd() ? QVector<Sample>() : it.value();
}

double DataStore::lastEpoch() const
{
    QMutexLocker locker(&m_mutex);
    return m_lastEpoch;
}

double DataStore::secondsSinceUpdate() const
{
    QMutexLocker locker(&m_mutex);
    if (m_lastWall <= 0) return -1;
    return QDateTime::currentMSecsSinceEpoch() / 1000.0 - m_lastWall;
}

QString DataStore::serverTimeString() const
{
    double ts;
    {
        QMutexLocker locker(&m_mutex);
        ts = m_serverTime;
    }
    if (ts <= 0) return "--.--.---- --:--:--";
    const QDateTime epoch(QDate(1899, 12, 30), QTime(0, 0), QTimeZone::UTC);
    return epoch.addMSecs(qint64(ts * 86400000.0))
               .toString("dd.MM.yyyy HH:mm:ss");
}

void DataStore::setTrackedSensors(const QVariantList &ids)
{
    QMutexLocker locker(&m_mutex);
    m_tracked.clear();
    for (const QVariant &v : ids) m_tracked.insert(v.toInt());
}

QVariantList DataStore::rangeOf(int id)
{
    const QVector<Sample> s = snapshot(uint16_t(id));
    QVariantList out;
    if (s.isEmpty()) { out.append(0.0); out.append(1.0); return out; }
    double mn = s.first().v, mx = s.first().v;
    for (const Sample &p : s) { mn = qMin(mn, p.v); mx = qMax(mx, p.v); }
    if (mx - mn < 1e-9) { mn -= 1.0; mx += 1.0; }
    out.append(mn);
    out.append(mx);
    return out;
}

int DataStore::standsCount(int depthId, int bitId, int standLength, int mode) const
{
    QMutexLocker locker(&m_mutex);
    if (standLength <= 0) return 0;
    const double d = m_latest.value(uint16_t(depthId), 0.0);
    const double b = m_latest.value(uint16_t(bitId), 0.0);
    double base = 0.0;
    switch (mode) {
    case 1:  base = d;          break;   // забой / длина свечи
    case 2:  base = d - b;      break;   // (забой - долото) / длина свечи
    default: base = b;          break;   // долото / длина свечи
    }
    if (base < 0.5) return 0;
    return int(base / standLength);
}

double DataStore::bufferedHours() const
{
    QMutexLocker locker(&m_mutex);
    if (m_data.isEmpty()) return 0.0;
    const QVector<Sample> &buf = m_data.constBegin().value();
    if (buf.size() < 2) return 0.0;
    return (buf.last().t - buf.first().t) / 3600.0;
}
