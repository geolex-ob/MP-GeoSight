import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import GeoSight 1.0

ApplicationWindow {
    id: root
    visible: true
    width: 1600; height: 900
    minimumWidth: 1280
    minimumHeight: 720
    color: "#E8EBEE"
    title: "MP GeoSight"

    readonly property color ink: "#12314E"
    readonly property color inkSoft: "#28517A"
    readonly property color accent: "#E8A013"
    readonly property color paper: "#FFFFFF"
    readonly property color line: "#B9C4CE"
    readonly property color ok: "#1D8A4E"
    readonly property color bad: "#C22B2B"
    readonly property int gutterW: 92
    readonly property int colGap: 2

    property int tick: 0
    property int profilesRev: 0
    property int colRev: 0
    property int infosRev: 0
    property int commentCol: -1
    property int dragCol: -1
    property int dragId: -1
    property bool didDrag: false
    property int headerBand: 34 + chartManager.maxTraceRows * 26 + ((root.colRev, root.computeAnyLog()) ? 16 : 0)
    Timer { interval: 1000; running: true; repeat: true; onTriggered: root.tick++ }

    function wheelStep() { return Math.max(30, chartManager.timeWindow / 10) }
    function scroll(dir) {
        // не уходим дальше начала накопленного буфера
        var maxOff = Math.max(0, dataStore.bufferedHours() * 3600 - chartManager.timeWindow)
        maxOff = Math.min(maxOff, 96 * 3600)
        chartManager.viewOffset = Math.max(0, Math.min(maxOff,
            chartManager.viewOffset + dir * root.wheelStep()))
    }
    function fmtVal(v, col) {
        return chartManager.columnLog(col) ? v.toFixed(4) : v.toFixed(2)
    }
    function computeAnyLog() {
        for (var c = 0; c < chartManager.rowCount(); ++c)
            if (chartManager.columnLog(c)) return true
        return false
    }
    function decadesOf(col) {
        var lo = chartManager.columnMin(col), hi = chartManager.columnMax(col)
        if (!(lo > 0) || !(hi > lo)) return []
        var out = []
        for (var d = Math.ceil(Math.log10(lo)); d <= Math.floor(Math.log10(hi)); ++d)
            out.push(Math.pow(10, d))
        return out
    }
    function fmtDec(v) {
        return v >= 0.001 ? String(parseFloat(v.toPrecision(4))) : v.toExponential(0)
    }
    function decadeRange(col) {
        var ts = chartManager.tracesOf(col)
        var mn = Infinity, mx = -Infinity
        for (var i = 0; i < ts.length; ++i) {
            var r = dataStore.rangeOf(ts[i].sensorId)
            if (r[0] > 0 && r[0] < mn) mn = r[0]
            if (r[1] > mx) mx = r[1]
        }
        if (!isFinite(mn) || mn <= 0) mn = 0.0001
        if (!isFinite(mx) || mx <= mn) mx = mn * 10000
        mn = Math.pow(10, Math.floor(Math.log10(mn)))
        mx = Math.pow(10, Math.ceil(Math.log10(mx)))
        if (mx <= mn) mx = mn * 10
        return [mn, mx]
    }
    function toggleColumnLog(col) {
        var on = !chartManager.columnLog(col)
        chartManager.setColumnLog(col, on)
        if (on) {
            var r = decadeRange(col)
            chartManager.setColumnRange(col, r[0], r[1])
        }
        root.toast(on ? "Колонка " + (col + 1) + ": лог-шкала, общий диапазон " + r[0] + "…" + r[1]
                      : "Колонка " + (col + 1) + ": линейная шкала")
    }
    function applyCsvNames(notify) {
        var n = 0
        for (var c = 0; c < chartManager.rowCount(); ++c) {
            var ts = chartManager.tracesOf(c)
            for (var i = 0; i < ts.length; ++i) {
                var t = ts[i]
                var nm = sensorCatalog.displayName(t.sensorId)
                if (nm !== t.title) {
                    chartManager.updateTrace(c, t.id, t.sensorId, nm, t.color, t.min, t.max)
                    n++
                }
            }
        }
        if (notify) root.toast("Названия обновлены из Sensors.csv: " + n)
        return n
    }
    function syncWin() {
        var idx = 4
        for (var i = 0; i < winModel.count; ++i)
            if (winModel.get(i).sec === chartManager.timeWindow) idx = i
        winCombo.currentIndex = idx
    }
    function toast(msg) {
        toastLabel.text = msg
        toastLabel.opacity = 1
        toastTimer.restart()
    }
    function startComment(col) {
        root.commentCol = col
        root.toast("Кликните левой кнопкой по графику колонки " + (col + 1) + " для комментария")
    }
    function openCommentAdd(col, t, yFrac) {
        commentDialog.col = col; commentDialog.t = t; commentDialog.yFrac = yFrac; commentDialog.editId = -1
        commentText.text = ""
        commentDialog.open()
    }
    function openCommentEdit(col, cid) {
        var list = chartManager.commentsOf(col)
        for (var i = 0; i < list.length; ++i) {
            if (list[i].id === cid) {
                commentDialog.col = col; commentDialog.t = list[i].t; commentDialog.yFrac = list[i].yFrac
                commentDialog.editId = cid; commentText.text = list[i].text
                commentDialog.open(); return
            }
        }
    }

    // ---------- геометрия колонок: сумма ширин = ширине контейнера ----------
    function availWidth() {
        var n = Math.max(1, chartManager.rowCount())
        var a = chartsHost.width - root.gutterW - root.colGap * n
        return (isFinite(a) && a > 0) ? a : 100 * n
    }
    function sumWeights() {
        var n = chartManager.rowCount(), sumW = 0
        for (var k = 0; k < n; ++k) {
            var w = chartManager.columnWeight(k)
            sumW += (isFinite(w) && w > 0) ? w : 1
        }
        return (isFinite(sumW) && sumW > 0) ? sumW : Math.max(1, n)
    }
    function colWidth(i) {
        var n = Math.max(1, chartManager.rowCount())
        var w = chartManager.columnWeight(i)
        if (!isFinite(w) || w <= 0) w = 1
        var cw = availWidth() * w / sumWeights()
        return (isFinite(cw) && cw >= 10) ? cw : availWidth() / n
    }
    function colX(i) {
        var x = root.gutterW + root.colGap * (i + 1)
        for (var k = 0; k < i; ++k) x += colWidth(k)
        return x
    }
    function resizeColumn(i, d) {
        var n = chartManager.rowCount()
        if (i < 0 || i >= n - 1) return
        if (!isFinite(d)) return
        var avail = availWidth()
        if (avail <= 50) return
        var dw = d / avail * sumWeights()
        if (!isFinite(dw)) return
        var wi = chartManager.columnWeight(i) + dw
        var wj = chartManager.columnWeight(i + 1) - dw
        var minW = 0.05
        if (wi < minW) { wj -= (minW - wi); wi = minW }
        if (wj < minW) { wi -= (minW - wj); wj = minW }
        chartManager.setColumnWeight(i, wi)
        chartManager.setColumnWeight(i + 1, wj)
    }

    component ToolButton: Button {
        id: tb
        font.pixelSize: 12
        contentItem: Text {
            text: tb.text
            color: tb.down ? "#FFFFFF" : root.ink
            font: tb.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 6
            implicitHeight: 30
            color: tb.down ? root.inkSoft : (tb.hovered ? "#FDF3DC" : "#FFFFFF")
            border.color: tb.hovered ? root.accent : root.line
        }
    }

    component ValueBox: Rectangle {
        id: vb
        property string caption: ""
        property string val: "—"
        property color bar: root.accent
        Layout.preferredWidth: 108
        Layout.preferredHeight: 40
        color: "#FFFFFF"
        border.color: root.line
        radius: 6
        Rectangle {
            x: 0; y: 6
            width: 3; height: parent.height - 12
            radius: 2
            color: vb.bar
        }
        Column {
            anchors.centerIn: parent
            spacing: 0
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: vb.caption
                font.pixelSize: 9
                font.bold: true
                color: "#6B7A88"
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: vb.val
                font.pixelSize: 17
                font.bold: true
                font.family: "Monospace"
                color: root.ink
            }
        }
    }

    menuBar: MenuBar {
        background: Rectangle { color: root.ink; implicitHeight: 32 }
        palette.windowText: "#FFFFFF"
        palette.highlight: root.accent
        palette.highlightedText: "#12314E"
        Menu {
            title: "Меню"
            palette.text: root.ink
            palette.window: "#FFFFFF"
            palette.highlight: "#FDF3DC"
            palette.highlightedText: root.ink
            background: Rectangle { color: "#FFFFFF"; border.color: root.line; radius: 6 }
            MenuItem { text: "Настройки подключения…"; onTriggered: settingsDialog.open() }
            MenuSeparator { }
            MenuItem {
                text: "Сохранить настройки отображения"
                onTriggered: { chartManager.saveNow(); root.toast("Настройки отображения сохранены в charts.json") }
            }
            MenuItem { text: "Сбросить настройки отображения…"; onTriggered: resetConfirm.open() }
            Menu {
                title: "Профили (.cfg)"
                palette.text: root.ink
                palette.window: "#FFFFFF"
                palette.highlight: "#FDF3DC"
                palette.highlightedText: root.ink
                background: Rectangle { color: "#FFFFFF"; border.color: root.line; radius: 6 }
                MenuItem { text: "Сохранить как .cfg…"; onTriggered: cfgSave.open() }
                MenuItem { text: "Загрузить .cfg…"; onTriggered: cfgOpen.open() }
                MenuSeparator { }
                MenuItem { text: "Сохранить текущий в библиотеку…"; onTriggered: libSave.open() }
                Menu {
                    id: libMenu
                    title: "Библиотека профилей"
                    palette.text: root.ink
                    palette.highlight: "#FDF3DC"
                    background: Rectangle { color: "#FFFFFF"; border.color: root.line; radius: 6 }
                    Instantiator {
                        model: (root.profilesRev, chartManager.profileNames())
                        delegate: MenuItem {
                            required property var modelData
                            text: modelData
                            onTriggered: {
                                if (chartManager.importFromFile(chartManager.profilePath(modelData))) {
                                    root.profilesRev++
                                    root.toast("Профиль применён: " + modelData)
                                } else root.toast("Ошибка: " + chartManager.lastError())
                            }
                        }
                        onObjectAdded: libMenu.insertItem(index, object)
                        onObjectRemoved: libMenu.removeItem(object)
                    }
                    MenuItem { text: "(библиотека пуста)"; enabled: false; visible: chartManager.profileNames().length === 0 }
                }
                MenuItem { text: "Обновить список"; onTriggered: root.profilesRev++ }
            }
            MenuSeparator { }
            MenuItem {
                text: "Обновить названия из Sensors.csv"
                onTriggered: {
                    if (sensorCatalog.reloadCsv()) root.applyCsvNames(true)
                    else root.toast("Sensors.csv не найден рядом с программой")
                }
            }
            MenuItem { text: "Переподключиться"; onTriggered: { networkManager.reconnect(); root.toast("Переподключение к " + networkManager.serverIP + " …") } }
            MenuItem {
                id: autoItem
                checkable: true
                checked: networkManager.autoConnect
                text: (checked ? "✓  " : "     ") + "Автоподключение при запуске"
                onTriggered: networkManager.autoConnect = checked
            }
            MenuSeparator { }
            MenuItem { text: "О программе…"; onTriggered: aboutDialog.open() }
            MenuItem { text: "Выход"; onTriggered: Qt.quit() }
        }
    }

    header: Rectangle {
        height: 56
        color: "#F4F6F8"
        border.color: root.line
        RowLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 8

            Rectangle {
                Layout.preferredHeight: 30
                Layout.preferredWidth: 150
                radius: 6
                color: networkManager.connected ? "#E4F4EA" : "#FBE9E9"
                border.color: networkManager.connected ? root.ok : root.bad
                RowLayout {
                    anchors.centerIn: parent
                    spacing: 6
                    Rectangle { Layout.preferredWidth: 9; Layout.preferredHeight: 9; radius: 5
                                color: networkManager.connected ? root.ok : root.bad }
                    Label { text: networkManager.connected ? "Подключено" : "Нет связи"
                            color: networkManager.connected ? root.ok : root.bad
                            font.pixelSize: 12; font.bold: true }
                }
            }
            Label { text: networkManager.serverIP; color: root.inkSoft; font.pixelSize: 12; font.family: "Monospace" }
            Label {
                text: chartManager.viewOffset > 0 ? "АРХИВ −" + (chartManager.viewOffset / 60).toFixed(0) + "м" : "LIVE"
                color: chartManager.viewOffset > 0 ? root.bad : root.ok
                font.bold: true; font.pixelSize: 12
            }
            ToolButton { text: "К реальному времени"; visible: chartManager.viewOffset > 0; onClicked: chartManager.viewOffset = 0 }
            Item { Layout.fillWidth: true }
            Label { text: "Окно:"; color: root.ink; font.pixelSize: 12 }
            ComboBox {
                id: winCombo
                Layout.preferredWidth: 110
                textRole: "label"
                font.pixelSize: 12
                model: ListModel {
                    id: winModel
                    ListElement { label: "1 минута";  sec: 60 }
                    ListElement { label: "2 минуты";  sec: 120 }
                    ListElement { label: "5 минут";   sec: 300 }
                    ListElement { label: "10 минут";  sec: 600 }
                    ListElement { label: "15 минут";  sec: 900 }
                    ListElement { label: "20 минут";  sec: 1200 }
                    ListElement { label: "30 минут";  sec: 1800 }
                    ListElement { label: "60 минут";  sec: 3600 }
                    ListElement { label: "2 часа";    sec: 7200 }
                    ListElement { label: "3 часа";    sec: 10800 }
                    ListElement { label: "4 часа";    sec: 14400 }
                    ListElement { label: "6 часов";   sec: 21600 }
                    ListElement { label: "12 часов";  sec: 43200 }
                    ListElement { label: "24 часа";   sec: 86400 }
                }
                onActivated: function(i) { chartManager.timeWindow = winModel.get(i).sec }
                Component.onCompleted: root.syncWin()
                background: Rectangle { radius: 6; color: "#FFFFFF"; border.color: winCombo.hovered ? root.accent : root.line }
                popup: Popup {
                    y: winCombo.height + 2
                    width: winCombo.width
                    height: Math.min(winModel.count * 30 + 8, root.height - 140)
                    background: Rectangle { color: "#FFFFFF"; border.color: root.line; radius: 6 }
                    contentItem: ListView {
                        clip: true
                        model: winModel
                        currentIndex: winCombo.highlightedIndex
                        delegate: ItemDelegate {
                            width: winCombo.width
                            height: 30
                            required property var model
                            required property int index
                            contentItem: Text { text: model.label; color: root.ink; font.pixelSize: 12; verticalAlignment: Text.AlignVCenter; leftPadding: 10 }
                            background: Rectangle { color: highlighted ? "#FDF3DC" : "transparent" }
                            onClicked: {
                                winCombo.currentIndex = index
                                chartManager.timeWindow = winModel.get(index).sec
                                winCombo.popup.close()
                            }
                        }
                    }
                }
            }
            ToolButton { text: "+ Колонка"; onClicked: chartManager.addColumn() }
            ValueBox {
                caption: "ЗАБОЙ, м"
                bar: root.accent
                val: (root.tick, dataStore.secondsSinceUpdate() < 0 ? "—" : Number(dataStore.value(chartManager.depthSensorId)).toFixed(2))
            }
            ValueBox {
                caption: "СВЕЧИ, шт"
                bar: "#C22B2B"
                val: (root.tick, dataStore.secondsSinceUpdate() < 0 ? "—"
                      : Number(dataStore.value(chartManager.standsSensorId)).toFixed(0))
            }
            ValueBox {
                caption: "ДОЛОТО, м"
                bar: root.inkSoft
                val: (root.tick, dataStore.secondsSinceUpdate() < 0 ? "—" : Number(dataStore.value(chartManager.bitSensorId)).toFixed(2))
            }
            Repeater {
                model: 3
                delegate: Rectangle {
                    id: infoHdr
                    required property int index
                    Layout.preferredWidth: 92
                    Layout.preferredHeight: 40
                    color: "#FFFFFF"
                    border.color: root.line
                    radius: 6
                    property var info: (root.infosRev, chartManager.infos())[infoHdr.index]
                    Rectangle { x: 0; y: 6; width: 3; height: parent.height - 12; radius: 2; color: "#00B8A9" }
                    Column {
                        anchors.centerIn: parent
                        spacing: 0
                        Text {
                            width: parent.width - 8
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                            text: info.title
                            font.pixelSize: 8; font.bold: true; color: "#6B7A88"
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: (root.tick, info.sensorId > 0 && dataStore.secondsSinceUpdate() >= 0
                                  ? Number(dataStore.value(info.sensorId)).toFixed(info.decimals) + " " + info.unit : "—")
                            font.pixelSize: 15; font.bold: true; font.family: "Monospace"; color: root.ink
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onDoubleClicked: infoDialog.openFor(infoHdr.index)
                    }
                    Text { anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 3
                           text: "⚙"; font.pixelSize: 8; color: "#B9C4CE" }
                }
            }
            Rectangle {
                Layout.preferredWidth: 96; Layout.preferredHeight: 40
                color: "#FFFFFF"; border.color: root.line; radius: 6
                Text { id: clock; anchors.centerIn: parent; font.pixelSize: 32; font.bold: true; color: root.ink }
                Timer { interval: 1000; running: true; repeat: true; triggeredOnStart: true
                        onTriggered: clock.text = Qt.formatTime(new Date(), "hh:mm") }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 2

        // ---------- Шапки колонок ----------
        Item {
            id: headersHost
            Layout.fillWidth: true
            Layout.preferredHeight: root.headerBand
            Layout.minimumHeight: root.headerBand
            Layout.maximumHeight: root.headerBand

            Rectangle {
                x: 0; y: 0
                width: root.gutterW
                height: parent.height
                color: "#F2F5F7"
                border.color: root.line
                Column {
                    anchors.centerIn: parent
                    spacing: 2
                    Text { text: "Глубина"; font.pixelSize: 10; font.bold: true; color: root.ink; width: parent.width; horizontalAlignment: Text.AlignHCenter }
                    Text { text: "Время"; font.pixelSize: 10; font.bold: true; color: root.ink; width: parent.width; horizontalAlignment: Text.AlignHCenter }
                }
            }

            Repeater {
                model: chartManager
                delegate: Rectangle {
                    id: hdrRect
                    required property var modelData
                    required property int index
                    x: (root.colRev, root.colX(index))
                    y: 0
                    width: (root.colRev, root.colWidth(index))
                    height: parent.height
                    color: "#FBFCFD"
                    border.color: root.line

                    Column {
                        anchors.fill: parent
                        anchors.margins: 2
                        spacing: 2

                        Row {
                            width: parent.width
                            height: 22
                            spacing: 4
                            Text {
                                text: "Колонка " + (hdrRect.index + 1) + ((root.colRev, chartManager.columnLog(hdrRect.index)) ? " · log" : "")
                                font.pixelSize: 10; font.bold: true; color: root.ink
                                anchors.verticalCenter: parent.verticalCenter
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onDoubleClicked: colDialog.openFor(hdrRect.index)
                                }
                            }
                            ToolButton { text: "+ параметр"; font.pixelSize: 9; height: 20; anchors.verticalCenter: parent.verticalCenter
                                         onClicked: traceDialog.openAdd(hdrRect.index) }
                            ToolButton { text: "💬"; font.pixelSize: 9; height: 20; anchors.verticalCenter: parent.verticalCenter
                                         onClicked: root.startComment(hdrRect.index) }
                            ToolButton { text: "×"; font.pixelSize: 9; height: 20; anchors.verticalCenter: parent.verticalCenter
                                         onClicked: chartManager.removeColumn(hdrRect.index) }
                        }

                        Repeater {
                            model: hdrRect.modelData
                            delegate: Rectangle {
                                required property var modelData
                                width: parent ? parent.width : 0
                                height: 26
                                color: chartManager.selectedSensor === modelData.sensorId ? "#FFF3DD" : "transparent"
                                border.color: chartManager.selectedSensor === modelData.sensorId ? root.accent : "transparent"
                                border.width: 1

                                Column {
                                    anchors.fill: parent
                                    spacing: 0
                                    Text {
                                        width: parent.width
                                        height: 14
                                        font.pixelSize: chartManager.headerFontPx
                                        font.bold: true
                                        color: modelData.color
                                        text: (root.tick, modelData.title + " = " +
                                               root.fmtVal(Number(dataStore.value(modelData.sensorId)), hdrRect.index))
                                        elide: Text.ElideRight
                                    }
                                    Row {
                                        width: parent.width
                                        height: 10
                                        spacing: 0
                                        visible: !(root.colRev, chartManager.columnLog(hdrRect.index))
                                        Repeater {
                                            model: 5
                                            delegate: Text {
                                                required property int index
                                                width: (parent ? parent.width : 0) / 5
                                                font.pixelSize: 7
                                                color: "#8894A0"
                                                horizontalAlignment: index === 0 ? Text.AlignLeft : index === 4 ? Text.AlignRight : Text.AlignHCenter
                                                text: (modelData.min + (modelData.max - modelData.min) * index / 4).toFixed(1)
                                            }
                                        }
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                    onDoubleClicked: traceDialog.openEdit(hdrRect.index, modelData)
                                    onClicked: function(m) {
                                        if (m.button === Qt.RightButton) rowMenu.popup()
                                        else chartManager.selectedSensor = modelData.sensorId
                                    }
                                }
                                Menu {
                                    id: rowMenu
                                    palette.text: root.ink
                                    palette.highlight: "#FDF3DC"
                                    background: Rectangle { color: "#FFFFFF"; border.color: root.line; radius: 6 }
                                    MenuItem { text: "Изменить…"; onTriggered: traceDialog.openEdit(hdrRect.index, modelData) }
                                    MenuItem { text: "Убрать"; onTriggered: chartManager.removeTrace(hdrRect.index, modelData.id) }
                                }
                            }
                        }

                        Item {
                            visible: (root.colRev, chartManager.columnLog(hdrRect.index))
                            width: parent.width
                            height: 14
                            Repeater {
                                model: (root.colRev, root.decadesOf(hdrRect.index))
                                delegate: Text {
                                    required property var modelData
                                    x: parent ? (Math.log10(modelData) - Math.log10(chartManager.columnMin(hdrRect.index))) /
                                       (Math.log10(chartManager.columnMax(hdrRect.index)) - Math.log10(chartManager.columnMin(hdrRect.index))) *
                                       (parent.width - 34) : 0
                                    font.pixelSize: 7
                                    color: "#8894A0"
                                    text: root.fmtDec(modelData)
                                }
                            }
                        }
                    }
                }
            }
        }

        // ---------- Графики ----------
        Item {
            id: chartsHost
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 200

            TimeAxis {
                x: 0; y: 0
                width: root.gutterW
                height: parent.height
                store: dataStore
                windowSeconds: chartManager.timeWindow
                depthSensorId: chartManager.depthSensorId
                offsetSeconds: chartManager.viewOffset
            }

            Repeater {
                model: chartManager
                delegate: Rectangle {
                    id: colRect
                    required property var modelData
                    required property int index
                    x: (root.colRev, root.colX(index))
                    y: 0
                    width: (root.colRev, root.colWidth(index))
                    height: parent.height
                    color: "#FFFFFF"
                    border.color: root.line

                    GeoChart {
                        id: geoChart
                        anchors.fill: parent
                        anchors.margins: 1
                        store: dataStore
                        traces: colRect.modelData
                        windowSeconds: chartManager.timeWindow
                        offsetSeconds: chartManager.viewOffset
                        logScale: (root.colRev, chartManager.columnLog(colRect.index))
                        sharedMin: (root.colRev, chartManager.columnMin(colRect.index))
                        sharedMax: (root.colRev, chartManager.columnMax(colRect.index))
                        comments: (root.colRev, chartManager.commentsOf(colRect.index))
                    }

                    MouseArea {
                        id: plotMA
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onWheel: function(w) { root.scroll(w.angleDelta.y > 0 ? 1 : -1) }
                        onPressed: function(m) {
                            if (m.button !== Qt.LeftButton) return
                            var pt = geoChart.mapFromItem(plotMA, m.x, m.y)
                            var cid = geoChart.commentAt(pt.x, pt.y)
                            if (cid >= 0) {
                                root.dragCol = colRect.index
                                root.dragId = cid
                                root.didDrag = false
                                geoChart.highlightComment = cid
                            }
                        }
                        onPositionChanged: function(m) {
                            if (root.dragId < 0) return
                            var pt = geoChart.mapFromItem(plotMA, m.x, m.y)
                            var cc = geoChart.commentCoords(pt.x, pt.y)
                            chartManager.moveComment(root.dragCol, root.dragId, cc[0], cc[1])
                            root.didDrag = true
                        }
                        onReleased: function(m) {
                            if (root.dragId >= 0) {
                                chartManager.saveNow()
                                geoChart.highlightComment = -1
                                root.dragId = -1
                            }
                        }
                        onDoubleClicked: function(m) {
                            if (m.button !== Qt.LeftButton) return
                            var pt = geoChart.mapFromItem(plotMA, m.x, m.y)
                            var cid = geoChart.commentAt(pt.x, pt.y)
                            if (cid >= 0) root.openCommentEdit(colRect.index, cid)
                            else geoChart.addMarkerAt(pt.x, pt.y, chartManager.selectedSensor)
                        }
                        onClicked: function(m) {
                            if (root.didDrag) { root.didDrag = false; return }
                            var pt = geoChart.mapFromItem(plotMA, m.x, m.y)
                            if (m.button === Qt.RightButton) {
                                if (!geoChart.removeMarkerAt(pt.x, pt.y)) {
                                    var cid = geoChart.commentAt(pt.x, pt.y)
                                    if (cid >= 0) { chartManager.removeComment(colRect.index, cid); root.toast("Комментарий удалён") }
                                }
                            } else if (m.button === Qt.LeftButton && root.commentCol === colRect.index) {
                                var cc = geoChart.commentCoords(pt.x, pt.y)
                                root.commentCol = -1
                                root.openCommentAdd(colRect.index, cc[0], cc[1])
                            }
                        }
                    }

                    // ---------- ручка изменения ширины (DragHandler) ----------
                    Rectangle {
                        z: 2
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 8
                        color: resizeDrag.active ? root.accent
                             : (resizeHover.containsMouse ? "#66E8A013" : "transparent")
                        MouseArea {
                            id: resizeHover
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.NoButton
                            cursorShape: Qt.SizeHorCursor
                        }
                        DragHandler {
                            id: resizeDrag
                            target: null
                            property real lastX: 0
                            onActiveChanged: {
                                if (active) {
                                    lastX = centroid.position.x
                                } else {
                                    chartManager.saveNow()
                                }
                            }
                            onCentroidChanged: {
                                if (!active) return
                                var d = centroid.position.x - lastX
                                lastX = centroid.position.x
                                root.resizeColumn(colRect.index, d)
                            }
                        }
                    }
                }
            }
        }
    }

    footer: Rectangle {
        height: 26
        color: "#F4F6F8"
        border.color: root.line
        RowLayout {
            anchors.fill: parent
            anchors.margins: 6
            spacing: 10
            Label { text: networkManager.connected ? "● Подключено" : "○ Отключено"
                    color: networkManager.connected ? root.ok : root.bad; font.pixelSize: 12 }
            Label { text: "Колесо — архив. Двойной клик — метка. Тяните правый край колонки — ширина."; color: "#8894A0"; font.pixelSize: 11 }
            Label {
                id: toastLabel
                opacity: 0
                color: root.inkSoft
                font.pixelSize: 12
                font.bold: true
                Behavior on opacity { NumberAnimation { duration: 300 } }
                Timer { id: toastTimer; interval: 2600; onTriggered: toastLabel.opacity = 0 }
            }
            Item { Layout.fillWidth: true }
            Label {
                id: srvLabel
                color: "#55636F"
                font.pixelSize: 12
                Timer {
                    interval: 1000; running: true; repeat: true; triggeredOnStart: true
                    onTriggered: {
                        var age = dataStore.secondsSinceUpdate()
                        srvLabel.text = age < 0 ? "Сервер: нет данных"
                            : "Время сервера: " + dataStore.serverTimeString() + "  (срез " + Math.round(age) + " с назад)  ·  буфер " + dataStore.bufferedHours().toFixed(1) + " ч"
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        if (sensorCatalog.hasCsv()) root.applyCsvNames(false)
    }

    Connections {
        target: chartManager
        function onConfigChanged() { root.syncWin(); root.colRev++; root.infosRev++ }
    }

    SettingsDialog { id: settingsDialog }
    AboutDialog { id: aboutDialog }
    TraceDialog { id: traceDialog }

    CfgDialog {
        id: cfgSave
        mode: "save"
        onChosen: function(p) {
            if (chartManager.exportToFile(p)) {
                var base = p.substring(p.lastIndexOf("/") + 1)
                chartManager.saveToLibrary(base)
                root.profilesRev++
                root.toast("Профиль сохранён: " + base + " (также в библиотеке)")
            } else root.toast("Ошибка: " + chartManager.lastError())
        }
    }
    CfgDialog {
        id: cfgOpen
        mode: "open"
        onChosen: function(p) {
            if (chartManager.importFromFile(p)) {
                root.profilesRev++
                root.toast("Профиль применён: " + p)
            } else root.toast("Ошибка: " + chartManager.lastError())
        }
    }

    Dialog {
        id: libSave
        title: "Сохранить в библиотеку профилей"
        modal: true; width: 440
        anchors.centerIn: Overlay.overlay
        onAboutToShow: libName.text = "profile.cfg"
        header: Rectangle { implicitHeight: 42; color: root.ink; radius: 8
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 8; color: root.ink }
            Text { anchors.centerIn: parent; text: libSave.title; color: "#FFFFFF"; font.pixelSize: 14; font.bold: true } }
        background: Rectangle { color: "#FFFFFF"; border.color: root.line; radius: 8 }
        contentItem: ColumnLayout {
            spacing: 10
            Label { Layout.fillWidth: true; wrapMode: Text.Wrap; color: "#33424F"; font.pixelSize: 13
                    text: "Имя профиля (файл .cfg в папке profiles рядом с программой):" }
            TextField { id: libName; Layout.fillWidth: true; font.pixelSize: 14; font.family: "Monospace"; color: root.ink
                        background: Rectangle { radius: 6; color: libName.activeFocus ? "#FDF8EC" : "#FFFFFF"; border.color: libName.activeFocus ? root.accent : root.line } }
            Label { Layout.fillWidth: true; wrapMode: Text.Wrap; color: "#6B7A88"; font.pixelSize: 11
                    text: "Файлы .cfg можно копировать на другие ПК и применять через «Загрузить .cfg…» или из библиотеки." }
        }
        footer: DialogButtonBox {
            Button { text: "Сохранить"; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: {
                    if (chartManager.saveToLibrary(libName.text)) { root.profilesRev++; root.toast("Профиль сохранён в библиотеку"); libSave.close() }
                    else root.toast("Ошибка: " + chartManager.lastError())
                } }
            Button { text: "Отмена"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
    }

    Dialog {
        id: commentDialog
        property int col: 0
        property double t: 0
        property double yFrac: 0
        property int editId: -1
        title: editId >= 0 ? "Комментарий" : "Новый комментарий"
        modal: true; width: 420
        anchors.centerIn: Overlay.overlay
        header: Rectangle { implicitHeight: 42; color: root.ink; radius: 8
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 8; color: root.ink }
            Text { anchors.centerIn: parent; text: commentDialog.title; color: "#FFFFFF"; font.pixelSize: 14; font.bold: true } }
        background: Rectangle { color: "#FFFFFF"; border.color: root.line; radius: 8 }
        contentItem: TextArea {
            id: commentText
            wrapMode: TextArea.Wrap
            font.pixelSize: 13
            color: root.ink
            background: Rectangle { color: "#FBFCFD"; border.color: root.line; radius: 6 }
        }
        footer: DialogButtonBox {
            Button { text: "Сохранить"; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: {
                    var txt = commentText.text.trim()
                    if (txt === "") { commentDialog.close(); return }
                    if (commentDialog.editId >= 0) chartManager.updateComment(commentDialog.col, commentDialog.editId, txt)
                    else chartManager.addComment(commentDialog.col, commentDialog.t, commentDialog.yFrac, txt)
                    commentDialog.close()
                } }
            Button { text: "Отмена"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
    }

    Dialog {
        id: infoDialog
        property int idx: 0
        title: "Настройка информационного окна"
        modal: true; width: 440
        anchors.centerIn: Overlay.overlay
        function openFor(i) {
            idx = i
            var inf = chartManager.infos()[i]
            iTitle.text = inf.title; iSensor.value = inf.sensorId; iUnit.text = inf.unit; iDec.value = inf.decimals
            open()
        }
        header: Rectangle { implicitHeight: 42; color: root.ink; radius: 8
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 8; color: root.ink }
            Text { anchors.centerIn: parent; text: infoDialog.title; color: "#FFFFFF"; font.pixelSize: 14; font.bold: true } }
        background: Rectangle { color: "#FFFFFF"; border.color: root.line; radius: 8 }
        contentItem: ColumnLayout {
            spacing: 10
            Label { text: "Заголовок:"; color: root.ink; font.pixelSize: 12 }
            TextField { id: iTitle; Layout.fillWidth: true; font.pixelSize: 13 }
            RowLayout {
                spacing: 10
                Label { text: "Датчик:"; color: root.ink; font.pixelSize: 12 }
                SpinBox { id: iSensor; from: 0; to: 65535; Layout.preferredWidth: 110 }
                Label { text: "Единицы:"; color: root.ink; font.pixelSize: 12 }
                TextField { id: iUnit; Layout.preferredWidth: 70; font.pixelSize: 13 }
                Label { text: "Знаков:"; color: root.ink; font.pixelSize: 12 }
                SpinBox { id: iDec; from: 0; to: 6; Layout.preferredWidth: 90 }
            }
            Label { text: "Датчик 0 = окно не выводит значение (—)."; color: "#8894A0"; font.pixelSize: 11 }
        }
        footer: DialogButtonBox {
            Button { text: "Сохранить"; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: { chartManager.setInfo(infoDialog.idx, iSensor.value, iTitle.text, iUnit.text, iDec.value); infoDialog.close() } }
            Button { text: "Отмена"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
    }

    Dialog {
        id: resetConfirm
        title: "Сброс настроек отображения"
        modal: true; width: 430
        anchors.centerIn: Overlay.overlay
        header: Rectangle { implicitHeight: 42; color: root.ink; radius: 8
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 8; color: root.ink }
            Text { anchors.centerIn: parent; text: resetConfirm.title; color: "#FFFFFF"; font.pixelSize: 14; font.bold: true } }
        background: Rectangle { color: "#FFFFFF"; border.color: root.line; radius: 8 }
        contentItem: Label { wrapMode: Text.Wrap; color: "#33424F"; font.pixelSize: 13
                             text: "Колонки, параметры, цвета и диапазоны будут заменены стандартным набором. Продолжить?" }
        footer: DialogButtonBox {
            Button { text: "Сбросить"; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: { chartManager.resetToDefaults(); root.toast("Настройки отображения сброшены к стандартным"); resetConfirm.close() } }
            Button { text: "Отмена"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
    }
}
