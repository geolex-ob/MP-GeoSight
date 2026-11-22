#pragma once
#include <QQuickPaintedItem>
#include "core/DataStore.h"

class TimeAxisItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(DataStore* store READ store WRITE setStore NOTIFY storeChanged)
    Q_PROPERTY(int windowSeconds READ windowSeconds WRITE setWindowSeconds NOTIFY paramsChanged)
    Q_PROPERTY(int depthSensorId READ depthSensorId WRITE setDepthSensorId NOTIFY paramsChanged)
    Q_PROPERTY(int offsetSeconds READ offsetSeconds WRITE setOffsetSeconds NOTIFY paramsChanged)

public:
    explicit TimeAxisItem(QQuickItem *parent = nullptr);
    void paint(QPainter *p) override;

    DataStore *store() const { return m_store; }
    void setStore(DataStore *s);
    int windowSeconds() const { return m_window; }
    void setWindowSeconds(int v);
    int depthSensorId() const { return m_depth; }
    void setDepthSensorId(int v);
    int offsetSeconds() const { return m_offset; }
    void setOffsetSeconds(int v);

signals:
    void storeChanged();
    void paramsChanged();

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    void syncTextureSize();
    DataStore *m_store = nullptr;
    int m_window = 900;
    int m_depth = 101;
    int m_offset = 0;
};
