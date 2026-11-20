import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Dialog {
    id: dlg
    title: "О программе"
    modal: true
    width: 560
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape

    readonly property var win: ApplicationWindow.window

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

        RowLayout {
            spacing: 14
            Rectangle {
                Layout.preferredWidth: 62
                Layout.preferredHeight: 62
                radius: 12
                gradient: Gradient {
                    GradientStop { position: 0.0; color: dlg.win.inkSoft }
                    GradientStop { position: 1.0; color: dlg.win.ink }
                }
                Text {
                    anchors.centerIn: parent
                    text: "GTI"
                    color: "#FFFFFF"
                    font.pixelSize: 22
                    font.bold: true
                }
            }
            ColumnLayout {
                spacing: 2
                Label { text: "MP GeoSight"; color: dlg.win.ink; font.pixelSize: 22; font.bold: true }
                Label { text: "Версия " + appVersion + "  ·  сборка " + buildDate; color: "#6B7A88"; font.pixelSize: 12 }
                Label { text: "Мониторинг параметров ГТИ"; color: dlg.win.inkSoft; font.pixelSize: 13 }
            }
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: "#33424F"
            font.pixelSize: 13
            text: "Приём данных в реальном времени от СГТИ Разрез-2 на Geoscape v1. " +
                  "Совмещённые графики с индивидуальными масштабами и цветами, общая шкала времени и глубины, " +
                  "Буфер хранения — до 96 часов по всем датчикам."
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 108
            radius: 6
            color: "#F2F5F7"
            border.color: dlg.win.line
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 4
                Label { text: "Сервер: " + networkManager.serverIP + ""; color: dlg.win.ink; font.pixelSize: 12; font.family: "Monospace" }
                Label {
                    text: networkManager.connected ? "Состояние: подключено" : "Состояние: нет связи"
                    color: networkManager.connected ? dlg.win.ok : dlg.win.bad
                    font.pixelSize: 12; font.bold: true
                }
                Label { text: "Настройки отображения: charts.json (колонки, параметры, цвета, диапазоны)"; color: "#6B7A88"; font.pixelSize: 11 }
                Label { text: "Настройки подключения: settings.ini (IP, автоподключение)"; color: "#6B7A88"; font.pixelSize: 11 }
            }
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: "#6B7A88"
            font.pixelSize: 12
            text: "Управление: двойной клик по графику — метка со значением; правый клик по метке — удалить; " +
                  "колесо мыши — просмотр архива; «К живому» — возврат к текущему времени; " +
                  "клик по строке параметра в шапке — выбор параметра."
        }

        Label { text: "© 2026 · Qt Quick / C++"; color: "#9AA7B2"; font.pixelSize: 11 }
    }

    footer: DialogButtonBox {
        Button {
            text: "Закрыть"
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            font.pixelSize: 13
        }
    }
}
