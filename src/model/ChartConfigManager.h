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

struct Comment {
    int id = 0;
    double t = 0;
    double yFrac = 0;
    QString text;
};

struct InfoPanel {
    int sensorId = 0;
    QString title;
    QString unit;
    int decimals = 2;
};

struct Column {
    QVector<Trace> traces;
    bool log = false;
    double lo = 0.0001;
    double hi = 10;
    QVector<Comment> comments;
    double weight = 1.0;
};

class ChartConfigManager : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int timeWindow READ timeWindow WRITE setTimeWindow NOTIFY configChanged)
    Q_PROPERTY(int depthSensorId READ depthSensorId WRITE setDepthSensorId NOTIFY configChanged)
    Q_PROPERTY(int bitSensorId READ bitSensorId WRITE setBitSensorId NOTIFY configChanged)
    Q_PROPERTY(int maxTraceRows READ maxTraceRows NOTIFY configChanged)
    Q_PROPERTY(int viewOffset READ viewOffset WRITE setViewOffset NOTIFY viewOffsetChanged)
    Q_PROPERTY(int selectedSensor READ selectedSensor WRITE setSelectedSensor NOTIFY selectedChanged)
    Q_PROPERTY(int standLength READ standLength WRITE setStandLength NOTIFY configChanged)
    Q_PROPERTY(int standsMode READ standsMode WRITE setStandsMode NOTIFY configChanged)
    Q_PROPERTY(int standsSensorId READ standsSensorId WRITE setStandsSensorId NOTIFY configChanged)
    Q_PROPERTY(int headerFontPx READ headerFontPx WRITE setHeaderFontPx NOTIFY configChanged)

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
    int standLength() const { return m_standLength; }
    void setStandLength(int m);
    int standsMode() const { return m_standsMode; }
    void setStandsMode(int m);
    int standsSensorId() const { return m_standsSensor; }
    void setStandsSensorId(int id);
    int headerFontPx() const { return m_headerFont; }
    void setHeaderFontPx(int px);

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
    Q_INVOKABLE bool columnLog(int col) const;
    Q_INVOKABLE double columnMin(int col) const;
    Q_INVOKABLE double columnMax(int col) const;
    Q_INVOKABLE void setColumnLog(int col, bool on);
    Q_INVOKABLE void setColumnRange(int col, double mn, double mx);
    Q_INVOKABLE double columnWeight(int col) const;
    Q_INVOKABLE void setColumnWeight(int col, double w);
    Q_INVOKABLE void moveComment(int col, int id, double t, double yFrac);
    Q_INVOKABLE QVariantList commentsOf(int col) const;
    Q_INVOKABLE int addComment(int col, double t, double yFrac, const QString &text);
    Q_INVOKABLE void updateComment(int col, int id, const QString &text);
    Q_INVOKABLE void removeComment(int col, int id);
    Q_INVOKABLE QVariantList infos() const;
    Q_INVOKABLE void setInfo(int i, int sensorId, const QString &title, const QString &unit, int decimals);

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
    QVector<InfoPanel> m_infos;
    int m_timeWindow = 900;
    int m_viewOffset = 0;
    int m_selectedSensor = 0;
    int m_standLength = 25;
    int m_standsMode = 0;
    int m_standsSensor = 111;
    int m_headerFont = 10;
    int m_nextCommentId = 1;
    QString m_lastError;
    int m_depthSensor = 101;
    int m_bitSensor = 115;
    QString m_path;
};
