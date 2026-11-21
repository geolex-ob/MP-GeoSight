#include "ChartConfigManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

ChartConfigManager::ChartConfigManager(QObject *parent) : QAbstractListModel(parent)
{
    m_path = QCoreApplication::applicationDirPath() + "/charts.json";
    load();
}

void ChartConfigManager::load()
{
    beginResetModel();
    m_cols.clear();
    QFile f(m_path);
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
        m_timeWindow = root.value("timeWindow").toInt(900);
        m_depthSensor = root.value("depthSensor").toInt(101);
    m_bitSensor = root.value("bitSensor").toInt(115);
    if (m_bitSensor == 102 && !root.value("bitSensorMigrated").toBool(false))
        m_bitSensor = 115;   // 102 — «над забоем», положение долота = 115
        for (const QJsonValue &cv : root.value("columns").toArray()) {
            Column c;
            QJsonArray traceArr;
            if (cv.isObject()) {
                const QJsonObject co = cv.toObject();
                c.log = co.value("log").toBool(false);
                c.lo = co.value("min").toDouble(0.0001);
                c.hi = co.value("max").toDouble(10);
                traceArr = co.value("traces").toArray();
            } else {
                traceArr = cv.toArray();
            }
            for (const QJsonValue &tv : traceArr) {
                const QJsonObject o = tv.toObject();
                Trace t;
                t.id = o.value("id").toInt();
                t.sensorId = o.value("sensor").toInt();
                t.title = o.value("title").toString();
                t.color = o.value("color").toString("#0000CC");
                t.mn = o.value("min").toDouble(0);
                t.mx = o.value("max").toDouble(100);
                c.traces.append(t);
            }
            m_cols.append(c);
        }
        f.close();
    }
    if (m_cols.isEmpty()) {
        m_cols = {
            { { {1, 201, "Нагр.", "#CC0000", 0, 150},
                {2, 200, "W кр.", "#0000CC", 0, 150},
                {3, 103, "Тальблок", "#00AA00", -5, 40},
                {4, 300, "Р вх.", "#CC00CC", 0, 300} } },
            { { {5, 720, "V-S", "#CC0000", 0, 200},
                {6, 711, "V ёмк.", "#0000CC", 0, 50},
                {7, 600, "Q вх.", "#00AA00", 0, 60},
                {8, 101, "Гл.инстр.", "#CC00CC", 0, 4000} } },
            { { {9, 1600, "Метан абс.", "#0000CC", 0, 5},
                {10, 1601, "Этан абс.", "#00AA00", 0, 1},
                {11, 1602, "Сумма газов", "#CC00CC", 0, 5} } },
        };
        save();
    }
    endResetModel();
    emit configChanged();
}

void ChartConfigManager::save()
{
    QJsonObject root;
    root["timeWindow"] = m_timeWindow;
    root["depthSensor"] = m_depthSensor;
    root["bitSensor"] = m_bitSensor;
    root["bitSensorMigrated"] = true;
    QJsonArray cols;
    for (const Column &c : m_cols) {
        QJsonArray ts;
        for (const Trace &t : c.traces) {
            QJsonObject o;
            o["id"] = t.id;
            o["sensor"] = t.sensorId;
            o["title"] = t.title;
            o["color"] = t.color;
            o["min"] = t.mn;
            o["max"] = t.mx;
            ts.append(o);
        }
        QJsonObject co;
        co["log"] = c.log;
        co["min"] = c.lo;
        co["max"] = c.hi;
        co["traces"] = ts;
        cols.append(co);
    }
    root["columns"] = cols;
    QFile f(m_path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(root).toJson());
        f.close();
    }
}

int ChartConfigManager::rowCount(const QModelIndex &) const { return m_cols.size(); }

