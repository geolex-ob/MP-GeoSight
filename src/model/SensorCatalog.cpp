#include "SensorCatalog.h"
#include <QFile>
#include <QCoreApplication>
#include <QDebug>
#include <QVector>
#include <QStringList>
#include <QSettings>
#include <QVector>

SensorCatalog::SensorCatalog(QObject *parent) : QObject(parent) {}

void SensorCatalog::load(const QString &iniPath)
{
    QSettings st(iniPath, QSettings::IniFormat);
    for (const QString &g : st.childGroups()) {
        if (!g.startsWith("Sensor_")) continue;
        bool ok = false;
        const uint16_t id = g.mid(7).toUShort(&ok);
        if (!ok || !id) continue;
        st.beginGroup(g);
        Info i;
        i.name = st.value("name", QString("Датчик %1").arg(id)).toString();
        i.unit = st.value("unit").toString();
        i.mn = st.value("min", 0.0).toDouble();
        i.mx = st.value("max", 100.0).toDouble();
        if (i.mx <= i.mn) { i.mn = 0; i.mx = 100; }
        m_info[id] = i;
        st.endGroup();
    }
    emit changed();
}

QString SensorCatalog::name(int id) const
{
    auto it = m_info.constFind(uint16_t(id));
    return it == m_info.constEnd() ? QString("Датчик %1").arg(id) : it->name;
}

QString SensorCatalog::unit(int id) const
{
    auto it = m_info.constFind(uint16_t(id));
    return it == m_info.constEnd() ? QString() : it->unit;
}

double SensorCatalog::defMin(int id) const
{
    auto it = m_info.constFind(uint16_t(id));
    return it == m_info.constEnd() ? 0.0 : it->mn;
}

double SensorCatalog::defMax(int id) const
{
    auto it = m_info.constFind(uint16_t(id));
    return it == m_info.constEnd() ? 100.0 : it->mx;
}

QVariantList SensorCatalog::list() const
{
    QVariantList out;
    for (auto it = m_info.constBegin(); it != m_info.constEnd(); ++it) {
        QVariantMap m;
        m["id"] = int(it.key());
        m["name"] = it->name;
        m["unit"] = it->unit;
        out.append(m);
    }
    return out;
}

void SensorCatalog::registerDiscovered(const QVector<uint16_t> &ids)
{
    bool added = false;
    for (uint16_t id : ids) {
        if (!id || m_info.contains(id)) continue;
        m_info[id] = Info{QString("Параметр %1").arg(id), QString(), 0, 100};
        added = true;
    }
    if (added) emit changed();
}

