#include "GeoChartItem.h"

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

static constexpr double kLogZero = 1e-7;
static constexpr double kRulerHitPx = 8.0;

static QDateTime utcAt(double t)
{
    #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QDateTime::fromSecsSinceEpoch(qint64(t), QTimeZone::UTC);
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
    return QSizeF(fm.horizontalAdvance(txt) + 10.0, fm.height() + 4.0);
}

static QSizeF commentSize(const QString &txt, int px)
{
    const QFontMetrics fm(labelFont(px));
    const QStringList lines = txt.split('\n');
    int w = 0;
    for (const QString &ln : lines) w = qMax(w, fm.horizontalAdvance(ln));
    return QSizeF(w + 12.0, lines.size() * fm.height() + 8.0);
}

static QRectF placeLabel(const QPointF &a, const QSizeF &sz,
                         double padL, double padT, double pw, double ph,
                         const QVector<QRectF> &placed)
{
    const double gap = 8.0;
    const double stepY = sz.height() + 3.0;
    const double minX = padL, maxX = padL + pw - sz.width();
    const double minY = padT, maxY = padT + ph - sz.height();

    QRectF best;
    double bestPenalty = 1e18;

    for (int side = 0; side < 2; ++side) {
        double bx = (side == 0) ? a.x() + gap : a.x() - gap - sz.width();
        if (bx < minX - 0.5 || bx > maxX + 0.5) continue;
        bx = qBound(minX, bx, maxX);

        for (int k = 0; k <= 60; ++k) {
            double dy = 0.0;
            if (k > 0) {
                const int n = (k + 1) / 2;
                dy = (k % 2 == 1) ? -n * stepY : n * stepY;
            }
            const double by = a.y() - sz.height() / 2.0 + dy;
            if (by < minY - 0.5 || by > maxY + 0.5) continue;

            const QRectF r(bx, by, sz.width(), sz.height());
            double penalty = 0.0;
            for (const QRectF &p : placed) {
                const QRectF in = r.intersected(p.adjusted(-2, -2, 2, 2));
                if (in.isValid() && in.width() > 0 && in.height() > 0)
                    penalty += in.width() * in.height();
            }
            if (penalty <= 0.0) return r;
            if (penalty < bestPenalty) { bestPenalty = penalty; best = r; }
        }
    }
    if (best.isValid()) return best;
    return QRectF(qBound(minX, a.x() + gap, maxX),
                  qBound(minY, a.y() - sz.height() / 2.0, maxY),
                  sz.width(), sz.height());
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
    rebuildRuler();
    emit storeChanged();
    update();
}

void GeoChartItem::setTraces(const QVariantList &v)
{
    m_traces = v;
    rebuildRuler();
    emit paramsChanged();
    update();
}

void GeoChartItem::setComments(const QVariantList &v) { m_comments = v; emit paramsChanged(); update(); }
void GeoChartItem::setWindowSeconds(int v) { if (m_window == v) return; m_window = v; emit paramsChanged(); update(); }
void GeoChartItem::setOffsetSeconds(int v) { if (m_offset == v) return; m_offset = v; emit paramsChanged(); update(); }
void GeoChartItem::setLogScale(bool v) { if (m_log == v) return; m_log = v; rebuildRuler(); emit paramsChanged(); update(); }
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

QVariantList GeoChartItem::rulerTimes() const
{
    QVariantList out;
    for (double t : m_rulerTimes) out.append(t);
    return out;
}

void GeoChartItem::setRulerTimes(const QVariantList &v)
{
    QVector<double> nv;
    nv.reserve(v.size());
    for (const QVariant &x : v) nv.append(x.toDouble());
    // Порядок НЕ сортируем: индекс линейки должен совпадать
    // с порядком массива root.rulerTimes в QML, иначе при нескольких
    // линейках захватывается и двигается не та, на которую навели.
    if (nv == m_rulerTimes) return;
    m_rulerTimes = nv;
    rebuildRuler();
    emit paramsChanged();
    update();
}

