import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: hdr
    property int colIndex: 0
    property var traces: []

    color: "#FAFAFA"
    border.color: "#999999"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 2
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 22
            Button {
                text: "+ параметр"
                font.pixelSize: 9
                Layout.preferredHeight: 20
                onClicked: traceDialog.openAdd(hdr.colIndex)
            }
            Item { Layout.fillWidth: true }
            Button {
                text: "× колонка"
                font.pixelSize: 9
                Layout.preferredHeight: 20
                onClicked: colCloseConfirm.open()
            }
        }

        Repeater {
            model: hdr.traces
            delegate: Rectangle {
                required property var modelData
                Layout.fillWidth: true
                Layout.preferredHeight: 26
                color: "transparent"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0
                    Text {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 14
                        font.pixelSize: 10
                        font.bold: true
                        color: modelData.color
                        text: modelData.title + " = " +
                              Number(dataStore.value(modelData.sensorId)).toFixed(2) + " " +
                              sensorCatalog.displayUnit(modelData.sensorId)
                        elide: Text.ElideRight
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 10
                        spacing: 0
                        Repeater {
                            model: 5
                            delegate: Text {
                                required property int index
                                Layout.fillWidth: true
                                font.pixelSize: 7
                                color: "#888888"
                                horizontalAlignment: index === 0 ? Text.AlignLeft :
                                                     index === 4 ? Text.AlignRight : Text.AlignHCenter
                                text: (modelData.min + (modelData.max - modelData.min) * index / 4).toFixed(1)
                            }
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onDoubleClicked: traceDialog.openEdit(hdr.colIndex, modelData)
                    onClicked: function(m) { if (m.button === Qt.RightButton) rowMenu.popup() }
                }
                Menu {
                    id: rowMenu
                    MenuItem { text: "Изменить…"; onTriggered: traceDialog.openEdit(hdr.colIndex, modelData) }
                    MenuItem { text: "Убрать"; onTriggered: chartManager.removeTrace(hdr.colIndex, modelData.id) }
                }
            }
        }
        Item { Layout.fillHeight: true }
    }

    Dialog {
        id: colCloseConfirm
        title: "Удалить колонку"
        modal: true
        standardButtons: Dialog.Yes | Dialog.No
        anchors.centerIn: Overlay.overlay
        contentItem: Text {
            text: "Удалить колонку " + (hdr.colIndex + 1) + " вместе со всеми её параметрами?"
            font.pixelSize: 13
            color: "#33424F"
        }
        onAccepted: chartManager.removeColumn(hdr.colIndex)
    }
}