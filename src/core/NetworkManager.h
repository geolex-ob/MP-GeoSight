#pragma once
#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QMap>
#include <QByteArray>
#include <QList>

class NetworkManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectionChanged)
    Q_PROPERTY(QString serverIP READ serverIP WRITE setServerIP NOTIFY serverIPChanged)
    Q_PROPERTY(bool autoConnect READ autoConnect WRITE setAutoConnect NOTIFY autoConnectChanged)

public:
    explicit NetworkManager(QObject *parent = nullptr);

    bool connected() const { return m_connected; }
    QString serverIP() const { return m_serverIP; }
    void setServerIP(const QString &ip);
    bool autoConnect() const { return m_autoConnect; }
    void setAutoConnect(bool on);

    Q_INVOKABLE void connectToServer();
    Q_INVOKABLE void disconnectFromServer();
    Q_INVOKABLE void reconnect();

    QString netStats() const;

signals:
    void connectionChanged();
    void serverIPChanged();
    void autoConnectChanged();
    void dataReceived(int port, const QByteArray &data);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError);
    void onHeartbeat();
    void tryReconnect();

private:
    void sendPoll(QTcpSocket *s);
    void updateConnected();

    QString m_serverIP;
    bool m_autoConnect = true;
    bool m_connected = false;
    bool m_manualDisconnect = false;
    QMap<int, QTcpSocket*> m_sockets;
    QMap<int, QByteArray> m_buffers;
    QMap<int, bool> m_firstLogged;
    QTimer m_heartbeat;
    QTimer m_reconnect;
    QMap<int, qint64> m_polls, m_data, m_idle;
    bool m_liveAnnounced = false;
};