void GeoChartItem::clearRulers()
{
    if (m_rulerTimes.isEmpty()) return;
    m_rulerTimes.clear();
    rebuildRuler();
    emit paramsChanged();
    update();
}

int GeoChartItem::rulerIndexAt(double y) const
{
    if (m_rulerTimes.isEmpty() || !m_store) return -1;
    const QRectF r = boundingRect();
    const double padT = 4, padB = 10;
    const double ph = r.height() - padT - padB;
    if (ph <= 4) return -1;
    const double tStart = m_store->lastEpoch() - m_offset - m_window;
    int best = -1;
    double bestD = kRulerHitPx;
    for (int i = 0; i < m_rulerTimes.size(); ++i) {
        const double ry = padT + (m_rulerTimes[i] - tStart) / m_window * ph;
        const double d = qAbs(ry - y);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

void GeoChartItem::rebuildRuler()
{
    for (int i = m_markers.size() - 1; i >= 0; --i)
        if (m_markers[i].ruler) m_markers.removeAt(i);

        if (m_rulerTimes.isEmpty() || !m_store) return;

        for (double t : m_rulerTimes) {
            for (const QVariant &tv : m_traces) {
                const QVariantMap m = tv.toMap();
                Marker mk;
                mk.id = m_nextMarkerId++;
                mk.t = t;
                mk.sensor = m.value("sensorId").toInt();
                mk.value = sampleAt(mk.sensor, t);
                mk.color = QColor(m.value("color").toString());
                mk.ruler = true;
                m_markers.append(mk);
            }
        }
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

double GeoChartItem::sampleAt(int sensor, double t) const
{
    if (!m_store) return 0.0;
    const QVector<Sample> s = m_store->snapshot(uint16_t(sensor));
    if (s.isEmpty()) return m_log ? kLogZero : 0.0;
    auto it = std::lower_bound(s.begin(), s.end(), t,
                               [](const Sample &sm, double tt){ return sm.t < tt; });
    double v;
    if (it == s.end()) v = s.last().v;
    else if (it == s.begin()) v = it->v;
    else { auto prev = it - 1; v = (t - prev->t < it->t - t) ? prev->v : it->v; }
    if (m_log && (!std::isfinite(v) || !(v > 0))) v = kLogZero;
    return v;
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

                                    const double tEnd = m_store ? m_store->lastEpoch() - m_offset : 0;
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
                                        for (double t = qCeil(tStart / step) * step; t <= tEnd + 1e-9; t += step) {
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

                                    // линии линеек (сколько есть — столько и рисуем)
                                    for (double rt : m_rulerTimes) {
                                        const double ry = padT + (rt - tStart) / m_window * ph;
                                        if (ry < padT - 1 || ry > padT + ph + 1) continue;
                                        QPen rp(QColor("#E03A2F"));
                                        rp.setWidthF(1.0);
                                        p->setPen(rp);
                                        p->drawLine(QPointF(padL, ry), QPointF(padL + pw, ry));
                                    }

                                    // подписи: сначала резервируем комментарии, затем разводим метки линеек и обычные
                                    QVector<QRectF> placed;
                                    struct CBox { QRectF box; double cx, cy; QString text; bool hl; };
                                    QVector<CBox> cboxes;
                                    for (const QVariant &cv : m_comments) {
                                        const QVariantMap cm = cv.toMap();
                                        double cx = 0, cy = 0;
                                        const QRectF box = commentBox(cm, padL, padT, pw, ph, tStart, cx, cy);
                                        if (!std::isfinite(cx) || !std::isfinite(cy)) continue;
                                        if (cy < padT - 2 || cy > padT + ph + 2) continue;
                                        const bool hl = (cm.value("id").toInt() == m_hlComment);
                                        cboxes.append(CBox{box, cx, cy, cm.value("text").toString(), hl});
                                        placed.append(box);
                                    }

                                    struct Lbl { QPointF anchor; QRectF box; QString text; QColor color; };
                                    QVector<Lbl> labels;

                                    QVector<Marker> sorted = m_markers;
                                    std::stable_sort(sorted.begin(), sorted.end(), [](const Marker &a, const Marker &b){
                                        if (a.t != b.t) return a.t < b.t;
                                        return a.sensor < b.sensor;
                                    });

                                    p->setFont(labelFont(m_fontPx));
                                    for (const Marker &mk : sorted) {
                                        double lo = 0, hi = 100;
                                        traceRange(mk.sensor, lo, hi);
                                        const double y = padT + (mk.t - tStart) / m_window * ph;
                                        const double x = qBound(padL, xOf(mk.value, lo, hi, padL, pw), padL + pw);
                                        if (!std::isfinite(x) || !std::isfinite(y)) continue;
                                        if (y < padT - 2 || y > padT + ph + 2) continue;

                                        const QString txt = fmtVal(mk.value, m_log);
                                        const QSizeF sz = labelSize(txt, m_fontPx);
                                        const QPointF anchor(x, y);
                                        const QRectF box = placeLabel(anchor, sz, padL, padT, pw, ph, placed);
                                        placed.append(box);
                                        labels.append(Lbl{anchor, box, txt, mk.color});
                                    }

                                    for (const Lbl &l : labels) {
                                        const double bx = (l.box.left() > l.anchor.x()) ? l.box.left() : l.box.right();
                                        const double by = l.box.center().y();
                                        p->setPen(QPen(l.color, 1));
                                        p->setBrush(Qt::NoBrush);
                                        if (qAbs(by - l.anchor.y()) > 1.0 || qAbs(bx - l.anchor.x()) > 9.0)
                                            p->drawLine(l.anchor, QPointF(bx, by));
                                        p->drawEllipse(l.anchor, 2.2, 2.2);

                                        p->setBrush(QColor(255, 255, 255, 240));
                                        p->setPen(QPen(l.color, 1));
                                        p->drawRect(l.box);
                                        p->setPen(QColor("#101820"));
                                        p->drawText(l.box, Qt::AlignCenter, l.text);
                                    }

                                    for (const CBox &c : cboxes) {
                                        p->setPen(QPen(c.hl ? QColor("#C22B2B") : QColor("#E8A013"), c.hl ? 2 : 1));
                                        p->setBrush(c.hl ? QColor("#FFE9C9") : QColor("#FFF8E6"));
                                        p->drawEllipse(QPointF(c.cx, c.cy), 2.5, 2.5);
                                        p->drawRect(c.box);
                                        p->setPen(QColor("#33424F"));
                                        p->setBrush(Qt::NoBrush);
                                        p->drawText(c.box.adjusted(5, 3, -5, -3), Qt::AlignLeft | Qt::AlignTop, c.text);
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
                                    const double tStart = m_store->lastEpoch() - m_offset - m_window;
                                    for (const Marker &mk : m_markers) {
                                        if (mk.ruler) continue;
                                        double lo = 0, hi = 100;
                                        traceRange(mk.sensor, lo, hi);
                                        const double yy = padT + (mk.t - tStart) / m_window * ph;
                                        const double xx = qBound(padL, xOf(mk.value, lo, hi, padL, pw), padL + pw);
                                        if (qAbs(xx - x) < 14 && qAbs(yy - y) < 14) return mk.id;
                                        const QSizeF sz = labelSize(fmtVal(mk.value, m_log), m_fontPx);
                                        const QRectF br(xx + 8, yy - sz.height() / 2, sz.width(), sz.height());
                                        const QRectF bl(xx - 8 - sz.width(), yy - sz.height() / 2, sz.width(), sz.height());
                                        if (br.adjusted(-4, -18, 4, 18).contains(x, y) || bl.adjusted(-4, -18, 4, 18).contains(x, y))
                                            return mk.id;
                                    }
                                    return -1;
                                }

                                void GeoChartItem::moveMarker(int id, double t)
                                {
                                    if (!m_store) return;
                                    for (Marker &mk : m_markers) {
                                        if (mk.id != id) continue;
                                        mk.t = t;
                                        mk.value = sampleAt(mk.sensor, t);
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
