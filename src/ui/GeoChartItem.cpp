#include <QDebug>
#include "GeoChartItem.h"
#include "core/DataStore.h"

#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QQuickWindow>
#include <QVector>
#include <QPair>
#include <QFont>
#include <QFontMetrics>
#include <QDate>
#include <QDateTime>
#include <QTimeZone>
#include <QtGlobal>
#include <QtMath>
#include <limits>
#include <cmath>
#include <algorithm>

// Р’В«Р СњР С•Р В»РЎРЉР’В» Р В»Р С•Р С–-РЎв‚¬Р С”Р В°Р В»РЎвЂ№: Р СР ВµР Р…РЎРЉРЎв‚¬Р Вµ Р Р…Р С‘Р В¶Р Р…Р ВµР в„– Р С–РЎР‚Р В°Р Р…Р С‘РЎвЂ РЎвЂ№ (1e-4) РІвЂ вЂ™ РЎР‚Р С‘РЎРѓРЎС“Р ВµРЎвЂљРЎРѓРЎРЏ Р С—Р С• Р В»Р ВµР Р†Р С•Р СРЎС“ Р С”РЎР‚Р В°РЎР‹.
static constexpr double kLogZero = 1e-7;

static QDateTime utcAt(double t)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QDateTime::fromSecsSinceEpoch(qint64(t), QTimeZone::utc());
#else
    return QDateTime::fromSecsSinceEpoch(qint64(t), Qt::UTC);
#endif
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

static QFont labelFont(int px)
{
    QFont f;
    f.setPixelSize(qMax(7, px));
    return f;
}

static QSizeF labelSize(const QString &txt, int px)
{
    const QFontMetrics fm(labelFont(px));
    return QSizeF(fm.horizontalAdvance(txt) + 8.0, fm.height() + 4.0);
}

