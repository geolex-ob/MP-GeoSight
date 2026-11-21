import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import GeoSight 1.0

Dialog {
    id: dlg
    title: editMode ? "Параметр колонки" : "Новый параметр"
    modal: true
    width: 700
    height: 680
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape

    readonly property var win: ApplicationWindow.window

    property bool editMode: false
    property int colIndex: 0
    property int traceId: 0
    property int sensorId: 200
    property string titleStr: ""
    property string colorStr: "#0033CC"
    property string unitStr: ""
    property real minV: 0
    property real maxV: 100
    property int tick: 0
    property var fullList: []
    property var shownList: []

    Timer { interval: 1000; running: dlg.visible; repeat: true; onTriggered: dlg.tick++ }

    function lum(hex) {
        var r = parseInt(hex.substr(1, 2), 16) / 255
        var g = parseInt(hex.substr(3, 2), 16) / 255
        var b = parseInt(hex.substr(5, 2), 16) / 255
        return 0.2126 * r + 0.7152 * g + 0.0722 * b
    }
    function refreshList() {
        fullList = sensorCatalog.csvSensors()
        filterList()
    }
    function filterList() {
        var q = searchField.text.trim().toLowerCase()
        if (q === "") { shownList = fullList; return }
        var out = []
        for (var i = 0; i < fullList.length; ++i) {
            var s = fullList[i]
            if (String(s.id).indexOf(q) >= 0 ||
                s.name.toLowerCase().indexOf(q) >= 0 ||
                String(s.unit || "").toLowerCase().indexOf(q) >= 0) out.push(s)
        }
        shownList = out
    }
    function setSensor(id) {
        sensorId = id
        unitStr = sensorCatalog.displayUnit(id)
        titleStr = sensorCatalog.displayName(id)
        minV = sensorCatalog.displayMin(id)
        maxV = sensorCatalog.displayMax(id)
        titleField.text = titleStr
        minField.text = minV
        maxField.text = maxV
    }
    function openAdd(col) {
        editMode = false; colIndex = col; traceId = 0
        searchField.text = ""
        refreshList()
        colorStr = "#0033CC"
        setSensor(fullList.length ? fullList[0].id : 200)
        open()
    }
    function openEdit(col, t) {
        editMode = true; colIndex = col; traceId = t.id
        searchField.text = ""
        refreshList()
        sensorId = t.sensorId
        unitStr = sensorCatalog.displayUnit(t.sensorId)
        titleStr = t.title; colorStr = t.color; minV = t.min; maxV = t.max
        titleField.text = t.title; minField.text = t.min; maxField.text = t.max
        open()
    }
    function apply() {
        var mn = parseFloat(minField.text), mx = parseFloat(maxField.text)
        if (isNaN(mn)) mn = 0
        if (isNaN(mx) || mx <= mn) mx = mn + 1
        var ttl = titleField.text.trim()
        if (ttl === "") ttl = sensorCatalog.displayName(dlg.sensorId)
        if (editMode) chartManager.updateTrace(colIndex, traceId, sensorId, ttl, colorStr, mn, mx)
        else chartManager.addTrace(colIndex, sensorId, ttl, colorStr, mn, mx)
        win.toast(editMode ? "Параметр обновлён" : "Параметр добавлен в колонку " + (colIndex + 1))
        close()
    }

    header: Rectangle {
        implicitHeight: 48
        color: dlg.win.ink
        radius: 8
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 8; color: dlg.win.ink }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            Text {
                text: dlg.title
                color: "#FFFFFF"
                font.pixelSize: 16
                font.bold: true
            }
            Item { Layout.fillWidth: true }
            Text {
                text: "колонка " + (dlg.colIndex + 1)
                color: "#9FB6CC"
                font.pixelSize: 12
            }
        }
    }
    background: Rectangle { color: "#FFFFFF"; border.color: dlg.win.line; radius: 8 }

    contentItem: ColumnLayout {
        spacing: 10

        // ---- выбор датчика из Sensors.csv ----
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label { text: "Датчик:"; color: dlg.win.ink; font.pixelSize: 13; font.bold: true }
            TextField {
                id: searchField
                Layout.fillWidth: true
                placeholderText: "Поиск по коду, названию или единицам…"
                font.pixelSize: 13
                color: dlg.win.ink
                onTextChanged: dlg.filterList()
                background: Rectangle {
                    radius: 6
                    color: searchField.activeFocus ? "#FDF8EC" : "#FFFFFF"
                    border.color: searchField.activeFocus ? dlg.win.accent : dlg.win.line
                }
            }
            Label {
                text: dlg.shownList.length + " / " + dlg.fullList.length
                color: "#8894A0"; font.pixelSize: 11
            }
            Button {
                text: "Перечитать CSV"
                font.pixelSize: 11
                onClicked: {
                    if (sensorCatalog.reloadCsv()) { dlg.refreshList(); dlg.win.toast("Sensors.csv перечитан: " + sensorCatalog.csvCount()) }
                    else dlg.win.toast("Sensors.csv не найден рядом с программой")
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 168
            radius: 6
            color: "#FBFCFD"
            border.color: dlg.win.line

            Text {
                anchors.centerIn: parent
                width: parent.width - 50
                visible: dlg.fullList.length === 0
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: "#8894A0"
                font.pixelSize: 12
                text: "Sensors.csv не загружен. Положите файл рядом с программой и нажмите «Перечитать CSV», либо задайте код датчика вручную ниже."
            }
            Text {
                anchors.centerIn: parent
                width: parent.width - 50
                visible: dlg.fullList.length > 0 && dlg.shownList.length === 0
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: "#8894A0"
                font.pixelSize: 12
                text: "Ничего не найдено по запросу «" + searchField.text + "»"
            }

            ListView {
                id: sensorView
                anchors.fill: parent
                anchors.margins: 4
                clip: true
                model: dlg.shownList
                delegate: Rectangle {
                    required property var modelData
                    width: sensorView.width
                    height: 32
                    radius: 5
                    color: ma.hovered ? "#FDF3DC"
                         : (dlg.sensorId === modelData.id ? "#E8F0F7" : "transparent")
                    border.color: dlg.sensorId === modelData.id ? dlg.win.accent : "transparent"
                    border.width: 1
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        anchors.rightMargin: 8
                        spacing: 8
                        Rectangle {
                            Layout.preferredWidth: 46
                            Layout.preferredHeight: 20
                            radius: 4
                            color: dlg.sensorId === modelData.id ? dlg.win.ink : "#E4EAEF"
                            Text {
                                anchors.centerIn: parent
                                text: modelData.id
                                font.pixelSize: 11
                                font.bold: true
                                font.family: "Monospace"
                                color: dlg.sensorId === modelData.id ? "#FFFFFF" : dlg.win.ink
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: modelData.name
                            color: dlg.win.ink
                            font.pixelSize: 12
                            font.bold: dlg.sensorId === modelData.id
                            elide: Text.ElideRight
                        }
                        Text {
                            text: modelData.unit || ""
                            color: "#8894A0"; font.pixelSize: 11
                        }
                        Text {
                            text: (dlg.tick, Number(dataStore.value(modelData.id)).toFixed(2))
                            color: dlg.win.inkSoft
                            font.pixelSize: 11
                            font.family: "Monospace"
                        }
                    }
                    MouseArea {
                        id: ma
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: dlg.setSensor(modelData.id)
                    }
                }
                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                    background: Rectangle { color: "#EDF0F2"; radius: 4 }
                    contentItem: Rectangle {
                        implicitWidth: 8
                        radius: 4
                        color: dlg.win.inkSoft
                    }
                }
            }
        }

        // ---- код вручную + название ----
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label { text: "Код:"; color: dlg.win.ink; font.pixelSize: 13; font.bold: true }
            SpinBox {
                from: 1; to: 65535
                value: dlg.sensorId
                onValueModified: dlg.setSensor(value)
                Layout.preferredWidth: 110
            }
            Label { text: "Название:"; color: dlg.win.ink; font.pixelSize: 13; font.bold: true }
            TextField {
                id: titleField
                Layout.fillWidth: true
                font.pixelSize: 13
                color: dlg.win.ink
                onTextChanged: dlg.titleStr = text
                background: Rectangle {
                    radius: 6
                    color: titleField.activeFocus ? "#FDF8EC" : "#FFFFFF"
                    border.color: titleField.activeFocus ? dlg.win.accent : dlg.win.line
                }
            }
            Button {
                text: "Из каталога"
                font.pixelSize: 11
                onClicked: dlg.setSensor(dlg.sensorId)
            }
        }

        // ---- цвет ----
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label { text: "Цвет:"; color: dlg.win.ink; font.pixelSize: 13; font.bold: true }
            Flow {
                Layout.fillWidth: true
                spacing: 6
                Repeater {
                    model: ["#D40000", "#0033CC", "#00A651", "#E6007E", "#E8720C", "#00A3E8",
                            "#8A2BE2", "#00B8A9", "#C2185B", "#6B7A88", "#111111", "#FF7A00"]
                    Rectangle {
                        required property var modelData
                        width: 26; height: 26; radius: 6
                        color: modelData
                        border.color: dlg.colorStr === modelData ? dlg.win.ink : "#D5DDE3"
                        border.width: dlg.colorStr === modelData ? 2 : 1
                        Text {
                            anchors.centerIn: parent
                            visible: dlg.colorStr === modelData
                            text: "✓"
                            font.pixelSize: 13
                            font.bold: true
                            color: dlg.lum(modelData) > 0.55 ? "#12314E" : "#FFFFFF"
                        }
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: dlg.colorStr = modelData
                            cursorShape: Qt.PointingHandCursor
                        }
                    }
                }
            }
            Rectangle {
                Layout.preferredWidth: 46
                Layout.preferredHeight: 26
                radius: 6
                color: dlg.colorStr
                border.color: dlg.win.line
            }
        }

        // ---- диапазон + колонка ----
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label { text: "Мин:"; color: dlg.win.ink; font.pixelSize: 13; font.bold: true }
            TextField {
                id: minField
                Layout.preferredWidth: 90
                font.pixelSize: 13
                font.family: "Monospace"
                color: dlg.win.ink
                validator: DoubleValidator { notation: DoubleValidator.StandardNotation }
                background: Rectangle {
                    radius: 6
                    color: minField.activeFocus ? "#FDF8EC" : "#FFFFFF"
                    border.color: minField.activeFocus ? dlg.win.accent : dlg.win.line
                }
            }
            Label { text: "Макс:"; color: dlg.win.ink; font.pixelSize: 13; font.bold: true }
            TextField {
                id: maxField
                Layout.preferredWidth: 90
                font.pixelSize: 13
                font.family: "Monospace"
                color: dlg.win.ink
                validator: DoubleValidator { notation: DoubleValidator.StandardNotation }
                background: Rectangle {
                    radius: 6
                    color: maxField.activeFocus ? "#FDF8EC" : "#FFFFFF"
                    border.color: maxField.activeFocus ? dlg.win.accent : dlg.win.line
                }
            }
            Label { text: dlg.unitStr; color: "#8894A0"; font.pixelSize: 12 }
            Button {
                text: "По данным"
                font.pixelSize: 11
                onClicked: {
                    var r = dataStore.rangeOf(dlg.sensorId)
                    minField.text = Number(r[0]).toFixed(2)
                    maxField.text = Number(r[1]).toFixed(2)
                }
            }
            Item { Layout.fillWidth: true }
            Label { text: "Колонка:"; color: dlg.win.ink; font.pixelSize: 13; font.bold: true }
            ComboBox {
                id: colCombo
                Layout.preferredWidth: 120
                model: chartManager.rowCount()
                currentIndex: dlg.colIndex
                onActivated: function(i) { dlg.colIndex = i }
                delegate: ItemDelegate {
                    required property int index
                    width: colCombo.width
                    text: "Колонка " + (index + 1)
                    font.pixelSize: 12
                    highlighted: colCombo.highlightedIndex === index
                }
                background: Rectangle {
                    radius: 6
                    color: "#FFFFFF"
                    border.color: colCombo.hovered ? dlg.win.accent : dlg.win.line
                }
            }
        }

        // ---- живой предпросмотр ----
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 110
            radius: 6
            color: "#FFFFFF"
            border.color: dlg.win.line
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 4
                Label {
                    text: "Предпросмотр · окно " + (chartManager.timeWindow / 60).toFixed(0) + " мин"
                    color: "#8894A0"; font.pixelSize: 10
                }
                GeoChart {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    store: dataStore
                    windowSeconds: chartManager.timeWindow
                    offsetSeconds: chartManager.viewOffset
                    traces: [{
                        sensorId: dlg.sensorId,
                        title: dlg.titleStr,
                        color: dlg.colorStr,
                        min: parseFloat(minField.text) || 0,
                        max: parseFloat(maxField.text) || 100
                    }]
                }
            }
        }
    }

    footer: DialogButtonBox {
        padding: 10
        Button {
            text: "Убрать из колонки"
            visible: dlg.editMode
            DialogButtonBox.buttonRole: DialogButtonBox.DestructiveRole
            font.pixelSize: 12
            contentItem: Text {
                text: parent.text
                color: dlg.win.bad
                font: parent.font
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: 6
                implicitHeight: 34
                color: parent.down ? "#FBE9E9" : "#FFFFFF"
                border.color: dlg.win.bad
            }
            onClicked: {
                chartManager.removeTrace(dlg.colIndex, dlg.traceId)
                dlg.win.toast("Параметр убран из колонки")
                dlg.close()
            }
        }
        Item { Layout.fillWidth: true }
        Button {
            text: "Отмена"
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            font.pixelSize: 13
        }
        Button {
            text: dlg.editMode ? "Сохранить" : "Добавить"
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
                implicitHeight: 34
                implicitWidth: 130
                color: parent.down ? dlg.win.ink : dlg.win.inkSoft
            }
            onClicked: dlg.apply()
        }
    }
}
