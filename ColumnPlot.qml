import QtQuick 2.15
import QtQuick.Layouts 1.15
import GeoSight 1.0

Rectangle {
    property var traces: []
    color: "white"
    border.color: "#999999"

    GeoChart {
        anchors.fill: parent
        anchors.margins: 1
        store: dataStore
        traces: parent.traces
        windowSeconds: chartManager.timeWindow
    }
}