static QSizeF commentSize(const QString &txt, int px)
{
    const QFontMetrics fm(labelFont(px));
    const QStringList lines = txt.split('\n');
    int w = 0;
    for (const QString &ln : lines) w = qMax(w, fm.horizontalAdvance(ln));
    return QSizeF(w + 12.0, lines.size() * fm.height() + 8.0);
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

void GeoChartItem::setTraces(const QVariantList &v) { m_traces = v; emit paramsChanged(); update(); }
void GeoChartItem::setComments(const QVariantList &v) { m_comments = v; emit paramsChanged(); update(); }
void GeoChartItem::setWindowSeconds(int v) { if (m_window == v) return; m_window = v; emit paramsChanged(); update(); }
void GeoChartItem::setOffsetSeconds(int v) { if (m_offset == v) return; m_offset = v; emit paramsChanged(); update(); }
void GeoChartItem::setLogScale(bool v) { if (m_log == v) return; m_log = v; emit paramsChanged(); update(); }
void GeoChartItem::setSharedMin(double v) { if (m_sharedMin == v) return; m_sharedMin = v; emit paramsChanged(); update(); }
void GeoChartItem::setSharedMax(double v) { if (m_sharedMax == v) return; m_sharedMax = v; emit paramsChanged(); update(); }
void GeoChartItem::setHighlightComment(int id) { if (m_hlComment == id) return; m_hlComment = id; emit paramsChanged(); update(); }

void GeoChartItem::setFontPx(int px)
{
    if (px < 7) px = 7;
    if (px == m_fontPx) return;
    m_fontPx = px;
    emit paramsChanged();
    update();
}

double GeoChartItem::xOf(double v, double lo, double hi, double padL, double pw) const
{
    if (m_log) {
        if (!(lo > 0)) lo = 1e-4;
        if (!(hi > lo)) hi = lo * 10;
        if (!(v > 0)) v = kLogZero;
        const double l0 = std::log10(lo);
        const double l1 = std::log10(hi);
        const double lv = std::log10(qBound(lo, v, hi));
        return padL + (lv - l0) / (l1 - l0) * pw;
    }
    return padL + (v - lo) / (hi - lo) * pw;
}

void GeoChartItem::traceRange(int sensor, double &lo, double &hi) const
{
    lo = 0; hi = 100;
    for (const QVariant &tv : m_traces) {
        const QVariantMap mm = tv.toMap();
        if (mm.value("sensorId").toInt() != sensor) continue;
        lo = mm.value("min").toDouble();
        hi = mm.value("max").toDouble();
        if (!(hi > lo)) hi = lo + 1;
        break;
    }
    if (m_log) {
        lo = m_sharedMin; hi = m_sharedMax;
        if (!(lo > 0)) lo = 1e-4;
        if (!(hi > lo)) hi = lo * 10;
    }
}

QRectF GeoChartItem::commentBox(const QVariantMap &cm, double padL, double padT,
                                double pw, double ph, double tStart,
                                double &cx, double &cy) const
{
    const double xFrac = qBound(0.0, cm.value("yFrac").toDouble(), 1.0);
    cx = padL + xFrac * pw;
    cy = padT + (cm.value("t").toDouble() - tStart) / m_window * ph;
    const QSizeF sz = commentSize(cm.value("text").toString(), m_fontPx);
    double bx = cx + 6;
    if (bx + sz.width() > padL + pw) bx = cx - 6 - sz.width();
    bx = qBound(padL, bx, qMax(padL, padL + pw - sz.width()));
    return QRectF(bx, cy - sz.height() / 2, sz.width(), sz.height());
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

    double tEndBase = m_store ? m_store->lastEpoch() : 0.0;
    if (!(tEndBase > 0) && m_store) {
        for (const QVariant &tv : m_traces) {
            const int sid = tv.toMap().value("sensorId").toInt();
            const QVector<Sample> ss = m_store->snapshot(uint16_t(sid));
            if (!ss.isEmpty() && ss.last().t > tEndBase) tEndBase = ss.last().t;
        }
    }
    const double tEnd = tEndBase - m_offset;
        const double tStart = tEnd - m_window;

    QPen grid(QColor("#BBBBBB"));
    grid.setStyle(Qt::DashLine);
    grid.setWidthF(0.6);
    p->setPen(grid);
    if (m_log) {
        const double l0 = std::log10(qMax(m_sharedMin, 1e-12));
        const double l1 = std::log10(qMax(m_sharedMax, m_sharedMin * 10));
        for (int d = int(std::ceil(l0)); d <= int(std::floor(l1)); ++d) {
            const double x = padL + (d - l0) / (l1 - l0) * pw;
            if (std::isfinite(x)) p->drawLine(QPointF(x, padT), QPointF(x, padT + ph));
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
        for (double t = std::ceil(tStart / step) * step; t <= tEnd + 1e-9; t += step) {
            const double y = padT + (t - tStart) / m_window * ph;
            if (!std::isfinite(y)) continue;
            const QDate day = utcAt(t).date();
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
        double lo = 0, hi = 100;
        traceRange(sensor, lo, hi);
        const QColor color(m.value("color").toString());
        auto X = [&](double v) { return xOf(v, lo, hi, padL, pw); };

        const QVector<Sample> s = m_store->snapshot(uint16_t(sensor));
        if (s.isEmpty()) continue;

        int first = s.size() - 1;
        while (first > 0 && s[first - 1].t >= tStart) --first;

        QVector<double> bMin(buckets, inf), bMax(buckets, -inf);
        QVector<bool> bHas(buckets, false);
        for (int i = first; i < s.size(); ++i) {
            int b = int((s[i].t - tStart) / m_window * buckets);
            b = qBound(0, b, buckets - 1);
            const double v = s[i].v;
            if (!std::isfinite(v)) continue;
            if (m_log && !(v > 0)) continue;
            if (!bHas[b]) { bHas[b] = true; bMin[b] = v; bMax[b] = v; }
            else { bMin[b] = qMin(bMin[b], v); bMax[b] = qMax(bMax[b], v); }
        }

        // Р В»Р С•Р С–-РЎв‚¬Р С”Р В°Р В»Р В°: Р С—РЎС“РЎРѓРЎвЂљРЎвЂ№Р Вµ Р В±Р В°Р С”Р ВµРЎвЂљРЎвЂ№ = Р Р…Р С•Р В»РЎРЉ РІвЂ вЂ™ Р В»Р С‘Р Р…Р С‘РЎРЏ РЎвЂљРЎРЏР Р…Р ВµРЎвЂљРЎРѓРЎРЏ Р С•РЎвЂљ Р В»Р ВµР Р†Р С•Р С–Р С• Р С”РЎР‚Р В°РЎРЏ
        if (m_log) {
            for (int b = 0; b < buckets; ++b) {
                if (!bHas[b]) { bHas[b] = true; bMin[b] = kLogZero; bMax[b] = kLogZero; }
            }
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

        // Р СџР В Р С›Р вЂ™Р вЂўР В Р вЂўР СњР СњР С›Р вЂў Р С—Р ВµРЎР‚Р С•: Р В±Р ВµР В· cosmetic / FlatCap / MiterJoin
        QPen line(color);
        line.setWidthF(1.2);
        p->setPen(line);
        p->setBrush(Qt::NoBrush);
        p->drawPath(path);

        double latest = m_store->latest(uint16_t(sensor));
        if (m_log && (!std::isfinite(latest) || !(latest > 0))) latest = kLogZero;
        const double lx = X(latest);
        curMarks.append(qMakePair(qBound(padL, std::isfinite(lx) ? lx : padL, padL + pw), color));
        p->setPen(grid);
    }
    p->setClipping(false);

    // РЎвЂљРЎР‚Р ВµРЎС“Р С–Р С•Р В»РЎРЉР Р…Р С‘Р С”Р С‘ РЎвЂљР ВµР С”РЎС“РЎвЂ°Р ВµР С–Р С• Р В·Р Р…Р В°РЎвЂЎР ВµР Р…Р С‘РЎРЏ
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

    // Р СР ВµРЎвЂљР С”Р С‘: РЎв‚¬РЎР‚Р С‘РЎвЂћРЎвЂљ = РЎв‚¬РЎР‚Р С‘РЎвЂћРЎвЂљ Р В»Р ВµР Р†Р С•Р в„– РЎв‚¬Р С”Р В°Р В»РЎвЂ№
    p->setFont(labelFont(m_fontPx));
    for (const Marker &mk : m_markers) {
        double lo = 0, hi = 100;
        traceRange(mk.sensor, lo, hi);
        const double y = padT + (mk.t - tStart) / m_window * ph;
        const double x = qBound(padL, xOf(mk.value, lo, hi, padL, pw), padL + pw);
        if (!std::isfinite(x) || !std::isfinite(y)) continue;
        if (y < padT || y > padT + ph) continue;

        p->setPen(QPen(mk.color, 1));
        p->setBrush(Qt::NoBrush);
        p->drawLine(QPointF(x, y), QPointF(x + 8, y - 8));
        p->drawEllipse(QPointF(x, y), 2.0, 2.0);

        const QString txt = fmtVal(mk.value, m_log);
        const QSizeF sz = labelSize(txt, m_fontPx);
        QRectF br(x + 8, y - 8 - sz.height() / 2, sz.width(), sz.height());
        p->setBrush(Qt::white);
        p->drawRect(br);
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(mk.color, 1));
        p->drawRect(br);
        p->setPen(Qt::black);
        p->drawText(br, Qt::AlignCenter, txt);
    }

    // Р С”Р С•Р СР СР ВµР Р…РЎвЂљР В°РЎР‚Р С‘Р С‘: РЎвЂљР С•РЎвЂљ Р В¶Р Вµ РЎв‚¬РЎР‚Р С‘РЎвЂћРЎвЂљ
    for (const QVariant &cv : m_comments) {
        const QVariantMap cm = cv.toMap();
        double cx = 0, cy = 0;
        const QRectF box = commentBox(cm, padL, padT, pw, ph, tStart, cx, cy);
        if (!std::isfinite(cx) || !std::isfinite(cy)) continue;
        if (cy < padT - 2 || cy > padT + ph + 2) continue;
        const bool hl = (cm.value("id").toInt() == m_hlComment);
        p->setPen(QPen(hl ? QColor("#C22B2B") : QColor("#E8A013"), hl ? 2 : 1));
        p->setBrush(hl ? QColor("#FFE9C9") : QColor("#FFF8E6"));
        p->drawEllipse(QPointF(cx, cy), 2.5, 2.5);
        p->drawRect(box);
        p->setPen(QColor("#33424F"));
        p->setBrush(Qt::NoBrush);
        p->drawText(box.adjusted(5, 3, -5, -3), Qt::AlignLeft | Qt::AlignTop,
                    cm.value("text").toString());
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
    if (pw <= 4 || ph <= 4) return;
    const double tEnd = m_store->lastEpoch() - m_offset;
    const double tStart = tEnd - m_window;
    const double t = tStart + (y - padT) / ph * m_window;
    if (t < tStart || t > tEnd) return;

    int bestSensor = -1;
    double bestDist = 1e18, bestVal = 0;
    QColor bestColor;
    for (const QVariant &tv : m_traces) {
        const QVariantMap m = tv.toMap();
        const int sensor = m.value("sensorId").toInt();
        double lo = 0, hi = 100;
        traceRange(sensor, lo, hi);
        const QVector<Sample> smp = m_store->snapshot(uint16_t(sensor));
        if (smp.isEmpty()) continue;
        int idx = -1;
        double bd = 1e18;
        for (int i = 0; i < smp.size(); ++i) {
            const double d = qAbs(smp[i].t - t);
            if (d < bd) { bd = d; idx = i; }
        }
        if (idx < 0) continue;
        double v = smp[idx].v;
        if (m_log && (!std::isfinite(v) || !(v > 0))) v = kLogZero;
        const double d = qAbs(xOf(v, lo, hi, padL, pw) - x);
        if (d < bestDist) {
            bestDist = d; bestSensor = sensor; bestVal = v;
            bestColor = QColor(m.value("color").toString());
        }
    }
    if (bestSensor < 0) return;
    Marker mk;
    mk.id = m_nextMarkerId++;
    mk.t = t;
    mk.sensor = bestSensor; mk.value = bestVal; mk.color = bestColor;
    m_markers.append(mk);
    update();
}

int GeoChartItem::markerAt(double x, double y) const
{
    const QRectF r = boundingRect();
    const double padL = 4, padR = 4, padT = 4, padB = 10;
    const double pw = r.width() - padL - padR;
    const double ph = r.height() - padT - padB;
    if (pw <= 4 || ph <= 4 || !m_store) return -1;
    const double tEnd = m_store->lastEpoch() - m_offset;
    const double tStart = tEnd - m_window;
    for (const Marker &mk : m_markers) {
        double lo = 0, hi = 100;
        traceRange(mk.sensor, lo, hi);
        const double yy = padT + (mk.t - tStart) / m_window * ph;
        const double xx = qBound(padL, xOf(mk.value, lo, hi, padL, pw), padL + pw);
        const QSizeF sz = labelSize(fmtVal(mk.value, m_log), m_fontPx);
        const QRectF br(xx + 8, yy - 8 - sz.height() / 2, sz.width(), sz.height());
        if (br.contains(x, y) || (qAbs(xx - x) < 12 && qAbs(yy - y) < 12))
            return mk.id;
    }
    return -1;
}

void GeoChartItem::moveMarker(int id, double t)
{
    if (!m_store) return;
    for (int i = 0; i < m_markers.size(); ++i) {
        Marker &mk = m_markers[i];
        if (mk.id != id) continue;
        mk.t = t;
        const QVector<Sample> s = m_store->snapshot(uint16_t(mk.sensor));
        if (!s.isEmpty()) {
            auto it = std::lower_bound(s.begin(), s.end(), t,
                                       [](const Sample &sm, double tt){ return sm.t < tt; });
            if (it == s.end()) mk.value = s.last().v;
            else if (it == s.begin()) mk.value = it->v;
            else {
                auto prev = it - 1;
                mk.value = (t - prev->t < it->t - t) ? prev->v : it->v;
            }
        }
        update();
        return;
    }
}

bool GeoChartItem::removeMarkerAt(double x, double y)
{
    const int id = markerAt(x, y);
    if (id < 0) return false;
    for (int i = 0; i < m_markers.size(); ++i) {
        if (m_markers[i].id == id) {
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
    const double tStart = m_store->lastEpoch() - m_offset - m_window;
    for (const QVariant &cv : m_comments) {
        const QVariantMap m = cv.toMap();
        double cx = 0, cy = 0;
        const QRectF box = commentBox(m, padL, padT, pw, ph, tStart, cx, cy);
        if (box.adjusted(-4, -4, 4, 4).contains(x, y)) return m.value("id").toInt();
        if (qAbs(x - cx) < 10 && qAbs(y - cy) < 10) return m.value("id").toInt();
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
    const double tStart = m_store->lastEpoch() - m_offset - m_window;
    out.append(tStart + qBound(0.0, (y - padT) / ph, 1.0) * m_window);
    out.append(qBound(0.0, (x - padL) / pw, 1.0));
    return out;
}