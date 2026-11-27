#include <QDebug>
#include "TimeAxisItem.h"
#include "core/DataStore.h"

#include <QPainter>
#include <QQuickWindow>
#include <QDateTime>
#include <QTimeZone>
#include <QDate>
#include <QFont>
#include <QFontMetrics>
#include <QtGlobal>
#include <QtMath>
#include <cmath>
#include <algorithm>

// UTC-Р Р†РЎР‚Р ВµР СРЎРЏ: Qt6 / Qt5
static QDateTime utcAt(double t)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QDateTime::fromSecsSinceEpoch(qint64(t), QTimeZone::utc());
#else
    return QDateTime::fromSecsSinceEpoch(qint64(t), Qt::UTC);
#endif
}

static double tickStep(double windowSec, double pixHeight, double minPitch)
{
    static const double steps[] = {1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 900,
                                   1800, 3600, 7200, 10800, 14400, 21600, 43200, 86400};
    for (double st : steps)
        if (pixHeight * st / windowSec >= minPitch) return st;
    return 86400.0;
}

TimeAxisItem::TimeAxisItem(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    setRenderTarget(QQuickPaintedItem::Image);
    setOpaquePainting(true);
    recomputeWidth();
}

void TimeAxisItem::recomputeWidth()
{
    QFont f;
    f.setPixelSize(m_fontPx);
    const QFontMetrics fm(f);

    const double dv = m_store ? m_store->latest(uint16_t(m_depth)) : 0.0;
    const double tv = m_store ? m_store->latest(uint16_t(m_toolDepth)) : 0.0;
    m_depthTextW = double(fm.horizontalAdvance(QString::number(dv, 'f', 1)));
    m_toolTextW  = double(fm.horizontalAdvance(QString::number(tv, 'f', 1)));

    const bool twoLines = m_store && !m_store->snapshot(uint16_t(m_toolDepth)).isEmpty();
    const double depthW = (twoLines ? std::max(m_depthTextW, m_toolTextW) : m_depthTextW) + 2;

    QFont f2 = f;
    if (twoLines) f2.setPixelSize(qMax(7, m_fontPx - 1));
    const double lh2 = QFontMetrics(f2).height();
    const double minPitch = twoLines ? qMax(33.0, 2.0 * lh2 + 6.0) : 33.0;

    double ph = height() - 4 - 10;
    if (!(ph > 10)) ph = 100;
    const double step = tickStep(m_window, ph, minPitch);
    m_timeTextW = double(fm.horizontalAdvance(step < 60 ? "00:00:00" : "00:00"));

    const int w = int(std::ceil(2 + depthW + m_timeTextW + 4));
    if (w != m_preferredWidth) {
        m_preferredWidth = w;
        emit paramsChanged();
    }
}

void TimeAxisItem::syncTextureSize()
{
    const QSize ts(qRound(width()), qRound(height()));
    if (ts.isValid() && !ts.isNull() && ts != textureSize()) {
        setTextureSize(ts);
        update();
    }
}

void TimeAxisItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);

    recomputeWidth();
    if (newGeometry.size() != oldGeometry.size()) syncTextureSize();
}

void TimeAxisItem::setStore(DataStore *s)
{
    if (m_store == s) return;
    if (m_store) disconnect(m_store, nullptr, this, nullptr);
    m_store = s;
    if (m_store) connect(m_store, &DataStore::frameReady, this, [this]{
        recomputeWidth();
        if (isVisible()) { syncTextureSize(); update(); }
    });
    emit storeChanged();
    recomputeWidth();
    update();
}

void TimeAxisItem::setWindowSeconds(int v)
{
    if (m_window == v) return;
    m_window = v;
    recomputeWidth();
    emit paramsChanged();
    update();
}

void TimeAxisItem::setDepthSensorId(int v)
{
    if (m_depth == v) return;
    m_depth = v;
    recomputeWidth();
    emit paramsChanged();
    update();
}

void TimeAxisItem::setToolDepthSensorId(int v)
{
    if (m_toolDepth == v) return;
    m_toolDepth = v;
    recomputeWidth();
    emit paramsChanged();
    update();
}

void TimeAxisItem::setOffsetSeconds(int v)
{
    if (m_offset == v) return;
    m_offset = v;
    emit paramsChanged();
    update();
}

void TimeAxisItem::setFontPx(int px)
{
    if (px < 9) px = 9;
    if (px == m_fontPx) return;
    m_fontPx = px;
    recomputeWidth();
    emit paramsChanged();
    update();
}

