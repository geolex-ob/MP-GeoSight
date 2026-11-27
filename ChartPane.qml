import QtQuick 2.15
import QtQuick.Controls 2.15
import GeoSight 1.0

Rectangle {
    id: pane
    property int chartId: 0
    property int sensorId: 0
    property string chartTitle: ""
    property string chartColor: "#00AA00"
    property real minV: 0
    property real maxV: 100

    color: "white"
    border.color: "#999999"

    GeoChart {
        anchors.fill: parent
        anchors.margins: 1
        store: dataStore
        sensorId: pane.sensorId
        title: pane.chartTitle
        unit: sensorCatalog.displayUnit(pane.sensorId)
        lineColor: pane.chartColor
        minValue: pane.minV
        maxValue: pane.maxV
        windowSeconds: chartManager.timeWindow
    }

    MouseArea {
        id: hover
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onDoubleClicked: chartDialog.openEdit(pane.chartId, pane.sensorId, pane.chartTitle,
                                              pane.chartColor, pane.minV, pane.maxV)
        onClicked: function(m) { if (m.button === Qt.RightButton) menu.popup() }
    }

    Rectangle {
        anchors.top: parent.top; anchors.right: parent.right
        width: 22; height: 22; radius: 2
        color: "#CC0000"
        opacity: hover.containsMouse ? 0.9 : 0.0
        Behavior on opacity { NumberAnimation { duration: 120 } }
        Text { anchors.centerIn: parent; text: "×"; color: "white"; font.bold: true }
        MouseArea { anchors.fill: parent; onClicked: chartManager.removeChart(pane.chartId) }
    }

    Menu {
        id: menu
        width: 300
		MenuItem {
            text: "Настроить…"
            onTriggered: chartDialog.openEdit(pane.chartId, pane.sensorId, pane.chartTitle,
                                              pane.chartColor, pane.minV, pane.maxV)
        }
        MenuItem { text: "Удалить"; onTriggered: chartManager.removeChart(pane.chartId) }
    }
}
