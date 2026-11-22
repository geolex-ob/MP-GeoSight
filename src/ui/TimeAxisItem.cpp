#include "TimeAxisItem.h"
#include <QPainter>
#include <QQuickWindow>
#include <QDateTime>
#include <QTimeZone>
#include <QDate>
#include <QFont>
#include <QFontMetrics>
#include <cmath>
#include <algorithm>

static qreal dprOf(const QQuickItem *item)
{
    const QQuickWindow *w = item ? item->window() : nullptr;
    const qreal d = w ? w->effectiveDevicePixelRatio() : 1.0;
    return d > 0 ? d : 1.0;
}

static double tickStep(double windowSec, double pixHeight)
{
    static const double steps[] = {1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 900,
                                   1800, 3600, 7200, 10800, 14400, 21600, 43200, 86400};
    const double minPitch = 33.0;
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

// ширина = только необходимые данные (забой до 0.1 + время), без минимального порога
void TimeAxisItem::recomputeWidth()
{
    QFont f;
    f.setPixelSize(m_fontPx);
    const QFontMetrics fm(f);
    const double dv = m_store ? m_store->latest(uint16_t(m_depth)) : 0.0;
    m_depthTextW = double(fm.horizontalAdvance(QString::number(dv, 'f', 1)));
    double ph = height() - 4 - 10;
    if (!(ph > 10)) ph = 100;
    const double step = tickStep(m_window, ph);
    m_timeTextW = double(fm.horizontalAdvance(step < 60 ? "00:00:00" : "00:00"));
    const int w = int(std::ceil(2 + m_depthTextW + m_timeTextW + 4));
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
    const double tEnd = m_store->lastEpoch() - m_offset;
    if (tEnd <= 0) return;
    const double tStart = tEnd - m_window;
    auto Y = [&](double t){ return padT + (t - tStart) / m_window * ph; };

    const double pad = 1;
    const double colSplit = pad + m_depthTextW + 2;
    p->setPen(QPen(QColor("#D5DDE3"), 1));
    p->drawLine(QPointF(colSplit, padT), QPointF(colSplit, padT + ph));

    QFont f = p->font();
    f.setPixelSize(m_fontPx);
    p->setFont(f);
    const QFontMetrics fm(f);

    const QVector<Sample> ds = m_store->snapshot(uint16_t(m_depth));
    auto depthAt = [&](double t)->double {
        if (ds.isEmpty()) return 0.0;
        auto it = std::lower_bound(ds.begin(), ds.end(), t,
                                   [](const Sample &s, double tt){ return s.t < tt; });
        if (it == ds.end()) return ds.last().v;
        if (it == ds.begin()) return it->v;
        auto prev = it - 1;
        return (t - prev->t < it->t - t) ? prev->v : it->v;
    };

    const double step = tickStep(m_window, ph);
    QDate prevDate;
    bool havePrev = false;
    const double timeX = colSplit + 2;
    const double timeW = r.width() - pad - timeX;

    for (double t = std::ceil(tStart / step) * step; t <= tEnd + 1e-9; t += step) {
        const double y = Y(t);
        if (!std::isfinite(y)) continue;
        const QDateTime dt = QDateTime::fromSecsSinceEpoch(qint64(t), QTimeZone::UTC);
        const bool newDay = havePrev && dt.date() != prevDate;

        p->setPen(QPen(newDay ? QColor("#8FA1B0") : QColor("#C4CED6"),
                       newDay ? 1.0 : 0.6,
                       newDay ? Qt::SolidLine : Qt::DashLine));
        p->drawLine(QPointF(colSplit, y), QPointF(r.width() - pad, y));

        const QString ts = step < 60 ? dt.toString("hh:mm:ss") : dt.toString("hh:mm");
        p->setPen(QColor("#33424F"));
        p->drawText(QRectF(timeX, y - 7, timeW, 14), Qt::AlignRight | Qt::AlignVCenter, ts);

        // дата новых суток: если не помещается — уменьшаем шрифт до помещения
        if (newDay) {
            const QString dstr = dt.toString("dd.MM.yy");
            QFont fd = f;
            fd.setBold(true);
            int px = m_fontPx;
            double adv = QFontMetrics(fd).horizontalAdvance(dstr);
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

        // забой с точностью до 0.1
        const double dv = depthAt(t);
        p->setPen(QPen(QColor("#12314E"), 1));
        p->drawText(QRectF(pad, y - 6, m_depthTextW, 12), Qt::AlignRight | Qt::AlignVCenter,
                    QString::number(dv, 'f', 1));
        p->setPen(QPen(QColor("#8FA1B0"), 1));
        p->drawLine(QPointF(colSplit - 2, y), QPointF(colSplit, y));
    }
}
