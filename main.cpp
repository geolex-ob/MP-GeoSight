#include <QGuiApplication>
#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQml>
#include <QTimer>

#include "core/NetworkManager.h"
#include "core/GtiProtocolParser.h"
#include "core/DataStore.h"
#include "model/ChartConfigManager.h"
#include "model/SensorCatalog.h"
#include "ui/GeoChartItem.h"
#include "ui/TimeAxisItem.h"

int main(int argc, char *argv[])
{
    if (!qEnvironmentVariableIsSet("GTI_THREADED"))
        qputenv("QSG_RENDER_LOOP", "basic");

    if (qEnvironmentVariableIsSet("GTI_SOFTWARE"))
        qputenv("QT_QUICK_BACKEND", "software");

    QGuiApplication app(argc, argv);
    qmlRegisterType<GeoChartItem>("GeoSight", 1, 0, "GeoChart");
    qmlRegisterType<TimeAxisItem>("GeoSight", 1, 0, "TimeAxis");

    SensorCatalog catalog;
    catalog.load(QCoreApplication::applicationDirPath() + "/sensors.ini");

    ChartConfigManager charts;
    DataStore store;
    GtiProtocolParser parser;
    NetworkManager network;

    QObject::connect(&network, &NetworkManager::dataReceived,
                     [&parser](int port, const QByteArray &d){ parser.feedData(d, port); });
    QObject::connect(&parser, &GtiProtocolParser::configReceived,
                     [&catalog](int, const QVector<uint16_t> &ids){ catalog.registerDiscovered(ids); });
    QObject::connect(&parser, &GtiProtocolParser::dataParsed,
                     [&store](int port, const QMap<uint16_t,double> &v, double ts) {
                         if (port != 255) return;
                         static double lastTs = 0;
                         if (lastTs > 0 && (ts - lastTs) * 86400.0 > 10.0)
                             qDebug() << "pause ended: new scan, server time:" << store.serverTimeString();
                         lastTs = ts;
                         store.ingest(v, ts);
                     });

    QTimer *stat = new QTimer(&app);
        stat->setInterval(5000);
        QObject::connect(stat, &QTimer::timeout, [&store, &network]{
            qDebug() << "STATUS sensors:"
                     << "101=" << store.value(101)
                     << "200=" << store.value(200)
                     << "300=" << store.value(300)
                     << "600=" << store.value(600)
                     << "711=" << store.value(711)
                     << "720=" << store.value(720)
                     << "| server time:" << store.serverTimeString()
                     << "| net:" << network.netStats();
        });
        stat->start();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("networkManager", &network);
    engine.rootContext()->setContextProperty("chartManager", &charts);
    engine.rootContext()->setContextProperty("sensorCatalog", &catalog);
    catalog.loadCsv(QCoreApplication::applicationDirPath() + "/Sensors.csv");
    engine.rootContext()->setContextProperty("dataStore", &store);
    engine.rootContext()->setContextProperty("appVersion", QStringLiteral("0.1.3"));
    engine.rootContext()->setContextProperty("buildDate", QStringLiteral(__DATE__));

    engine.load(QUrl("qrc:/main.qml"));
    if (engine.rootObjects().isEmpty()) return 1;

    engine.rootContext()->setContextProperty("appVersion", QStringLiteral("0.1.3"));
    engine.rootContext()->setContextProperty("buildDate", QStringLiteral(__DATE__));
    if (network.autoConnect())
        network.connectToServer();

    return app.exec();
}