void TimeAxisItem::paint(QPainter *p)
{
    const QRectF r = boundingRect();
    p->fillRect(r, QColor("#F2F5F7"));
    p->setRenderHint(QPainter::TextAntialiasing);
    p->setBrush(Qt::NoBrush);

    const double padT = 4, padB = 10;
    const double ph = r.height() - padT - padB;
    if (!std::isfinite(ph) || ph <= 10 || !m_store) return;

    double tEndBase = m_store->lastEpoch();
    if (!(tEndBase > 0)) {
        const QVector<Sample> d0 = m_store->snapshot(uint16_t(m_depth));
        if (!d0.isEmpty()) tEndBase = d0.last().t;
        const QVector<Sample> t0 = m_store->snapshot(uint16_t(m_toolDepth));
        if (!t0.isEmpty() && t0.last().t > tEndBase) tEndBase = t0.last().t;
    }
    const double tEnd = tEndBase - m_offset;
        if (tEnd <= 0) return;
    const double tStart = tEnd - m_window;
    auto Y = [&](double t) { return padT + (t - tStart) / m_window * ph; };

    const double pad = 1;

    const QVector<Sample> ds = m_store->snapshot(uint16_t(m_depth));
    const QVector<Sample> ts = m_store->snapshot(uint16_t(m_toolDepth));
    const bool twoLines = !ts.isEmpty();

    const double depthColW = (twoLines ? std::max(m_depthTextW, m_toolTextW) : m_depthTextW) + 2;
    const double colSplit = pad + depthColW + 2;

    p->setPen(QPen(QColor("#D5DDE3"), 1));
    p->drawLine(QPointF(colSplit, padT), QPointF(colSplit, padT + ph));

    QFont f = p->font();
    f.setPixelSize(m_fontPx);
    p->setFont(f);

    QFont f2 = f;
    if (twoLines) f2.setPixelSize(qMax(7, m_fontPx - 1));
    const double lh2 = QFontMetrics(f2).height();
    const double minPitch = twoLines ? qMax(33.0, 2.0 * lh2 + 6.0) : 33.0;

    auto nearest = [](const QVector<Sample> &v, double t) -> double {
        if (v.isEmpty()) return 0.0;
        auto it = std::lower_bound(v.begin(), v.end(), t,
                                   [](const Sample &s, double tt){ return s.t < tt; });
        if (it == v.end()) return v.last().v;
        if (it == v.begin()) return it->v;
        auto prev = it - 1;
        return (t - prev->t < it->t - t) ? prev->v : it->v;
    };

    const double step = tickStep(m_window, ph, minPitch);
    QDate prevDate;
    bool havePrev = false;
    const double timeX = colSplit + 2;
    const double timeW = r.width() - pad - timeX;

    for (double t = std::ceil(tStart / step) * step; t <= tEnd + 1e-9; t += step) {
        const double y = Y(t);
        if (!std::isfinite(y)) continue;

        const QDateTime dt = utcAt(t);
        const bool newDay = havePrev && dt.date() != prevDate;

        p->setPen(QPen(newDay ? QColor("#8FA1B0") : QColor("#C4CED6"),
                       newDay ? 1.0 : 0.6,
                       newDay ? Qt::SolidLine : Qt::DashLine));
        p->drawLine(QPointF(colSplit, y), QPointF(r.width() - pad, y));

        const QString tstr = step < 60 ? dt.toString("hh:mm:ss") : dt.toString("hh:mm");
        p->setPen(QColor("#33424F"));
        p->drawText(QRectF(timeX, y - 7, timeW, 14), Qt::AlignRight | Qt::AlignVCenter, tstr);

        if (newDay) {
            const QString dstr = dt.toString("dd.MM.yy");
            QFont fd = f;
            fd.setBold(true);
            int px = m_fontPx;
            const double adv = QFontMetrics(fd).horizontalAdvance(dstr);
            if (adv > timeW && adv > 0) {
                px = qMax(6, int(m_fontPx * timeW / adv));
                fd.setPixelSize(px);
            }
            p->setFont(fd);
            p->setPen(QColor("#12314E"));
            p->drawText(QRectF(timeX, y + 5, timeW, 12), Qt::AlignRight | Qt::AlignVCenter, dstr);
            p->setFont(f);
        }
        prevDate = dt.date();
        havePrev = true;

        const double dv = nearest(ds, t);
        if (twoLines) {
            const double tv = nearest(ts, t);
            p->setFont(f2);

            // Р С‘Р Р…РЎРѓРЎвЂљРЎР‚РЎС“Р СР ВµР Р…РЎвЂљ РІР‚вЂќ Р Р†Р ВµРЎР‚РЎвЂ¦Р Р…РЎРЏРЎРЏ РЎРѓРЎвЂљРЎР‚Р С•Р С”Р В°
            p->setPen(QPen(QColor("#3E6B8C"), 1));
            p->drawText(QRectF(pad, y - lh2 - 1, depthColW, lh2),
                        Qt::AlignRight | Qt::AlignVCenter, QString::number(tv, 'f', 1));

            // Р В·Р В°Р В±Р С•Р в„– РІР‚вЂќ Р Р…Р С‘Р В¶Р Р…РЎРЏРЎРЏ РЎРѓРЎвЂљРЎР‚Р С•Р С”Р В°
            p->setPen(QPen(QColor("#12314E"), 1));
            p->drawText(QRectF(pad, y + 1, depthColW, lh2),
                        Qt::AlignRight | Qt::AlignVCenter, QString::number(dv, 'f', 1));

            p->setFont(f);
        } else {
            p->setPen(QPen(QColor("#12314E"), 1));
            p->drawText(QRectF(pad, y - 6, depthColW, 12),
                        Qt::AlignRight | Qt::AlignVCenter, QString::number(dv, 'f', 1));
        }

        p->setPen(QPen(QColor("#8FA1B0"), 1));
        p->drawLine(QPointF(colSplit - 2, y), QPointF(colSplit, y));
    }
}