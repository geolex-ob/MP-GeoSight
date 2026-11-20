#include "NetworkManager.h"
#include <QSettings>
#include <QCoreApplication>
#include <QDebug>

static const QByteArray POLL = QByteArray(1, char(0x44));
static const QList<int> PORTS = {253, 254, 255};

static QString settingsPath()
{
    return QCoreApplication::applicationDirPath() + "/settings.ini";
}

NetworkManager::NetworkManager(QObject *parent) : QObject(parent)
{
    QSettings st(settingsPath(), QSettings::IniFormat);
    m_serverIP = st.value("connection/ip", "192.168.121.1").toString();
    m_autoConnect = st.value("connection/auto", true).toBool();

    m_heartbeat.setInterval(1000);
    connect(&m_heartbeat, &QTimer::timeout, this, &NetworkManager::onHeartbeat);
    m_reconnect.setInterval(3000);
    connect(&m_reconnect, &QTimer::timeout, this, &NetworkManager::tryReconnect);
}

void NetworkManager::setServerIP(const QString &ip)
{
    const QString v = ip.trimmed();
    if (v.isEmpty() || v == m_serverIP) return;
    m_serverIP = v;
    QSettings st(settingsPath(), QSettings::IniFormat);
    st.setValue("connection/ip", v);
    emit serverIPChanged();
}

void NetworkManager::setAutoConnect(bool on)
{
    if (on == m_autoConnect) return;
    m_autoConnect = on;
    QSettings st(settingsPath(), QSettings::IniFormat);
    st.setValue("connection/auto", on);
    emit autoConnectChanged();
}

void NetworkManager::connectToServer()
{
    m_manualDisconnect = false;
    m_reconnect.stop();
    m_firstLogged.clear();
    qDebug() << "Подключение к" << m_serverIP << "порты 253/254/255";
    for (int port : PORTS) {
        if (m_sockets.contains(port)) {
            QTcpSocket *old = m_sockets.take(port);
            old->abort();
            old->deleteLater();
        }
        QTcpSocket *s = new QTcpSocket(this);
        s->setProperty("port", port);
        connect(s, &QTcpSocket::connected, this, &NetworkManager::onConnected);
        connect(s, &QTcpSocket::disconnected, this, &NetworkManager::onDisconnected);
        connect(s, &QTcpSocket::readyRead, this, &NetworkManager::onReadyRead);
        connect(s, &QTcpSocket::errorOccurred, this, &NetworkManager::onError);
        m_sockets.insert(port, s);
        m_buffers[port].clear();
        s->connectToHost(m_serverIP, quint16(port));
    }
    if (!m_heartbeat.isActive()) m_heartbeat.start();
}

void NetworkManager::disconnectFromServer()
{
    m_manualDisconnect = true;
    m_reconnect.stop();
    const auto socks = m_sockets.values();
    m_sockets.clear();
    for (QTcpSocket *s : socks) {
        s->abort();
        s->deleteLater();
    }
    updateConnected();
}

void NetworkManager::reconnect()
{
    disconnectFromServer();
    QTimer::singleShot(300, this, [this]{ connectToServer(); });
}

void NetworkManager::sendPoll(QTcpSocket *s)
{
    if (s && s->state() == QAbstractSocket::ConnectedState) {
        s->write(POLL);
        m_polls[s->property("port").toInt()]++;
    }
}

void NetworkManager::onConnected()
{
    auto *s = qobject_cast<QTcpSocket*>(sender());
    if (!s) return;
    const int port = s->property("port").toInt();
    qDebug() << "Порт" << port << "TCP подключен, шлю запрос 0x44";
    sendPoll(s);
    m_reconnect.stop();
    updateConnected();
}

void NetworkManager::onDisconnected()
{
    auto *s = qobject_cast<QTcpSocket*>(sender());
    if (!s) return;
    const int port = s->property("port").toInt();
    qDebug() << "Порт" << port << "отключен";
    m_sockets.remove(port);
    s->deleteLater();
    updateConnected();
    if (!m_manualDisconnect && !m_reconnect.isActive()) m_reconnect.start();
}

void NetworkManager::onError(QAbstractSocket::SocketError)
{
    auto *s = qobject_cast<QTcpSocket*>(sender());
    if (!s) return;
    qDebug() << "Порт" << s->property("port").toInt() << "ошибка:" << s->errorString();
    if (!m_manualDisconnect && !m_connected && !m_reconnect.isActive()) m_reconnect.start();
}

void NetworkManager::onReadyRead()
{
    auto *s = qobject_cast<QTcpSocket*>(sender());
    if (!s) return;
    const int port = s->property("port").toInt();
    m_buffers[port].append(s->readAll());
    QByteArray &buf = m_buffers[port];
    while (buf.size() >= 2) {
        const quint16 len = quint16(quint8(buf[0])) | (quint16(quint8(buf[1])) << 8);
        if (len < 2 || buf.size() < int(len)) break;
        const QByteArray frame = buf.left(int(len));
        buf.remove(0, int(len));
        if (!m_firstLogged.value(port)) {
            m_firstLogged[port] = true;
            qDebug() << "Порт" << port << "первые" << frame.size() << "байт:" << frame.left(16).toHex(' ');
        }
        if (frame.size() > 100) m_data[port]++;
        else if (frame.size() >= 3) {
            m_idle[port]++;
            if (port == 255 && m_data.value(255) > 10 && !m_liveAnnounced) {
                m_liveAnnounced = true;
                qDebug() << "Порт 255: архив слит, переходим в живой режим";
            }
        }
        emit dataReceived(port, frame);
    }
    // живой поток забирает тот, кто опрашивает чаще: на 255 опрос сразу после любого ответа
    if (port == 255)
        QTimer::singleShot(1, this, [this]{ sendPoll(m_sockets.value(255, nullptr)); });
}

void NetworkManager::onHeartbeat()
{
    for (QTcpSocket *s : m_sockets) sendPoll(s);
}

void NetworkManager::tryReconnect()
{
    if (!m_manualDisconnect && !m_connected) connectToServer();
}

void NetworkManager::updateConnected()
{
    bool any = false;
    for (QTcpSocket *s : m_sockets)
        if (s->state() == QAbstractSocket::ConnectedState) { any = true; break; }
    if (any != m_connected) {
        m_connected = any;
        emit connectionChanged();
    }
}

QString NetworkManager::netStats() const
{
    QString out;
    for (int port : PORTS)
        out += QString("port%1: poll=%2 data=%3 idle=%4  ")
                   .arg(port).arg(m_polls.value(port)).arg(m_data.value(port)).arg(m_idle.value(port));
    return out;
}