// ---------- декодирование текста CSV: BOM UTF-8/UTF-16, иначе UTF-8 либо Windows-1251 ----------
static QString decodeCsvText(const QByteArray &raw)
{
    if (raw.startsWith("\xEF\xBB\xBF"))
        return QString::fromUtf8(raw.mid(3));
    if (raw.startsWith("\xFF\xFE"))
        return QString::fromUtf16(reinterpret_cast<const char16_t *>(raw.constData() + 2),
                                  (raw.size() - 2) / 2);
    if (raw.startsWith("\xFE\xFF")) {
        QByteArray sw = raw.mid(2);
        for (int i = 0; i + 1 < sw.size(); i += 2) std::swap(sw[i], sw[i + 1]);
        return QString::fromUtf16(reinterpret_cast<const char16_t *>(sw.constData()), sw.size() / 2);
    }
    // строгая проверка UTF-8
    bool ok = true;
    const unsigned char *d = reinterpret_cast<const unsigned char *>(raw.constData());
    int n = raw.size(), i = 0;
    while (i < n && ok) {
        unsigned char c = d[i];
        int need = 0;
        if (c < 0x80) need = 0;
        else if (c >= 0xC2 && c <= 0xDF) need = 1;
        else if (c >= 0xE0 && c <= 0xEF) need = 2;
        else if (c >= 0xF0 && c <= 0xF4) need = 3;
        else { ok = false; break; }
        if (i + need >= n + 0 && need > 0 && i + need > n - 1) { ok = false; break; }
        for (int k = 1; k <= need; ++k)
            if (d[i + k] < 0x80 || d[i + k] > 0xBF) { ok = false; break; }
        i += need + 1;
    }
    if (ok) return QString::fromUtf8(raw);
    // Windows-1251
    static const ushort t80[64] = {
        0x0402,0x0403,0x201A,0x0453,0x201E,0x2026,0x2020,0x2021,0x20AC,0x2030,0x0409,0x2039,0x040A,0x040C,0x040B,0x040F,
        0x0452,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,0x0098,0x2122,0x0459,0x203A,0x045A,0x045C,0x045B,0x045F,
        0x00A0,0x040E,0x045E,0x0408,0x00A4,0x0490,0x00A6,0x00A7,0x0401,0x00A9,0x0404,0x00AB,0x00AC,0x00AD,0x00AE,0x0407,
        0x00B0,0x00B1,0x0406,0x0456,0x0491,0x00B5,0x00B6,0x00B7,0x0451,0x2116,0x0454,0x00BB,0x0458,0x0405,0x0455,0x0457
    };
    QString out;
    out.reserve(n);
    for (int k = 0; k < n; ++k) {
        unsigned char c = d[k];
        if (c < 0x80) out.append(QChar(c));
        else if (c >= 0xC0) out.append(QChar(0x0410 + (c - 0xC0)));
        else out.append(QChar(t80[c - 0x80]));
    }
    return out;
}

// ---------- разбор CSV с кавычками и автоопределением разделителя ----------
static QVector<QStringList> parseCsvRows(const QString &text)
{
    QString firstLine;
    int nl = text.indexOf('\n');
    firstLine = nl < 0 ? text : text.left(nl);
    int cntSemi = 0, cntComma = 0, cntTab = 0;
    bool q = false;
    for (QChar c : firstLine) {
        if (c == '"') q = !q;
        else if (!q) {
            if (c == ';') cntSemi++;
            else if (c == ',') cntComma++;
            else if (c == '\t') cntTab++;
        }
    }
    QChar delim = ';';
    if (cntComma > cntSemi && cntComma >= cntTab) delim = ',';
    else if (cntTab > cntSemi && cntTab > cntComma) delim = '\t';

    QVector<QStringList> rows;
    QStringList row;
    QString field;
    bool inQ = false;
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text[i];
        if (inQ) {
            if (c == '"') {
                if (i + 1 < text.size() && text[i + 1] == '"') { field += '"'; ++i; }
                else inQ = false;
            } else field += c;
        } else if (c == '"') inQ = true;
        else if (c == delim) { row.append(field.trimmed()); field.clear(); }
        else if (c == '\n' || c == '\r') {
            if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n') ++i;
            row.append(field.trimmed()); field.clear();
            bool empty = true;
            for (const QString &f : row) if (!f.isEmpty()) { empty = false; break; }
            if (!empty) rows.append(row);
            row.clear();
        } else field += c;
    }
    row.append(field.trimmed());
    bool empty = true;
    for (const QString &f : row) if (!f.isEmpty()) { empty = false; break; }
    if (!empty) rows.append(row);
    return rows;
}

