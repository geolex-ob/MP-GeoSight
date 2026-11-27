#pragma once

#include <QQuickPaintedItem>
#include <QtGlobal>

#include "core/DataStore.h"

class TimeAxisItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(DataStore* store READ store WRITE setStore NOTIFY storeChanged)
    Q_PROPERTY(int windowSeconds READ windowSeconds WRITE setWindowSeconds NOTIFY paramsChanged)
    Q_PROPERTY(int depthSensorId READ depthSensorId WRITE setDepthSensorId NOTIFY paramsChanged)
    Q_PROPERTY(int toolDepthSensorId READ toolDepthSensorId WRITE setToolDepthSensorId NOTIFY paramsChanged)
    Q_PROPERTY(int offsetSeconds READ offsetSeconds WRITE setOffsetSeconds NOTIFY paramsChanged)
    Q_PROPERTY(int fontPx READ fontPx WRITE setFontPx NOTIFY paramsChanged)
    Q_PROPERTY(int preferredWidth READ preferredWidth NOTIFY paramsChanged)

public:
    explicit TimeAxisItem(QQuickItem *parent = nullptr);
    void paint(QPainter *p) override;

    DataStore *store() const { return m_store; }
    void setStore(DataStore *s);

    int windowSeconds() const { return m_window; }
    void setWindowSeconds(int v);

    int depthSensorId() const { return m_depth; }
    void setDepthSensorId(int v);

    // РќРћР’РћР•: РґР°С‚С‡РёРє РіР»СѓР±РёРЅС‹ РёРЅСЃС‚СЂСѓРјРµРЅС‚Р° (115 В«РџРѕР»РѕР¶РµРЅРёРµ РґРѕР»РѕС‚Р°В»)
    int toolDepthSensorId() const { return m_toolDepth; }
    void setToolDepthSensorId(int v);

    int offsetSeconds() const { return m_offset; }
    void setOffsetSeconds(int v);

    int fontPx() const { return m_fontPx; }
    void setFontPx(int px);

    int preferredWidth() const { return m_preferredWidth; }

signals:
    void storeChanged();
    void paramsChanged();

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;


private:
    void syncTextureSize();
    void recomputeWidth();

    DataStore *m_store = nullptr;
    int m_window = 900;
    int m_depth = 101;        // Р·Р°Р±РѕР№
    int m_toolDepth = 115;    // РіР»СѓР±РёРЅР° РёРЅСЃС‚СЂСѓРјРµРЅС‚Р°
    int m_offset = 0;
    int m_fontPx = 9;
    int m_preferredWidth = 92;
    double m_depthTextW = 34;
    double m_toolTextW = 34;
    double m_timeTextW = 46;
};