QVariant ChartConfigManager::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_cols.size()) return {};
    if (role != TracesRole) return {};
    QVariantList out;
    for (const Trace &t : m_cols[index.row()].traces) {
        QVariantMap m;
        m["id"] = t.id;
        m["sensorId"] = t.sensorId;
        m["title"] = t.title;
        m["color"] = t.color;
        m["min"] = t.mn;
        m["max"] = t.mx;
        out.append(m);
    }
    return out;
}

QHash<int, QByteArray> ChartConfigManager::roleNames() const
{
    return { {TracesRole, "traces"} };
}

void ChartConfigManager::setTimeWindow(int s)
{
    s = qBound(30, s, 86400);
    if (s == m_timeWindow) return;
    m_timeWindow = s;
    save();
    emit configChanged();
}

void ChartConfigManager::setDepthSensorId(int id)
{
    if (id == m_depthSensor) return;
    m_depthSensor = id;
    save();
    emit configChanged();
}

int ChartConfigManager::maxTraceRows() const
{
    int n = 1;
    for (const Column &c : m_cols) n = qMax(n, int(c.traces.size()));
    return n;
}

int ChartConfigManager::nextId()
{
    int mx = 0;
    for (const Column &c : m_cols)
        for (const Trace &t : c.traces) mx = qMax(mx, t.id);
    return mx + 1;
}

void ChartConfigManager::addColumn()
{
    beginInsertRows(QModelIndex(), m_cols.size(), m_cols.size());
    m_cols.append(Column());
    endInsertRows();
    save();
    emit configChanged();
}

void ChartConfigManager::removeColumn(int col)
{
    if (col < 0 || col >= m_cols.size()) return;
    beginRemoveRows(QModelIndex(), col, col);
    m_cols.removeAt(col);
    endRemoveRows();
    save();
    emit configChanged();
}

void ChartConfigManager::addTrace(int col, int sensorId, const QString &title,
                                  const QString &color, double mn, double mx)
{
    if (col < 0 || col >= m_cols.size()) return;
    Trace t;
    t.id = nextId();
    t.sensorId = sensorId;
    t.title = title;
    t.color = color;
    t.mn = mn;
    t.mx = (mx > mn) ? mx : mn + 1;
    m_cols[col].traces.append(t);
    emit dataChanged(index(col), index(col));
    save();
    emit configChanged();
}

void ChartConfigManager::updateTrace(int col, int traceId, int sensorId, const QString &title,
                                     const QString &color, double mn, double mx)
{
    if (col < 0 || col >= m_cols.size()) return;
    for (Trace &t : m_cols[col].traces) {
        if (t.id != traceId) continue;
        t.sensorId = sensorId;
        t.title = title;
        t.color = color;
        t.mn = mn;
        t.mx = (mx > mn) ? mx : mn + 1;
        emit dataChanged(index(col), index(col));
        save();
        emit configChanged();
        return;
    }
}

void ChartConfigManager::removeTrace(int col, int traceId)
{
    if (col < 0 || col >= m_cols.size()) return;
    for (int i = 0; i < m_cols[col].traces.size(); ++i) {
        if (m_cols[col].traces[i].id != traceId) continue;
        m_cols[col].traces.removeAt(i);
        emit dataChanged(index(col), index(col));
        save();
        emit configChanged();
        return;
    }
}

void ChartConfigManager::setViewOffset(int sec)
{
    sec = qBound(0, sec, 6 * 3600);
    if (sec == m_viewOffset) return;
    m_viewOffset = sec;
    emit viewOffsetChanged();
}

void ChartConfigManager::setSelectedSensor(int id)
{
    if (id == m_selectedSensor) return;
    m_selectedSensor = id;
    emit selectedChanged();
}

QVariantList ChartConfigManager::allSensorIds() const
{
    QVariantList out;
    out.append(m_depthSensor);
    for (const Column &c : m_cols)
        for (const Trace &t : c.traces)
            if (!out.contains(t.sensorId)) out.append(t.sensorId);
    return out;
}

void ChartConfigManager::saveNow()
{
    save();
}

void ChartConfigManager::resetToDefaults()
{
    QFile::remove(m_path);
    load();
}

