#include "GeoChartItem.h"
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QQuickWindow>
#include <QVector>
#include <QPair>
#include <QFont>
#include <QDate>
#include <QDateTime>
#include <QTimeZone>
#include <limits>
#include <cmath>

static qreal dprOf(const QQuickItem *item)
{
    const QQuickWindow *w = item ? item->window() : nullptr;
    const qreal d = w ? w->effectiveDevicePixelRatio() : 1.0;
    return d > 0 ? d : 1.0;
}

static QString fmtVal(double v, bool log4 = false)
{
    if (log4) return QString::number(v, 'f', 4);
    QString t = QString::number(v, 'f', 3);
    if (t.contains('.')) {
        while (t.endsWith('0')) t.chop(1);
        if (t.endsWith('.')) t.chop(1);
    }
    return t;
}

static double minuteStep(double windowSec, double pixHeight)
{
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

void GeoChartItem::setLogScale(bool v)
{
    if (m_log == v) return;
    m_log = v;
    emit paramsChanged();
    update();
}

void GeoChartItem::setSharedMin(double v)
{
    if (m_sharedMin == v) return;
    m_sharedMin = v;
    emit paramsChanged();
    update();
}

void GeoChartItem::setSharedMax(double v)
{
    if (m_sharedMax == v) return;
    m_sharedMax = v;
    emit paramsChanged();
    update();
}

void GeoChartItem::setHighlightComment(int id)
{
    if (m_hlComment == id) return;
    m_hlComment = id;
    emit paramsChanged();
    update();
}

void GeoChartItem::setComments(const QVariantList &v)
{
    m_comments = v;
    emit paramsChanged();
    update();
}

double GeoChartItem::xOf(double v, double lo, double hi, double padL, double pw) const
{
    if (m_log) {
        if (!(lo > 0)) lo = 1e-4;
        if (!(hi > lo)) hi = lo * 10;
        const double l0 = std::log10(lo);
        const double l1 = std::log10(hi);
        const double lv = std::log10(qBound(lo, v, hi));
        return padL + (lv - l0) / (l1 - l0) * pw;
    }
    return padL + (v - lo) / (hi - lo) * pw;
}

void GeoChartItem::paint(QPainter *p)
{
    const QRectF r = boundingRect();
    p->fillRect(r, Qt::white);
    p->setRenderHint(QPainter::Antialiasing);
    p->setBrush(Qt::NoBrush);

    const double padL = 4, padR = 4, padT = 4, padB = 10;
    const double pw = r.width() - padL - padR;
    const double ph = r.height() - padT - padB;
    if (!std::isfinite(pw) || !std::isfinite(ph) || pw <= 4 || ph <= 4) return;

    const double tEnd = m_store ? m_store->lastEpoch() - m_offset : 0;
    const double tStart = tEnd - m_window;
    auto Y = [&](double t){ return padT + (t - tStart) / m_window * ph; };

    QPen grid(QColor("#BBBBBB"));
    grid.setStyle(Qt::DashLine);
    grid.setWidthF(0.6);
    p->setPen(grid);
    if (m_log) {
        const double l0 = std::log10(qMax(m_sharedMin, 1e-12));
        const double l1 = std::log10(qMax(m_sharedMax, m_sharedMin * 10));
        for (int d = int(std::ceil(l0)); d <= int(std::floor(l1)); ++d) {
            const double x = padL + (d - l0) / (l1 - l0) * pw;
            if (!std::isfinite(x)) continue;
            p->drawLine(QPointF(x, padT), QPointF(x, padT + ph));
        }
    } else {
        for (int i = 0; i <= 8; ++i) {
            const double x = padL + pw * i / 8.0;
            p->drawLine(QPointF(x, padT), QPointF(x, padT + ph));
        }
    }
    if (tEnd > 0) {
        const double step = minuteStep(m_window, ph);
        QDate prevDay;
        bool havePrevDay = false;
        for (double t = qCeil(tStart / step) * step; t <= tEnd + 1e-9; t += step) {
            const double y = Y(t);
            if (!std::isfinite(y)) continue;
            const QDate day = QDateTime::fromSecsSinceEpoch(qint64(t), QTimeZone::UTC).date();
            const bool newDay = havePrevDay && day != prevDay;
            p->setPen(QPen(newDay ? QColor("#8FA1B0") : QColor("#BBBBBB"),
                           newDay ? 1.0 : 0.6,
                           newDay ? Qt::SolidLine : Qt::DashLine));
            p->drawLine(QPointF(padL, y), QPointF(padL + pw, y));
            prevDay = day;
            havePrevDay = true;
        }
        p->setPen(grid);
    }

    if (!m_store || tEnd <= 0) return;

    const int buckets = qMax(1, int(ph));
    const double inf = std::numeric_limits<double>::infinity();

    QVector<QPair<double, QColor>> curMarks;
    p->setClipRect(QRectF(padL, padT, pw, ph));
    for (const QVariant &tv : m_traces) {
        const QVariantMap m = tv.toMap();
        const int sensor = m.value("sensorId").toInt();
        double lo = m.value("min").toDouble();
        double hi = m.value("max").toDouble();
        if (!(hi > lo)) hi = lo + 1;
        if (m_log) { lo = m_sharedMin; hi = m_sharedMax; if (!(lo > 0)) lo = 1e-4; if (!(hi > lo)) hi = lo * 10; }
        const QColor color(m.value("color").toString());
        auto X = [&](double v){ return xOf(v, lo, hi, padL, pw); };

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
            if (!std::isfinite(v)) continue;
            if (!bHas[b]) { bHas[b] = true; bMin[b] = v; bMax[b] = v; }
            else { bMin[b] = qMin(bMin[b], v); bMax[b] = qMax(bMax[b], v); }
        }

        QPainterPath path;
        bool started = false;
        for (int b = 0; b < buckets; ++b) {
            if (!bHas[b]) continue;
            const double y = padT + (b + 0.5) * ph / buckets;
            const double xa = X(bMin[b]);
            const double xb = X(bMax[b]);
            if (!std::isfinite(xa) || !std::isfinite(xb)) continue;
            const double x1 = qBound(padL, xa, padL + pw);
            const double x2 = qBound(padL, xb, padL + pw);
            if (!started) { path.moveTo(x1, y); started = true; }
            else path.lineTo(x1, y);
            path.lineTo(x2, y);
        }

        QPen line(color);
        line.setWidthF(1.2);
        p->setPen(line);
        p->setBrush(Qt::NoBrush);
        p->drawPath(path);

        const double lx = X(m_store->latest(uint16_t(sensor)));
        curMarks.append(qMakePair(qBound(padL, std::isfinite(lx) ? lx : padL, padL + pw), color));
        p->setPen(grid);
    }
    p->setClipping(false);

    // треугольники текущего значения внизу колонки (цвет = цвет линии)
    for (const auto &cm : curMarks) {
        p->setPen(Qt::NoPen);
        p->setBrush(cm.second);
        QPolygonF tri;
        tri << QPointF(cm.first, padT + ph)
            << QPointF(cm.first - 5, padT + ph + 8)
            << QPointF(cm.first + 5, padT + ph + 8);
        p->drawPolygon(tri);
        p->setBrush(Qt::NoBrush);
    }

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
        if (m_log) { lo = m_sharedMin; hi = m_sharedMax; if (!(lo > 0)) lo = 1e-4; if (!(hi > lo)) hi = lo * 10; }
        const double y = Y(mk.t);
        const double x = qBound(padL, xOf(mk.value, lo, hi, padL, pw), padL + pw);
        if (!std::isfinite(x) || !std::isfinite(y)) continue;
        if (y < padT || y > padT + ph) continue;

        p->setPen(QPen(mk.color, 1));
        p->setBrush(Qt::NoBrush);
        p->drawLine(QPointF(x, y), QPointF(x + 8, y - 8));
        p->drawEllipse(QPointF(x, y), 2.0, 2.0);

        const QString txt = fmtVal(mk.value, m_log);
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

    const double tEnd2 = m_store ? m_store->lastEpoch() - m_offset : 0;
    const double tStart2 = tEnd2 - m_window;
    QFont fc = p->font();
    fc.setPixelSize(9);
    p->setFont(fc);
    for (const QVariant &cv : m_comments) {
        const QVariantMap cm = cv.toMap();
        const double xFrac = qBound(0.0, cm.value("yFrac").toDouble(), 1.0);
        const double cx = padL + xFrac * pw;                       // фиксировано по ширине колонки
        const double cy = padT + (cm.value("t").toDouble() - tStart2) / m_window * ph;  // движется со шкалой времени
        if (!std::isfinite(cx) || !std::isfinite(cy)) continue;
        if (cy < padT - 2 || cy > padT + ph + 2) continue;
        const QString txt = cm.value("text").toString();
        const bool hl = (cm.value("id").toInt() == m_hlComment);
        p->setPen(QPen(hl ? QColor("#C22B2B") : QColor("#E8A013"), hl ? 2 : 1));
        p->setBrush(hl ? QColor("#FFE9C9") : QColor("#FFF8E6"));
        p->drawEllipse(QPointF(cx, cy), 2.5, 2.5);
        const QStringList lines = txt.split('\n');
        int maxLen = 1;
        for (const QString &ln : lines) maxLen = qMax(maxLen, ln.length());
        const double w = 6 + maxLen * 5.4 + 6;
        const double h = lines.size() * 12 + 6;
        double bx = cx + 6;
        if (bx + w > padL + pw) bx = cx - 6 - w;      // не выходим за правый край
        bx = qBound(padL, bx, qMax(padL, padL + pw - w));
        QRectF box(bx, cy - h / 2, w, h);
        p->drawRect(box);
        p->setPen(QColor("#33424F"));
        p->setBrush(Qt::NoBrush);
        p->drawText(box.adjusted(3, 2, -3, -2), Qt::AlignLeft | Qt::AlignTop, txt);
    }
}

