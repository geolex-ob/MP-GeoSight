import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Dialog {
    id: dlg
    title: "Настройки подключения и отображения"
    modal: true
    width: 800
    height: 700
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape

    readonly property var win: ApplicationWindow.window

    onAboutToShow: {
        ipField.text = networkManager.serverIP
        autoChk.checked = networkManager.autoConnect
        errLabel.text = ""
    }

    header: Rectangle {
        implicitHeight: 46
        color: dlg.win.ink
        radius: 8
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 8; color: dlg.win.ink }
        Text { anchors.centerIn: parent; text: dlg.title; color: "#FFFFFF"; font.pixelSize: 15; font.bold: true }
    }
    background: Rectangle { color: "#FFFFFF"; border.color: dlg.win.line; radius: 8 }

    contentItem: Flickable {
        clip: true
        contentHeight: settingsCol.implicitHeight + 24
        flickableDirection: Flickable.VerticalFlick
        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            background: Rectangle { color: "#EDF0F2"; radius: 4 }
            contentItem: Rectangle { implicitWidth: 8; radius: 4; color: dlg.win.inkSoft }
        }
        ColumnLayout {
            Row {
                spacing: 10
                Label { text: "Вид метки:"; font.pixelSize: 12; color: "#12314E"; anchors.verticalCenter: parent.verticalCenter }
                RadioButton { text: "Метка";   checked: !root.rulerMode; onClicked: root.rulerMode = false }
                RadioButton { text: "Линейка"; checked:  root.rulerMode; onClicked: root.rulerMode = true }
            }
            Label {
                text: "Линейка ставит метки сразу на всех параметрах всех колонок."
                font.pixelSize: 10; color: "#8894A0"; wrapMode: Text.Wrap
                width: parent ? parent.width : 300
            }

            id: settingsCol
            width: parent.width
            spacing: 12

            Label { text: "IP-адрес удалённого ПК (сервер GeoSight):"; color: dlg.win.ink; font.pixelSize: 13; font.bold: true }
            TextField {
                id: ipField
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                font.pixelSize: 14
                font.family: "Monospace"
                color: dlg.win.ink
                validator: RegularExpressionValidator { regularExpression: /^(\d{1,3}\.){3}\d{1,3}$/ }
                background: Rectangle {
                    radius: 6
                    color: ipField.activeFocus ? "#FDF8EC" : "#FFFFFF"
                    border.color: ipField.activeFocus ? dlg.win.accent : dlg.win.line
                }
            }
            Label { id: errLabel; text: ""; color: dlg.win.bad; font.pixelSize: 11 }
            CheckBox {
                id: autoChk
                text: "Подключаться автоматически при запуске"
                font.pixelSize: 13
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: sensorsCol.implicitHeight + 20
                radius: 6
                color: "#FBFCFD"
                border.color: dlg.win.line
                ColumnLayout {
                    id: sensorsCol
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10
                    Label { text: "Датчики и шкалы"; color: dlg.win.ink; font.pixelSize: 13; font.bold: true }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Label { text: "Датчик забоя:"; color: dlg.win.ink; font.pixelSize: 12 }
                        SpinBox { from: 1; to: 65535; value: chartManager.depthSensorId
                                  onValueModified: chartManager.depthSensorId = value
                                  Layout.preferredWidth: 110; editable: true }
                        Label { text: "Датчик долота:"; color: dlg.win.ink; font.pixelSize: 12 }
                        SpinBox { from: 1; to: 65535; value: chartManager.bitSensorId
                                  onValueModified: chartManager.bitSensorId = value
                                  Layout.preferredWidth: 110; editable: true }
                        Item { Layout.fillWidth: true }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Label { text: "Датчик свечей:"; color: dlg.win.ink; font.pixelSize: 12 }
                        SpinBox { from: 0; to: 65535; value: chartManager.standsSensorId
                                  onValueModified: chartManager.standsSensorId = value
                                  Layout.preferredWidth: 110; editable: true }
                        Label { text: "Длина свечи, м:"; color: dlg.win.ink; font.pixelSize: 12 }
                        SpinBox { from: 1; to: 100; value: chartManager.standLength
                                  onValueModified: chartManager.standLength = value
                                  Layout.preferredWidth: 100; editable: true }
                        Item { Layout.fillWidth: true }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Label { text: "Шрифт шапки колонок, px:"; color: dlg.win.ink; font.pixelSize: 12 }
                        SpinBox { from: 6; to: 24; value: chartManager.headerFontPx
                                  onValueModified: chartManager.headerFontPx = value
                                  Layout.preferredWidth: 100; editable: true }
                        Label { text: "(6–24 px, применяется сразу)"; color: "#8894A0"; font.pixelSize: 11 }
                        Item { Layout.fillWidth: true }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 74
                radius: 6
                color: "#F2F5F7"
                border.color: dlg.win.line
                Column {
                    anchors.centerIn: parent
                    spacing: 4
                    Text { text: "TCP-порты: 253, 254, 255 (фиксированы протоколом GeoSight)"; color: "#55636F"; font.pixelSize: 11 }
                    Text { text: "Опрос 0x44, кадры с префиксом длины LE16"; color: "#8894A0"; font.pixelSize: 10 }
                }
            }

            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: "#6B7A88"
                font.pixelSize: 11
                text: "Адрес и флаг автоподключения сохраняются в settings.ini, датчики/шрифт — в charts.json рядом с программой."
            }
        }
    }

    footer: DialogButtonBox {
        padding: 10
        Button {
            text: "Сохранить и подключиться"
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            font.pixelSize: 13
            font.bold: true
            contentItem: Text { text: parent.text; color: "#FFFFFF"; font: parent.font
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            background: Rectangle { radius: 6; implicitHeight: 34; implicitWidth: 190
                                    color: parent.down ? dlg.win.ink : dlg.win.inkSoft }
            onClicked: {
                if (!ipField.acceptableInput) { errLabel.text = "Неверный формат IP-адреса"; return }
                networkManager.serverIP = ipField.text.trim()
                networkManager.autoConnect = autoChk.checked
                networkManager.reconnect()
                dlg.win.toast("Подключение к " + ipField.text.trim() + " …")
                dlg.close()
            }
        }
        Button { text: "Отмена"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole; font.pixelSize: 13 }
    }
}