bool SensorCatalog::loadCsv(const QString &path)
{
    m_csvName.clear(); m_csvUnit.clear(); m_csvMin.clear(); m_csvMax.clear();
    m_hasCsv = false;
    QFile f(path);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) return false;
    const QByteArray raw = f.readAll();
    f.close();
    const QVector<QStringList> rows = parseCsvRows(decodeCsvText(raw));
    if (rows.isEmpty()) return false;

    int cId = -1, cName = -1, cUnit = -1, cMin = -1, cMax = -1;
    int start = 0;
    const QStringList h = rows.first();
    auto isId = [](const QString &c){ return c == "id" || c.contains("код") || c.contains("номер") ||
                                             c == "№" || c.contains("датчик") || c.contains("sensor") || c.contains("адрес"); };
    auto isName = [](const QString &c){ return c == "name" || c == "title" || c.contains("назв") || c.contains("наимен") ||
                                              c.contains("имя") || c.contains("параметр") || c.contains("описан"); };
    auto isUnit = [](const QString &c){ return c == "unit" || c.contains("ед") || c.contains("изм"); };
    auto isMin = [](const QString &c){ return c == "min" || c.contains("мин"); };
    auto isMax = [](const QString &c){ return c == "max" || c.contains("макс"); };
    bool header = false;
    for (const QString &cell : h) {
        const QString c = cell.trimmed().toLower();
        if (isId(c) || isName(c)) header = true;
    }
    if (header) {
        for (int i = 0; i < h.size(); ++i) {
            const QString c = h[i].trimmed().toLower();
            if (cId < 0 && isId(c)) cId = i;
            else if (cName < 0 && isName(c)) cName = i;
            else if (cUnit < 0 && isUnit(c)) cUnit = i;
            else if (cMin < 0 && isMin(c)) cMin = i;
            else if (cMax < 0 && isMax(c)) cMax = i;
        }
        start = 1;
    } else {
        cId = 0; cName = 1;
        if (h.size() > 2) cUnit = 2;
        if (h.size() > 3) cMin = 3;
        if (h.size() > 4) cMax = 4;
    }
    if (cId < 0) cId = 0;
    if (cName < 0) cName = (cId == 0 ? 1 : 0);

    for (int r = start; r < rows.size(); ++r) {
        const QStringList &f = rows[r];
        if (cId >= f.size()) continue;
        bool ok = false;
        const int id = f[cId].trimmed().toInt(&ok);
        if (!ok || id <= 0) continue;
        if (cName < f.size() && !f[cName].isEmpty()) m_csvName[id] = f[cName];
        if (cUnit >= 0 && cUnit < f.size() && !f[cUnit].isEmpty()) m_csvUnit[id] = f[cUnit];
        if (cMin >= 0 && cMin < f.size()) {
            const double v = f[cMin].trimmed().replace(',', '.').toDouble(&ok);
            if (ok) m_csvMin[id] = v;
        }
        if (cMax >= 0 && cMax < f.size()) {
            const double v = f[cMax].trimmed().replace(',', '.').toDouble(&ok);
            if (ok) m_csvMax[id] = v;
        }
    }
    m_hasCsv = !m_csvName.isEmpty();
    qDebug() << "Sensors.csv:" << path << "— названий загружено:" << m_csvName.size();
    return m_hasCsv;
}

bool SensorCatalog::reloadCsv()
{
    return loadCsv(QCoreApplication::applicationDirPath() + "/Sensors.csv");
}

QString SensorCatalog::displayName(int id) const
{
    return m_csvName.contains(id) ? m_csvName.value(id) : name(id);
}
QString SensorCatalog::displayUnit(int id) const
{
    return m_csvUnit.contains(id) ? m_csvUnit.value(id) : unit(id);
}
double SensorCatalog::displayMin(int id) const
{
    return m_csvMin.contains(id) ? m_csvMin.value(id) : defMin(id);
}
double SensorCatalog::displayMax(int id) const
{
    return m_csvMax.contains(id) ? m_csvMax.value(id) : defMax(id);
}

QVariantList SensorCatalog::csvSensors() const
{
    QVariantList out;
    for (auto it = m_csvName.constBegin(); it != m_csvName.constEnd(); ++it) {
        QVariantMap m;
        m["id"] = it.key();
        m["name"] = it.value();
        m["unit"] = m_csvUnit.value(it.key());
        m["min"] = m_csvMin.contains(it.key()) ? m_csvMin.value(it.key()) : defMin(it.key());
        m["max"] = m_csvMax.contains(it.key()) ? m_csvMax.value(it.key()) : defMax(it.key());
        out.append(m);
    }
    return out;
}
