#include "TimeAxisItem.h"
#include <QPainter>
#include <QQuickWindow>
#include <QDateTime>
#include <QTimeZone>
#include <QVector>
#include <algorithm>
#include <cmath>

static qreal dprOf(const QQuickItem *item)
{
    const QQuickWindow *w = item ? item->window() : nullptr;
    const qreal d = w ? w->effectiveDevicePixelRatio() : 1.0;
    return d > 0 ? d : 1.0;
}

// Единый шаг отметок для шкалы и сетки графиков.
// Высота надписи ~11 px; шаг подбирается так, чтобы между соседними
// надписями оставалось >= 22 px свободного места (две высоты надписи).
static double tickStep(double windowSec, double pixHeight)
{
    static const double steps[] = {1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 900,
                                   1800, 3600, 7200, 10800, 14400, 21600, 43200, 86400};
    const double minPitch = 33.0;   // 11 px надпись + 22 px зазор
    for (double s : steps)
        if (pixHeight * s / windowSec >= minPitch) return s;
    return 86400.0;
}

TimeAxisItem::TimeAxisItem(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    setRenderTarget(QQuickPaintedItem::Image);
    setOpaquePainting(true);
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
    if (newGeometry.size() != oldGeometry.size()) syncTextureSize();
}

void TimeAxisItem::setStore(DataStore *s)
{
    if (m_store == s) return;
    if (m_store) disconnect(m_store, nullptr, this, nullptr);
    m_store = s;
    if (m_store) connect(m_store, &DataStore::frameReady, this, [this]{
        if (isVisible()) { syncTextureSize(); update(); }
    });
    emit storeChanged();
    update();
}

void TimeAxisItem::setWindowSeconds(int v)
{
    if (m_window == v) return;
    m_window = v;
    emit paramsChanged();
    update();
}

void TimeAxisItem::setDepthSensorId(int v)
{
    if (m_depth == v) return;
    m_depth = v;
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

void TimeAxisItem::paint(QPainter *p)
{
    const QRectF r = boundingRect();
    p->fillRect(r, QColor("#F2F5F7"));
    p->setRenderHint(QPainter::TextAntialiasing);

    const double padT = 4, padB = 18;
    const double ph = r.height() - padT - padB;
    const double colSplit = 40.0;          // граница колонок «глубина | время»
    if (!std::isfinite(ph) || ph <= 10 || !m_store) return;
    const double tEnd = m_store->lastEpoch() - m_offset;
    if (tEnd <= 0) return;
    const double tStart = tEnd - m_window;
    auto Y = [&](double t){ return padT + (t - tStart) / m_window * ph; };

    // разделитель колонок и рамка
    p->setPen(QPen(QColor("#D5DDE3"), 1));
    p->drawLine(QPointF(colSplit, padT), QPointF(colSplit, padT + ph));
    p->drawLine(QPointF(r.width() - 1, padT), QPointF(r.width() - 1, padT + ph));

    QFont f = p->font();
    f.setPixelSize(9);
    p->setFont(f);

    const QVector<Sample> ds = m_store->snapshot(uint16_t(m_depth));
    auto depthAt = [&](double t) -> double {
        if (ds.isEmpty()) return 0.0;
        auto it = std::lower_bound(ds.begin(), ds.end(), t,
                                   [](const Sample &s, double tt){ return s.t < tt; });
        if (it == ds.end()) return ds.last().v;
        if (it == ds.begin()) return it->v;
        auto prev = it - 1;
        return (t - prev->t < it->t - t) ? prev->v : it->v;
    };

    const double step = tickStep(m_window, ph);
    for (double t = std::ceil(tStart / step) * step; t <= tEnd + 1e-9; t += step) {
        const double y = Y(t);
        if (!std::isfinite(y)) continue;

        // горизонтальная риска только в колонке времени (не пересекает надписи глубины)
        p->setPen(QPen(QColor("#C4CED6"), 0.6, Qt::DashLine));
        p->drawLine(QPointF(colSplit + 1, y), QPointF(r.width() - 2, y));

        // время — левый край своей колонки, строго по центру строки
        const QDateTime dt = QDateTime::fromSecsSinceEpoch(qint64(t), QTimeZone::UTC);
        const QString ts = step < 60 ? dt.toString("hh:mm:ss") : dt.toString("hh:mm");
        p->setPen(QColor("#33424F"));
        p->drawText(QRectF(colSplit + 4, y - 6, r.width() - colSplit - 6, 12),
                    Qt::AlignLeft | Qt::AlignVCenter, ts);

        // глубина забоя — у КАЖДОЙ метки времени, в своей колонке слева
        const double dv = depthAt(t);
        const QString dstr = QString::number(dv, 'f', dv >= 1000 ? 0 : (dv >= 100 ? 1 : 2));
        p->setPen(QPen(QColor("#12314E"), 1));
        p->drawText(QRectF(2, y - 6, colSplit - 6, 12),
                    Qt::AlignRight | Qt::AlignVCenter, dstr);
        p->setPen(QPen(QColor("#8FA1B0"), 1));
        p->drawLine(QPointF(colSplit - 3, y), QPointF(colSplit, y));
    }

    // нижняя строка: текущие глубина и время сервера
    const double yb = padT + ph + 2;
    p->setPen(QPen(QColor("#B9C4CE"), 1));
    p->drawLine(QPointF(2, yb), QPointF(r.width() - 2, yb));
    QFont fb = f;
    fb.setBold(true);
    p->setFont(fb);
    const double dNow = m_store->latest(uint16_t(m_depth));
    p->setPen(QColor("#12314E"));
    p->drawText(QRectF(2, yb + 1, colSplit - 6, 14), Qt::AlignRight | Qt::AlignVCenter,
                QString::number(dNow, 'f', dNow >= 100 ? 1 : 2));
    p->setPen(m_offset > 0 ? QColor("#C22B2B") : QColor("#1D8A4E"));
    p->drawText(QRectF(colSplit + 4, yb + 1, r.width() - colSplit - 6, 14),
                Qt::AlignLeft | Qt::AlignVCenter,
                QDateTime::fromSecsSinceEpoch(qint64(tEnd), QTimeZone::UTC).toString("hh:mm:ss"));
}
