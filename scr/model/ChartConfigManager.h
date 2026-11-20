#pragma once
#include <QAbstractListModel>
#include <QVector>
#include <QVariantList>
#include <QString>

struct Trace {
    int id = 0;
    int sensorId = 0;
    QString title;
    QString color;
    double mn = 0;
    double mx = 100;
};

struct Column { QVector<Trace> traces; };

class ChartConfigManager : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int timeWindow READ timeWindow WRITE setTimeWindow NOTIFY configChanged)
    Q_PROPERTY(int depthSensorId READ depthSensorId WRITE setDepthSensorId NOTIFY configChanged)
    Q_PROPERTY(int bitSensorId READ bitSensorId WRITE setBitSensorId NOTIFY configChanged)
    Q_PROPERTY(int maxTraceRows READ maxTraceRows NOTIFY configChanged)
    Q_PROPERTY(int viewOffset READ viewOffset WRITE setViewOffset NOTIFY viewOffsetChanged)
    Q_PROPERTY(int selectedSensor READ selectedSensor WRITE setSelectedSensor NOTIFY selectedChanged)

public:
    enum Roles { TracesRole = Qt::UserRole + 1 };

    explicit ChartConfigManager(QObject *parent = nullptr);

    int rowCount(const QModelIndex & = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int timeWindow() const { return m_timeWindow; }
    void setTimeWindow(int s);
    int depthSensorId() const { return m_depthSensor; }
    void setDepthSensorId(int id);
    int bitSensorId() const { return m_bitSensor; }
    void setBitSensorId(int id);
    int maxTraceRows() const;
    int viewOffset() const { return m_viewOffset; }
    void setViewOffset(int s);
    int selectedSensor() const { return m_selectedSensor; }
    void setSelectedSensor(int id);
    Q_INVOKABLE QVariantList allSensorIds() const;
    Q_INVOKABLE void saveNow();
    Q_INVOKABLE void resetToDefaults();
    Q_INVOKABLE bool exportToFile(const QString &path);
    Q_INVOKABLE bool importFromFile(const QString &path);
    Q_INVOKABLE QStringList profileNames() const;
    Q_INVOKABLE QString profilePath(const QString &name) const;
    Q_INVOKABLE bool saveToLibrary(const QString &name);
    Q_INVOKABLE bool deleteFile(const QString &path);
    Q_INVOKABLE QString lastError() const;
    Q_INVOKABLE QString appDir() const;
    Q_INVOKABLE QString homeDir() const;
    Q_INVOKABLE QString libraryDir() const;
    Q_INVOKABLE QVariantList tracesOf(int col) const;

    Q_INVOKABLE void addColumn();
    Q_INVOKABLE void removeColumn(int col);
    Q_INVOKABLE void addTrace(int col, int sensorId, const QString &title,
                              const QString &color, double mn, double mx);
    Q_INVOKABLE void updateTrace(int col, int traceId, int sensorId, const QString &title,
                                 const QString &color, double mn, double mx);
    Q_INVOKABLE void removeTrace(int col, int traceId);

signals:
    void configChanged();
    void viewOffsetChanged();
    void selectedChanged();

private:
    void load();
    void save();
    int nextId();

    QVector<Column> m_cols;
    int m_timeWindow = 900;
    int m_viewOffset = 0;
    int m_selectedSensor = 0;
    QString m_lastError;
    int m_depthSensor = 101;
    int m_bitSensor = 102;
    QString m_path;
};
