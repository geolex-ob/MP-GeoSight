#include "GeoChartItem.h"
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QQuickWindow>
#include <QVector>
#include <limits>

static qreal dprOf(const QQuickItem *item)
{
    const QQuickWindow *w = item ? item->window() : nullptr;
    const qreal d = w ? w->effectiveDevicePixelRatio() : 1.0;
    return d > 0 ? d : 1.0;
}

static QString fmtVal(double v)
{
    QString t = QString::number(v, 'f', 3);
    if (t.contains('.')) {
        while (t.endsWith('0')) t.chop(1);
        if (t.endsWith('.')) t.chop(1);
    }
    return t;
}

static double minuteStep(double windowSec, double pixHeight)
{
    // идентично TimeAxisItem::tickStep: зазор между линиями >= двух высот надписи
    static const double steps[] = {1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 900,
                                   1800, 3600, 7200, 10800, 14400, 21600, 43200, 86400};
    const double minPitch = 33.0;
    for (double st : steps)
        if (pixHeight * st / windowSec >= minPitch) return st;
    return 86400.0;
}

GeoChartItem::GeoChartItem(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    setRenderTarget(QQuickPaintedItem::Image);
    setOpaquePainting(true);
}

void GeoChartItem::syncTextureSize()
{
    const QSize ts(qRound(width()), qRound(height()));
    if (ts.isValid() && !ts.isNull() && ts != textureSize()) {
        setTextureSize(ts);
        update();
    }
}

void GeoChartItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) syncTextureSize();
}

void GeoChartItem::setStore(DataStore *s)
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

void GeoChartItem::setTraces(const QVariantList &v)
{
    m_traces = v;
    emit paramsChanged();
    update();
}

void GeoChartItem::setWindowSeconds(int v)
{
    if (m_window == v) return;
    m_window = v;
    emit paramsChanged();
    update();
}

void GeoChartItem::setOffsetSeconds(int v)
{
    if (m_offset == v) return;
    m_offset = v;
    emit paramsChanged();
    update();
}

void GeoChartItem::paint(QPainter *p)
{
    const QRectF r = boundingRect();
    p->fillRect(r, Qt::white);
    p->setRenderHint(QPainter::Antialiasing);
    p->setBrush(Qt::NoBrush);   // глобальная гарантия: ничего не заливается

    const double padL = 4, padR = 4, padT = 4, padB = 10;
    const double pw = r.width() - padL - padR;
    const double ph = r.height() - padT - padB;
    if (pw <= 4 || ph <= 4) return;

    const double tEnd = m_store ? m_store->lastEpoch() - m_offset : 0;
    const double tStart = tEnd - m_window;
    auto Y = [&](double t){ return padT + (t - tStart) / m_window * ph; };

    QPen grid(QColor("#BBBBBB"));
    grid.setStyle(Qt::DashLine);
    grid.setWidthF(0.6);
    p->setPen(grid);
    for (int i = 0; i <= 8; ++i) {
        const double x = padL + pw * i / 8.0;
        p->drawLine(QPointF(x, padT), QPointF(x, padT + ph));
    }
    if (tEnd > 0) {
        const double step = minuteStep(m_window, ph);
        for (double t = qCeil(tStart / step) * step; t <= tEnd; t += step) {
            const double y = Y(t);
            p->drawLine(QPointF(padL, y), QPointF(padL + pw, y));
        }
    }

    if (!m_store || tEnd <= 0) return;

    const int buckets = qMax(1, int(ph));
    const double inf = std::numeric_limits<double>::infinity();

    p->setClipRect(QRectF(padL, padT, pw, ph));
    for (const QVariant &tv : m_traces) {
        const QVariantMap m = tv.toMap();
        const int sensor = m.value("sensorId").toInt();
        double lo = m.value("min").toDouble();
        double hi = m.value("max").toDouble();
        if (!(hi > lo)) hi = lo + 1;
        const QColor color(m.value("color").toString());
        auto X = [&](double v){ return padL + (v - lo) / (hi - lo) * pw; };

        const QVector<Sample> s = m_store->snapshot(uint16_t(sensor));
        if (s.size() < 2) continue;
        int first = s.size() - 1;
        while (first > 0 && s[first - 1].t >= tStart) --first;

        QVector<double> bMin(buckets, inf), bMax(buckets, -inf);
        QVector<bool> bHas(buckets, false);
        for (int i = first; i < s.size(); ++i) {
            int b = int((s[i].t - tStart) / m_window * buckets);
            b = qBound(0, b, buckets - 1);
            const double v = s[i].v;
            if (!bHas[b]) { bHas[b] = true; bMin[b] = v; bMax[b] = v; }
            else { bMin[b] = qMin(bMin[b], v); bMax[b] = qMax(bMax[b], v); }
        }

        QPainterPath path;
        bool started = false;
        for (int b = 0; b < buckets; ++b) {
            if (!bHas[b]) continue;
            const double y = padT + (b + 0.5) * ph / buckets;
            const double x1 = qBound(padL, X(bMin[b]), padL + pw);
            const double x2 = qBound(padL, X(bMax[b]), padL + pw);
            if (!started) { path.moveTo(x1, y); started = true; }
            else path.lineTo(x1, y);
            path.lineTo(x2, y);
        }

        QPen line(color);
        line.setWidthF(1.2);
        p->setPen(line);
        p->setBrush(Qt::NoBrush);   // кривая ТОЛЬКО обводится
        p->drawPath(path);

        // маркер текущего значения внизу
        const double cx = qBound(padL, X(m_store->latest(uint16_t(sensor))), padL + pw);
        p->setPen(Qt::NoPen);
        p->setBrush(color);
        QPolygonF tri;
        tri << QPointF(cx, padT + ph) << QPointF(cx - 4, padT + ph + 7)
            << QPointF(cx + 4, padT + ph + 7);
        p->drawPolygon(tri);
        p->setBrush(Qt::NoBrush);   // сброс сразу после маркера
        p->setPen(grid);
    }
    p->setClipping(false);

    // метки значений (двойной клик)
    for (const Marker &mk : m_markers) {
        double lo = 0, hi = 100;
        bool found = false;
        for (const QVariant &tv : m_traces) {
            const QVariantMap mm = tv.toMap();
            if (mm.value("sensorId").toInt() != mk.sensor) continue;
            lo = mm.value("min").toDouble();
            hi = mm.value("max").toDouble();
            if (!(hi > lo)) hi = lo + 1;
            found = true;
            break;
        }
        if (!found) continue;
        const double y = Y(mk.t);
        const double x = qBound(padL, padL + (mk.value - lo) / (hi - lo) * pw, padL + pw);
        if (y < padT || y > padT + ph) continue;

        p->setPen(QPen(mk.color, 1));
        p->setBrush(Qt::NoBrush);
        p->drawLine(QPointF(x, y), QPointF(x + 8, y - 8));
        p->drawEllipse(QPointF(x, y), 2.0, 2.0);

        const QString txt = fmtVal(mk.value);
        QFont f = p->font();
        f.setPixelSize(9);
        p->setFont(f);
        QRectF br = p->boundingRect(QRectF(x + 8, y - 15, 300, 14), Qt::AlignLeft | Qt::AlignVCenter, txt);
        br.adjust(-3, -1, 3, 1);
        p->setBrush(Qt::white);
        p->drawRect(br);
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(mk.color, 1));
        p->drawRect(br);
        p->setPen(Qt::black);
        p->drawText(br, Qt::AlignCenter, txt);
    }
}