void GeoChartItem::addMarkerAt(double x, double y, int preferredSensor)
{
    Q_UNUSED(preferredSensor);
    if (!m_store) return;
    const QRectF r = boundingRect();
    const double padL = 4, padR = 4, padT = 4, padB = 10;
    const double pw = r.width() - padL - padR;
    const double ph = r.height() - padT - padB;
    if (!std::isfinite(pw) || !std::isfinite(ph) || pw <= 4 || ph <= 4) return;
    const double tEnd = m_store->lastEpoch() - m_offset;
    const double tStart = tEnd - m_window;
    const double t = tStart + (y - padT) / ph * m_window;
    if (t < tStart || t > tEnd) return;

    int bestSensor = -1;
    double bestDist = 1e18, bestVal = 0, bestT = 0;
    QColor bestColor;
    for (const QVariant &tv : m_traces) {
        const QVariantMap m = tv.toMap();
        const int sensor = m.value("sensorId").toInt();
        double lo = m.value("min").toDouble();
        double hi = m.value("max").toDouble();
        if (!(hi > lo)) hi = lo + 1;
        if (m_log) { lo = m_sharedMin; hi = m_sharedMax; if (!(lo > 0)) lo = 1e-4; if (!(hi > lo)) hi = lo * 10; }
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
        const double xv = xOf(v, lo, hi, padL, pw);
        const double d = qAbs(xv - x);
        if (d < bestDist) {
            bestDist = d; bestSensor = sensor; bestVal = v; bestT = smp[idx].t;
            bestColor = QColor(m.value("color").toString());
        }
    }
    if (bestSensor < 0) return;
    Marker mk;
    mk.t = t;
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
        if (m_log) { lo = m_sharedMin; hi = m_sharedMax; if (!(lo > 0)) lo = 1e-4; if (!(hi > lo)) hi = lo * 10; }
        const double yy = padT + (mk.t - tStart) / m_window * ph;
        const double xx = xOf(mk.value, lo, hi, padL, pw);
        const QString mtxt = fmtVal(mk.value, m_log);
        const double bw = 6 + mtxt.length() * 5.4 + 6;
        const bool inBox = (x >= xx + 5 && x <= xx + 8 + bw && y >= yy - 16 && y <= yy + 2);
        if ((qAbs(xx - x) < 20 && qAbs(yy - y) < 20) || inBox) {
            m_markers.removeAt(i);
            update();
            return true;
        }
    }
    return false;
}

