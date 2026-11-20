import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Popup {
    id: dlg
    width: 430; height: 470
    modal: true; focus: true
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape
    onAboutToShow: { titleField.text = dlg.titleStr; minField.text = dlg.minV; maxField.text = dlg.maxV }

    property bool editMode: false
    property int chartId: 0
    property int sensorId: 200
    property string titleStr: ""
    property string colorStr: "#00AA00"
    property real minV: 0
    property real maxV: 100
    property int colV: 0

    function openAdd() {
        editMode = false; chartId = 0; sensorId = 200;
        titleStr = sensorCatalog.displayName(200); colorStr = "#CC00CC";
        minV = sensorCatalog.displayMin(200); maxV = sensorCatalog.displayMax(200); colV = 0;
        open();
    }
    function openEdit(id, sid, t, c, mn, mx) {
        editMode = true; chartId = id; sensorId = sid; titleStr = t; colorStr = c;
        minV = mn; maxV = mx; colV = 0;
        open();
    }
    function apply() {
        var mn = parseFloat(minField.text), mx = parseFloat(maxField.text);
        if (isNaN(mn)) mn = 0;
        if (isNaN(mx) || mx <= mn) mx = mn + 1;
        if (editMode) chartManager.updateChart(chartId, sensorId, titleStr, colorStr, mn, mx, colV);
        else          chartManager.addChart(sensorId, titleStr, colorStr, mn, mx, colV);
        close();
    }

    background: Rectangle { color: "#FAFAFA"; border.color: "#888888"; radius: 6 }

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 16; spacing: 8

        Label { text: editMode ? "Настройка графика" : "Новый график"
                font.pixelSize: 16; font.bold: true }

        RowLayout {
            Label { text: "Датчик:"; Layout.preferredWidth: 80 }
            SpinBox { from: 1; to: 65535; value: dlg.sensorId
                      onValueModified: dlg.sensorId = value }
            Button { text: "Имя из каталога"
                     onClicked: { dlg.titleStr = sensorCatalog.displayName(dlg.sensorId)
                                  dlg.minV = sensorCatalog.displayMin(dlg.sensorId)
                                  dlg.maxV = sensorCatalog.displayMax(dlg.sensorId) } }
        }

        RowLayout {
            Label { text: "Название:"; Layout.preferredWidth: 80 }
            TextField { id: titleField; text: dlg.titleStr; onTextChanged: dlg.titleStr = text
                        Layout.fillWidth: true }
        }

        Label { text: "Цвет:" }
        Flow {
            spacing: 5; Layout.fillWidth: true
            Repeater {
                model: ["#CC00CC","#0000CC","#CC0000","#00AA00","#CC6600","#0088FF",
                        "#8800AA","#00AAAA","#AA0066","#666666","#000000","#FF6600"]
                Rectangle {
                    width: 28; height: 28; radius: 3; color: modelData
                    border.color: dlg.colorStr === modelData ? "#000000" : "#BBBBBB"
                    border.width: dlg.colorStr === modelData ? 2 : 1
                    MouseArea { anchors.fill: parent; onClicked: dlg.colorStr = modelData }
                }
            }
        }

        RowLayout {
            Label { text: "Мин:" }
            TextField { id: minField; text: dlg.minV; Layout.preferredWidth: 90 }
            Label { text: "Макс:" }
            TextField { id: maxField; text: dlg.maxV; Layout.preferredWidth: 90 }
            Label { text: "Колонка:" }
            SpinBox { from: 0; to: chartManager.columns - 1; value: dlg.colV
                      onValueModified: dlg.colV = value }
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Button { text: "Удалить"; visible: dlg.editMode
                     onClicked: { chartManager.removeChart(dlg.chartId); dlg.close() } }
            Item { Layout.fillWidth: true }
            Button { text: "Отмена"; onClicked: dlg.close() }
            Button { text: dlg.editMode ? "Сохранить" : "Добавить"; onClicked: dlg.apply() }
        }
    }
}
