import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Popup {
    id: dlg
    width: 430; height: 470
    modal: true; focus: true
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape

    property bool editMode: false
    property int colIndex: 0
    property int traceId: 0
    property int sensorId: 200
    property string titleStr: ""
    property string colorStr: "#0000CC"
    property real minV: 0
    property real maxV: 100

    function openAdd(col) {
        editMode = false; colIndex = col; traceId = 0; sensorId = 200;
        titleStr = sensorCatalog.displayName(200); colorStr = "#0000CC";
        minV = sensorCatalog.displayMin(200); maxV = sensorCatalog.displayMax(200);
        open();
    }
    function openEdit(col, t) {
        editMode = true; colIndex = col; traceId = t.id; sensorId = t.sensorId;
        titleStr = t.title; colorStr = t.color; minV = t.min; maxV = t.max;
        open();
    }
    function apply() {
        var mn = parseFloat(minField.text), mx = parseFloat(maxField.text);
        if (isNaN(mn)) mn = 0;
        if (isNaN(mx) || mx <= mn) mx = mn + 1;
        if (editMode) chartManager.updateTrace(colIndex, traceId, sensorId, titleStr, colorStr, mn, mx);
        else          chartManager.addTrace(colIndex, sensorId, titleStr, colorStr, mn, mx);
        close();
    }

    onAboutToShow: { titleField.text = dlg.titleStr; minField.text = dlg.minV; maxField.text = dlg.maxV }

    background: Rectangle { color: "#FAFAFA"; border.color: "#888888"; radius: 6 }

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 16; spacing: 8
        Label { text: editMode ? "Параметр колонки" : "Новый параметр"; font.pixelSize: 16; font.bold: true }

        RowLayout {
            Label { text: "Датчик:"; Layout.preferredWidth: 80 }
            SpinBox { from: 1; to: 65535; value: dlg.sensorId; onValueModified: dlg.sensorId = value }
            Button { text: "Имя из каталога"
                     onClicked: { dlg.titleStr = sensorCatalog.displayName(dlg.sensorId)
                                  dlg.minV = sensorCatalog.displayMin(dlg.sensorId)
                                  dlg.maxV = sensorCatalog.displayMax(dlg.sensorId)
                                  titleField.text = dlg.titleStr; minField.text = dlg.minV; maxField.text = dlg.maxV } }
        }
        RowLayout {
            Label { text: "Название:"; Layout.preferredWidth: 80 }
            TextField { id: titleField; Layout.fillWidth: true; onTextChanged: dlg.titleStr = text }
        }
        Label { text: "Цвет:" }
        Flow {
            spacing: 5; Layout.fillWidth: true
            Repeater {
                model: ["#CC0000","#0000CC","#00AA00","#CC00CC","#CC6600","#0088FF",
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
            TextField { id: minField; Layout.preferredWidth: 90 }
            Label { text: "Макс:" }
            TextField { id: maxField; Layout.preferredWidth: 90 }
            Label { text: "Колонка:" }
            SpinBox { from: 0; to: Math.max(0, chartManager.rowCount() - 1); value: dlg.colIndex
                      onValueModified: dlg.colIndex = value }
        }
        Item { Layout.fillHeight: true }
        RowLayout {
            Button { text: "Убрать"; visible: dlg.editMode
                     onClicked: { chartManager.removeTrace(dlg.colIndex, dlg.traceId); dlg.close() } }
            Item { Layout.fillWidth: true }
            Button { text: "Отмена"; onClicked: dlg.close() }
            Button { text: dlg.editMode ? "Сохранить" : "Добавить"; onClicked: dlg.apply() }
        }
    }
}
