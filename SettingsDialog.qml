import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Dialog {
    id: dlg
    title: "Настройки подключения"
    modal: true
    width: 470
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
        Text {
            anchors.centerIn: parent
            text: dlg.title
            color: "#FFFFFF"
            font.pixelSize: 15
            font.bold: true
        }
    }
    background: Rectangle { color: "#FFFFFF"; border.color: dlg.win.line; radius: 8 }

    contentItem: ColumnLayout {
        spacing: 12

        Label {
            text: "IP-адрес удалённого ПК (сервер GeoSight):"
            color: dlg.win.ink
            font.pixelSize: 13
            font.bold: true
        }
        TextField {
            id: ipField
            Layout.fillWidth: true
            font.pixelSize: 15
            font.family: "Monospace"
            color: dlg.win.ink
            placeholderText: "192.168.121.1"
            validator: RegularExpressionValidator {
                regularExpression: /^(\d{1,3}\.){3}\d{1,3}$/
            }
            background: Rectangle {
                radius: 6
                color: ipField.activeFocus ? "#FDF8EC" : "#FFFFFF"
                border.color: ipField.activeFocus ? dlg.win.accent : dlg.win.line
                border.width: ipField.activeFocus ? 2 : 1
            }
        }
        Label {
            id: errLabel
            color: dlg.win.bad
            font.pixelSize: 12
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        CheckBox {
            id: autoChk
            text: "Подключаться автоматически при запуске"
            font.pixelSize: 13
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Label { text: "Датчик забоя:"; color: dlg.win.ink; font.pixelSize: 12 }
            SpinBox {
                from: 1; to: 65535
                value: chartManager.depthSensorId
                onValueModified: chartManager.depthSensorId = value
                Layout.preferredWidth: 110
            }
            Label { text: "Датчик долота:"; color: dlg.win.ink; font.pixelSize: 12 }
            SpinBox {
                from: 1; to: 65535
                value: chartManager.bitSensorId
                onValueModified: chartManager.bitSensorId = value
                Layout.preferredWidth: 110
            }
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 74
            radius: 6
            color: "#F2F5F7"
            border.color: dlg.win.line
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 4
                Label { text: "TCP-порты: 253, 254, 255 (фиксированы протоколом)"; color: dlg.win.inkSoft; font.pixelSize: 12 }
                Label { text: "Адрес и режим автоподключения сохраняются в settings.ini"; color: "#6B7A88"; font.pixelSize: 11 }
                Label { text: "рядом с исполняемым файлом программы."; color: "#6B7A88"; font.pixelSize: 11 }
            }
        }
    }

    footer: DialogButtonBox {
        Button {
            text: "Сохранить и подключиться"
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            font.pixelSize: 13
            font.bold: true
            contentItem: Text {
                text: parent.text
                color: "#FFFFFF"
                font: parent.font
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: 6
                color: parent.down ? dlg.win.ink : dlg.win.inkSoft
                implicitHeight: 34
                implicitWidth: 190
            }
            onClicked: {
                if (!ipField.acceptableInput || ipField.text.trim() === "") {
                    errLabel.text = "Неверный формат IP-адреса. Пример: 192.168.121.1"
                    return
                }
                networkManager.serverIP = ipField.text
                networkManager.autoConnect = autoChk.checked
                networkManager.reconnect()
                dlg.win.toast("Подключение к " + ipField.text + " …")
                dlg.close()
            }
        }
        Button {
            text: "Отмена"
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            font.pixelSize: 13
        }
    }
}