void GeoChartItem::addMarkerAt(double x, double y, int preferredSensor)
{
    if (!m_store) return;
    const QRectF r = boundingRect();
    const double padL = 4, padR = 4, padT = 4, padB = 10;
    const double pw = r.width() - padL - padR;
    const double ph = r.height() - padT - padB;
    if (pw <= 4 || ph <= 4) return;
    const double tEnd = m_store->lastEpoch() - m_offset;
    const double tStart = tEnd - m_window;
    const double t = tStart + (y - padT) / ph * m_window;
    if (t < tStart || t > tEnd) return;

    Q_UNUSED(preferredSensor);   // метка всегда на кривой под курсором

    int bestSensor = -1;
    double bestDist = 1e18, bestVal = 0, bestT = 0;
    QColor bestColor;
    for (const QVariant &tv : m_traces) {
        const QVariantMap m = tv.toMap();
        const int sensor = m.value("sensorId").toInt();
        double lo = m.value("min").toDouble();
        double hi = m.value("max").toDouble();
        if (!(hi > lo)) hi = lo + 1;
        const QVector<Sample> smp = m_store->snapshot(uint16_t(sensor));
        if (smp.isEmpty()) continue;
        int idx = -1;
        double bd = 1e18;
        for (int i = 0; i < smp.size(); ++i) {
            const double d = qAbs(smp[i].t - t);
            if (d < bd) { bd = d; idx = i; }
        }
        if (idx < 0) continue;
        const double v = smp[idx].v;
        const double xv = padL + (v - lo) / (hi - lo) * pw;
        const double d = qAbs(xv - x);
        if (d < bestDist) {
            bestDist = d; bestSensor = sensor; bestVal = v; bestT = smp[idx].t;
            bestColor = QColor(m.value("color").toString());
        }
    }
    if (bestSensor < 0) return;
    Marker mk;
    mk.t = t;             // по вертикали — точно точка клика
    mk.sensor = bestSensor; mk.value = bestVal; mk.color = bestColor;
    m_markers.append(mk);
    update();
}

bool GeoChartItem::removeMarkerAt(double x, double y)
{
    const QRectF r = boundingRect();
    const double padL = 4, padR = 4, padT = 4, padB = 10;
    const double pw = r.width() - padL - padR;
    const double ph = r.height() - padT - padB;
    if (pw <= 4 || ph <= 4 || !m_store) return false;
    const double tEnd = m_store->lastEpoch() - m_offset;
    const double tStart = tEnd - m_window;
    for (int i = m_markers.size() - 1; i >= 0; --i) {
        const Marker &mk = m_markers[i];
        double lo = 0, hi = 100;
        bool found = false;
        for (const QVariant &tv : m_traces) {
            const QVariantMap mm = tv.toMap();
            if (mm.value("sensorId").toInt() != mk.sensor) continue;
            lo = mm.value("min").toDouble();
            hi = mm.value("max").toDouble();
            if (!(hi > lo)) hi = lo + 1;
            found = true;
            break;
        }
        if (!found) { m_markers.removeAt(i); update(); continue; }
        const double yy = padT + (mk.t - tStart) / m_window * ph;
        const double xx = padL + (mk.value - lo) / (hi - lo) * pw;
        if (qAbs(xx - x) < 16 && qAbs(yy - y) < 16) {
            m_markers.removeAt(i);
            update();
            return true;
        }
    }
    return false;
}
