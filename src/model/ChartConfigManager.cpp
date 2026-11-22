#include "ChartConfigManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QRegularExpression>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>

ChartConfigManager::ChartConfigManager(QObject *parent) : QAbstractListModel(parent)
{
    m_path = QCoreApplication::applicationDirPath() + "/charts.json";
    load();
}

void ChartConfigManager::load()
{
    beginResetModel();
    m_cols.clear();
    m_infos.clear();
    QFile f(m_path);
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
        m_timeWindow = root.value("timeWindow").toInt(900);
        m_depthSensor = root.value("depthSensor").toInt(101);
        m_bitSensor = root.value("bitSensor").toInt(115);
        if (m_bitSensor == 102 && !root.value("bitSensorMigrated").toBool(false))
            m_bitSensor = 115;
        m_standLength = root.value("standLength").toInt(25);
        m_standsMode = root.value("standsMode").toInt(0);
        m_standsSensor = root.value("standsSensor").toInt(111);
        if (m_standLength < 1) m_standLength = 25;
        m_headerFont = root.value("headerFont").toInt(10);
        if (m_headerFont < 6) m_headerFont = 6;
        if (m_headerFont > 24) m_headerFont = 24;
        for (const QJsonValue &iv : root.value("infos").toArray()) {
            const QJsonObject o = iv.toObject();
            InfoPanel ip;
            ip.sensorId = o.value("sensor").toInt();
            ip.title = o.value("title").toString();
            ip.unit = o.value("unit").toString();
            ip.decimals = o.value("decimals").toInt(2);
            m_infos.append(ip);
        }
        for (const QJsonValue &cv : root.value("columns").toArray()) {
            Column c;
            QJsonArray traceArr;
            if (cv.isObject()) {
                const QJsonObject co = cv.toObject();
                c.log = co.value("log").toBool(false);
                c.lo = co.value("min").toDouble(0.0001);
                c.hi = co.value("max").toDouble(10);
                c.weight = co.value("weight").toDouble(1.0);
                if (!std::isfinite(c.weight) || c.weight <= 0.01) c.weight = 1.0;
                traceArr = co.value("traces").toArray();
                for (const QJsonValue &cv2 : co.value("comments").toArray()) {
                    const QJsonObject o = cv2.toObject();
                    Comment cm;
                    cm.id = o.value("id").toInt();
                    cm.t = o.value("t").toDouble();
                    cm.yFrac = o.value("yFrac").toDouble();
                    cm.text = o.value("text").toString();
                    c.comments.append(cm);
                    if (cm.id >= m_nextCommentId) m_nextCommentId = cm.id + 1;
                }
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
    if (m_infos.isEmpty()) {
        m_infos = { {101, "Глубина газа", "м", 2},
                    {101, "Глубина шлама", "м", 2},
                    {0,   "Время задержки", "мин", 1} };
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
    root["standLength"] = m_standLength;
    root["standsMode"] = m_standsMode;
    root["standsSensor"] = m_standsSensor;
    root["headerFont"] = m_headerFont;
    QJsonArray infosArr;
    for (const InfoPanel &ip : m_infos) {
        QJsonObject o;
        o["sensor"] = ip.sensorId;
        o["title"] = ip.title;
        o["unit"] = ip.unit;
        o["decimals"] = ip.decimals;
        infosArr.append(o);
    }
    root["infos"] = infosArr;
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
        QJsonArray cms;
        for (const Comment &cm : c.comments) {
            QJsonObject o;
            o["id"] = cm.id;
            o["t"] = cm.t;
            o["yFrac"] = cm.yFrac;
            o["text"] = cm.text;
            cms.append(o);
        }
        QJsonObject co;
        co["log"] = c.log;
        co["min"] = c.lo;
        co["max"] = c.hi;
        co["traces"] = ts;
        co["comments"] = cms;
        co["weight"] = c.weight;
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

void ChartConfigManager::setBitSensorId(int id)
{
    if (id == m_bitSensor) return;
    m_bitSensor = id;
    save();
    emit configChanged();
}

void ChartConfigManager::setStandLength(int m)
{
    if (m < 1) m = 1;
    if (m == m_standLength) return;
    m_standLength = m;
    save();
    emit configChanged();
}

void ChartConfigManager::setHeaderFontPx(int px)
{
    if (px < 6) px = 6;
    if (px > 24) px = 24;
    if (px == m_headerFont) return;
    m_headerFont = px;
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

void ChartConfigManager::saveNow() { save(); }

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
    n.replace(QRegularExpression("[/:*?\"<>|]"), "_");
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

QVariantList ChartConfigManager::commentsOf(int col) const
{
    QVariantList out;
    if (col < 0 || col >= m_cols.size()) return out;
    for (const Comment &c : m_cols[col].comments) {
        QVariantMap m;
        m["id"] = c.id; m["t"] = c.t; m["yFrac"] = c.yFrac; m["text"] = c.text;
        out.append(m);
    }
    return out;
}

int ChartConfigManager::addComment(int col, double t, double yFrac, const QString &text)
{
    if (col < 0 || col >= m_cols.size()) return -1;
    Comment c;
    c.id = m_nextCommentId++;
    c.t = t; c.yFrac = qBound(0.0, yFrac, 1.0); c.text = text;
    m_cols[col].comments.append(c);
    save();
    emit configChanged();
    return c.id;
}

void ChartConfigManager::updateComment(int col, int id, const QString &text)
{
    if (col < 0 || col >= m_cols.size()) return;
    for (Comment &c : m_cols[col].comments) {
        if (c.id != id) continue;
        c.text = text;
        save();
        emit configChanged();
        return;
    }
}

void ChartConfigManager::removeComment(int col, int id)
{
    if (col < 0 || col >= m_cols.size()) return;
    for (int i = 0; i < m_cols[col].comments.size(); ++i) {
        if (m_cols[col].comments[i].id != id) continue;
        m_cols[col].comments.removeAt(i);
        save();
        emit configChanged();
        return;
    }
}

QVariantList ChartConfigManager::infos() const
{
    QVariantList out;
    for (const InfoPanel &ip : m_infos) {
        QVariantMap m;
        m["sensorId"] = ip.sensorId; m["title"] = ip.title; m["unit"] = ip.unit; m["decimals"] = ip.decimals;
        out.append(m);
    }
    return out;
}

void ChartConfigManager::setInfo(int i, int sensorId, const QString &title, const QString &unit, int decimals)
{
    if (i < 0 || i >= m_infos.size()) return;
    m_infos[i].sensorId = sensorId;
    m_infos[i].title = title;
    m_infos[i].unit = unit;
    m_infos[i].decimals = qBound(0, decimals, 6);
    save();
    emit configChanged();
}

void ChartConfigManager::setStandsMode(int m)
{
    if (m < 0) m = 0;
    if (m > 2) m = 2;
    if (m == m_standsMode) return;
    m_standsMode = m;
    save();
    emit configChanged();
}

void ChartConfigManager::setStandsSensorId(int id)
{
    if (id == m_standsSensor) return;
    m_standsSensor = id;
    save();
    emit configChanged();
}

double ChartConfigManager::columnWeight(int col) const
{
    return (col >= 0 && col < m_cols.size()) ? m_cols[col].weight : 1.0;
}
void ChartConfigManager::setColumnWeight(int col, double w)
{
    if (col < 0 || col >= m_cols.size()) return;
    if (!std::isfinite(w)) return;          // защита от Infinity/NaN при drag
    if (w < 0.05) w = 0.05;
    if (w > 10000) w = 10000;
    m_cols[col].weight = w;
    emit configChanged();
}
void ChartConfigManager::moveComment(int col, int id, double t, double yFrac)
{
    if (col < 0 || col >= m_cols.size()) return;
    for (Comment &c : m_cols[col].comments) {
        if (c.id != id) continue;
        c.t = t;
        c.yFrac = qBound(0.0, yFrac, 1.0);
        emit configChanged();
        return;
    }
}
