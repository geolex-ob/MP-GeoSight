#pragma once
#include <QObject>
#include <QMap>
#include <QVariantList>
#include <cstdint>

class SensorCatalog : public QObject
{
    Q_OBJECT
public:
    bool loadCsv(const QString &path);
    Q_INVOKABLE bool reloadCsv();
    Q_INVOKABLE bool hasCsv() const { return m_hasCsv; }
    Q_INVOKABLE int csvCount() const { return m_csvName.size(); }
    Q_INVOKABLE QVariantList csvSensors() const;
    Q_INVOKABLE QString displayName(int id) const;
    Q_INVOKABLE QString displayUnit(int id) const;
    Q_INVOKABLE double displayMin(int id) const;
    Q_INVOKABLE double displayMax(int id) const;
    explicit SensorCatalog(QObject *parent = nullptr);
    void load(const QString &iniPath);

    Q_INVOKABLE QString name(int id) const;
    Q_INVOKABLE QString unit(int id) const;
    Q_INVOKABLE double defMin(int id) const;
    Q_INVOKABLE double defMax(int id) const;
    Q_INVOKABLE QVariantList list() const;

public slots:
    void registerDiscovered(const QVector<uint16_t> &ids);

signals:
    void changed();

private:
    QMap<int, QString> m_csvName;
    QMap<int, QString> m_csvUnit;
    QMap<int, double> m_csvMin;
    QMap<int, double> m_csvMax;
    bool m_hasCsv = false;
    struct Info { QString name, unit; double mn = 0, mx = 100; };
    QMap<uint16_t, Info> m_info;
};