QString ChartConfigManager::lastError() const { return m_lastError; }
QString ChartConfigManager::appDir() const { return QCoreApplication::applicationDirPath(); }
QString ChartConfigManager::homeDir() const { return QDir::homePath(); }
QString ChartConfigManager::libraryDir() const { return appDir() + "/profiles"; }

bool ChartConfigManager::exportToFile(const QString &path)
{
    m_lastError.clear();
    save();
    QFile::remove(path);
    if (!QFile::copy(m_path, path)) {
        m_lastError = "не удалось записать файл " + path;
        return false;
    }
    return true;
}

bool ChartConfigManager::importFromFile(const QString &path)
{
    m_lastError.clear();
    QFile f(path);
    if (!f.exists()) { m_lastError = "файл не найден"; return false; }
    if (!f.open(QIODevice::ReadOnly)) { m_lastError = "файл не читается"; return false; }
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    f.close();
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        m_lastError = "это не JSON-профиль";
        return false;
    }
    const QJsonObject imp = doc.object();
    if (!imp.contains("columns") || !imp.value("columns").isArray()) {
        m_lastError = "файл не похож на профиль GTI Monitor (нет списка колонок)";
        return false;
    }
    QFile::remove(m_path);
    if (!QFile::copy(path, m_path)) { m_lastError = "не удалось применить профиль"; return false; }
    load();
    return true;
}

QStringList ChartConfigManager::profileNames() const
{
    QDir d(libraryDir());
    return d.entryList(QStringList() << "*.cfg", QDir::Files, QDir::Name);
}

QString ChartConfigManager::profilePath(const QString &name) const
{
    return libraryDir() + "/" + name;
}

bool ChartConfigManager::saveToLibrary(const QString &name)
{
    m_lastError.clear();
    QString n = name.trimmed();
    if (n.isEmpty()) { m_lastError = "пустое имя профиля"; return false; }
    n.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_");
    if (!n.endsWith(".cfg", Qt::CaseInsensitive)) n += ".cfg";
    QDir().mkpath(libraryDir());
    return exportToFile(libraryDir() + "/" + n);
}

bool ChartConfigManager::deleteFile(const QString &path)
{
    m_lastError.clear();
    if (QFile::remove(path)) return true;
    m_lastError = "не удалось удалить файл";
    return false;
}

QVariantList ChartConfigManager::tracesOf(int col) const
{
    if (col < 0 || col >= m_cols.size()) return {};
    return data(index(col), TracesRole).toList();
}

void ChartConfigManager::setBitSensorId(int id)
{
    if (id == m_bitSensor) return;
    m_bitSensor = id;
    save();
    emit configChanged();
}

bool ChartConfigManager::columnLog(int col) const
{
    return (col >= 0 && col < m_cols.size()) ? m_cols[col].log : false;
}
double ChartConfigManager::columnMin(int col) const
{
    return (col >= 0 && col < m_cols.size()) ? m_cols[col].lo : 0.0001;
}
double ChartConfigManager::columnMax(int col) const
{
    return (col >= 0 && col < m_cols.size()) ? m_cols[col].hi : 10.0;
}
void ChartConfigManager::setColumnLog(int col, bool on)
{
    if (col < 0 || col >= m_cols.size()) return;
    if (m_cols[col].log == on) return;
    m_cols[col].log = on;
    if (on && !(m_cols[col].lo > 0 && m_cols[col].hi > m_cols[col].lo)) {
        m_cols[col].lo = 0.0001;
        m_cols[col].hi = 10;
    }
    save();
    emit configChanged();
}
void ChartConfigManager::setColumnRange(int col, double mn, double mx)
{
    if (col < 0 || col >= m_cols.size()) return;
    if (!(mn > 0)) mn = 0.0001;
    if (!(mx > mn)) mx = mn * 10;
    m_cols[col].lo = mn;
    m_cols[col].hi = mx;
    save();
    emit configChanged();
}
