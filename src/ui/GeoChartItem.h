#pragma once
#include <QQuickPaintedItem>
#include <QVariantList>
#include <QColor>
#include <QVector>
#include "core/DataStore.h"

class GeoChartItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(DataStore* store READ store WRITE setStore NOTIFY storeChanged)
    Q_PROPERTY(QVariantList traces READ traces WRITE setTraces NOTIFY paramsChanged)
    Q_PROPERTY(int windowSeconds READ windowSeconds WRITE setWindowSeconds NOTIFY paramsChanged)
    Q_PROPERTY(int offsetSeconds READ offsetSeconds WRITE setOffsetSeconds NOTIFY paramsChanged)
    Q_PROPERTY(bool logScale READ logScale WRITE setLogScale NOTIFY paramsChanged)
    Q_PROPERTY(double sharedMin READ sharedMin WRITE setSharedMin NOTIFY paramsChanged)
    Q_PROPERTY(double sharedMax READ sharedMax WRITE setSharedMax NOTIFY paramsChanged)
    Q_PROPERTY(QVariantList comments READ comments WRITE setComments NOTIFY paramsChanged)

public:
    explicit GeoChartItem(QQuickItem *parent = nullptr);
    void paint(QPainter *p) override;

    DataStore *store() const { return m_store; }
    void setStore(DataStore *s);
    QVariantList traces() const { return m_traces; }
    void setTraces(const QVariantList &v);
    int windowSeconds() const { return m_window; }
    void setWindowSeconds(int v);
    int offsetSeconds() const { return m_offset; }
    void setOffsetSeconds(int v);
    bool logScale() const { return m_log; }
    void setLogScale(bool v);
    double sharedMin() const { return m_sharedMin; }
    void setSharedMin(double v);
    double sharedMax() const { return m_sharedMax; }
    void setSharedMax(double v);
    QVariantList comments() const { return m_comments; }
    void setComments(const QVariantList &v);

    Q_INVOKABLE void addMarkerAt(double x, double y, int preferredSensor = -1);
    Q_INVOKABLE bool removeMarkerAt(double x, double y);
    Q_INVOKABLE int commentAt(double x, double y) const;
    Q_INVOKABLE QVariantList commentCoords(double x, double y) const;

signals:
    void storeChanged();
    void paramsChanged();

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    void syncTextureSize();
    DataStore *m_store = nullptr;
    QVariantList m_traces;
    int m_window = 900;
    int m_offset = 0;
    bool m_log = false;
    double m_sharedMin = 0.0001;
    double m_sharedMax = 10;
    QVariantList m_comments;
    double xOf(double v, double lo, double hi, double padL, double pw) const;

    struct Marker { double t = 0; int sensor = 0; double value = 0; QColor color; };
    QVector<Marker> m_markers;
};