int GeoChartItem::commentAt(double x, double y) const
{
    const QRectF r = boundingRect();
    const double padL = 4, padT = 4, padB = 10;
    const double pw = r.width() - padL - 4;
    const double ph = r.height() - padT - padB;
    if (ph <= 4 || pw <= 4 || !m_store) return -1;
    const double tEnd = m_store->lastEpoch() - m_offset;
    const double tStart = tEnd - m_window;
    for (const QVariant &cv : m_comments) {
        const QVariantMap m = cv.toMap();
        const double xFrac = qBound(0.0, m.value("yFrac").toDouble(), 1.0);
        const double cx = padL + xFrac * pw;
        const double cy = padT + (m.value("t").toDouble() - tStart) / m_window * ph;
        const QString txt = m.value("text").toString();
        const QStringList lines = txt.split('\n');
        int maxLen = 1;
        for (const QString &ln : lines) maxLen = qMax(maxLen, ln.length());
        const double w = 6 + maxLen * 5.4 + 6;
        const double h = lines.size() * 12 + 6;
        double bx = cx + 6;
        if (bx + w > padL + pw) bx = cx - 6 - w;
        bx = qBound(padL, bx, qMax(padL, padL + pw - w));
        if (x >= bx - 4 && x <= bx + w + 4 && y >= cy - h / 2 - 4 && y <= cy + h / 2 + 4)
            return m.value("id").toInt();
        if (qAbs(x - cx) < 10 && qAbs(y - cy) < 10)
            return m.value("id").toInt();
    }
    return -1;
}

QVariantList GeoChartItem::commentCoords(double x, double y) const
{
    QVariantList out;
    const QRectF r = boundingRect();
    const double padL = 4, padR = 4, padT = 4, padB = 10;
    const double pw = r.width() - padL - padR;
    const double ph = r.height() - padT - padB;
    if (pw <= 4 || ph <= 4 || !m_store) { out.append(0.0); out.append(0.0); return out; }
    const double tEnd = m_store->lastEpoch() - m_offset;
    const double tStart = tEnd - m_window;
    // время (вертикальная шкала) из Y-координаты клика; доля ширины колонки — из X
    const double t = tStart + qBound(0.0, (y - padT) / ph, 1.0) * m_window;
    out.append(t);
    out.append(qBound(0.0, (x - padL) / pw, 1.0));
    return out;
}
