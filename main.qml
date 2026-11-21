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

    property int tick: 0
    property int profilesRev: 0
    property int headerBand: 34 + chartManager.maxTraceRows * 26
    Timer { interval: 1000; running: true; repeat: true; onTriggered: root.tick++ }

    function wheelStep() { return Math.max(30, chartManager.timeWindow / 10) }
    function scroll(dir) {
        chartManager.viewOffset = Math.max(0, Math.min(96 * 3600,
            chartManager.viewOffset + dir * root.wheelStep()))
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
            MenuItem {
                text: "Настройки подключения…"
                onTriggered: settingsDialog.open()
            }
            MenuSeparator { }
            MenuItem {
                text: "Сохранить настройки отображения"
                onTriggered: {
                    chartManager.saveNow()
                    root.toast("Настройки отображения сохранены в charts.json")
                }
            }
            MenuItem {
                text: "Сбросить настройки отображения…"
                onTriggered: resetConfirm.open()
            }
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
                                } else {
                                    root.toast("Ошибка: " + chartManager.lastError())
                                }
                            }
                        }
                        onObjectAdded: libMenu.insertItem(index, object)
                        onObjectRemoved: libMenu.removeItem(object)
                    }
                    MenuItem {
                        text: "(библиотека пуста)"
                        enabled: false
                        visible: chartManager.profileNames().length === 0
                    }
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
            MenuItem {
                text: "Переподключиться"
                onTriggered: {
                    networkManager.reconnect()
                    root.toast("Переподключение к " + networkManager.serverIP + " …")
                }
            }
            MenuItem {
                id: autoItem
                checkable: true
                checked: networkManager.autoConnect
                text: (checked ? "✓  " : "     ") + "Автоподключение при запуске"
                onTriggered: networkManager.autoConnect = checked
            }
            MenuSeparator { }
            MenuItem {
                text: "О программе…"
                onTriggered: aboutDialog.open()
            }
            MenuItem {
                text: "Выход"
                onTriggered: Qt.quit()
            }
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
                    Rectangle {
                        Layout.preferredWidth: 9; Layout.preferredHeight: 9; radius: 5
                        color: networkManager.connected ? root.ok : root.bad
                    }
                    Label {
                        text: networkManager.connected ? "Подключено" : "Нет связи"
                        color: networkManager.connected ? root.ok : root.bad
                        font.pixelSize: 12; font.bold: true
                    }
                }
            }
            Label {
                text: networkManager.serverIP
                color: root.inkSoft
                font.pixelSize: 12
                font.family: "Monospace"
            }
            Label {
                text: chartManager.viewOffset > 0
                      ? "АРХИВ: −" + (chartManager.viewOffset / 60).toFixed(1) + " мин"
                      : "Режим реального времени"
                color: chartManager.viewOffset > 0 ? root.bad : root.ok
                font.bold: true
                font.pixelSize: 12
            }
            ToolButton {
                text: "К реальному времени"
                visible: chartManager.viewOffset > 0
                onClicked: chartManager.viewOffset = 0
            }
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
                Component.onCompleted: {
                    var idx = 4
                    for (var i = 0; i < winModel.count; ++i)
                        if (winModel.get(i).sec === chartManager.timeWindow) idx = i
                    currentIndex = idx
                }
                background: Rectangle {
                    radius: 6
                    color: "#FFFFFF"
                    border.color: winCombo.hovered ? root.accent : root.line
                }
                popup: Popup {
                    y: winCombo.height + 2
                    width: winCombo.width
                    height: Math.min(winModel.count * 30 + 8, root.height - 140)
                    background: Rectangle { color: "#FFFFFF"; border.color: root.line; radius: 6 }
                    contentItem: ListView {
                        clip: true
                        model: winCombo.popup.contentItem ? winModel : winModel
                        currentIndex: winCombo.highlightedIndex
                        delegate: ItemDelegate {
                            width: winCombo.width
                            height: 30
                            required property var model
                            required property int index
                            contentItem: Text {
                                text: model.label
                                color: root.ink
                                font.pixelSize: 12
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 10
                            }
                            background: Rectangle {
                                color: highlighted ? "#FDF3DC" : "transparent"
                            }
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
            ToolButton { text: "О программе"; onClicked: aboutDialog.open() }
            ValueBox {
                caption: "ЗАБОЙ, м"
                bar: root.accent
                val: (root.tick, dataStore.secondsSinceUpdate() < 0 ? "—"
                      : Number(dataStore.value(chartManager.depthSensorId)).toFixed(2))
            }
            ValueBox {
                caption: "ДОЛОТО, м"
                bar: root.inkSoft
                val: (root.tick, dataStore.secondsSinceUpdate() < 0 ? "—"
                      : Number(dataStore.value(chartManager.bitSensorId)).toFixed(2))
            }
            Rectangle {
                Layout.preferredWidth: 108; Layout.preferredHeight: 40
                color: "#FFFFFF"; border.color: root.line; radius: 6
                Text {
                    id: clock
                    anchors.centerIn: parent
                    font.pixelSize: 40; font.bold: true
                    color: root.ink
                }
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
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: root.headerBand
            Layout.minimumHeight: root.headerBand
            Layout.maximumHeight: root.headerBand
            spacing: 2

            Item {
                Layout.preferredWidth: 92
                Layout.fillHeight: true
                Rectangle {
                    anchors.fill: parent
                    color: "#F2F5F7"
                    border.color: root.line
                    Column {
                        anchors.centerIn: parent
                        spacing: 2
                        Text { text: "Глубина"; font.pixelSize: 10; font.bold: true; color: root.ink; width: parent.width; horizontalAlignment: Text.AlignHCenter }
                        Text { text: "Время"; font.pixelSize: 10; font.bold: true; color: root.ink; width: parent.width; horizontalAlignment: Text.AlignHCenter }
                    }
                }
            }

            Repeater {
                model: chartManager
                delegate: Rectangle {
                    id: hdrRect
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true
                    Layout.fillHeight: true
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
                            Text { text: "Колонка " + (hdrRect.index + 1); font.pixelSize: 10; font.bold: true; color: root.ink; anchors.verticalCenter: parent.verticalCenter }
                            ToolButton { text: "+ параметр"; font.pixelSize: 9; height: 20; anchors.verticalCenter: parent.verticalCenter
                                         onClicked: traceDialog.openAdd(hdrRect.index) }
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
                                        font.pixelSize: 10
                                        font.bold: true
                                        color: modelData.color
                                        text: (root.tick, modelData.title + " = " +
                                               Number(dataStore.value(modelData.sensorId)).toFixed(2))
                                        elide: Text.ElideRight
                                    }
                                    Row {
                                        width: parent.width
                                        height: 10
                                        spacing: 0
                                        Repeater {
                                            model: 5
                                            delegate: Text {
                                                required property int index
                                                width: (parent ? parent.width : 0) / 5
                                                font.pixelSize: 7
                                                color: "#8894A0"
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
                    }
                }
            }
        }

        // ---------- Графики ----------
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 200
            spacing: 2

            TimeAxis {
                Layout.preferredWidth: 92
                Layout.fillHeight: true
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
                    Layout.fillWidth: true
                    Layout.fillHeight: true
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
                    }

                    MouseArea {
                        id: plotMA
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onWheel: function(w) { root.scroll(w.angleDelta.y > 0 ? 1 : -1) }
                        onDoubleClicked: function(m) {
                            var pt = geoChart.mapFromItem(plotMA, m.x, m.y)
                            geoChart.addMarkerAt(pt.x, pt.y, chartManager.selectedSensor)
                        }
                        onClicked: function(m) {
                            if (m.button === Qt.RightButton) {
                                var pt = geoChart.mapFromItem(plotMA, m.x, m.y)
                                geoChart.removeMarkerAt(pt.x, pt.y)
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
            Label {
                text: networkManager.connected ? "● Подключено" : "○ Отключено"
                color: networkManager.connected ? root.ok : root.bad
                font.pixelSize: 12
            }
            Label {
                text: "Колесо — архив. Двойной клик — метка."
                color: "#8894A0"
                font.pixelSize: 11
            }
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
                            : "Время сервера: " + dataStore.serverTimeString() + "  (срез " + Math.round(age) + " с назад)"
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        if (sensorCatalog.hasCsv()) root.applyCsvNames(false)
    }

    SettingsDialog { id: settingsDialog }
    AboutDialog { id: aboutDialog }
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
    Connections {
        target: chartManager
        function onConfigChanged() { root.syncWin() }
    }

    Dialog {
        id: libSave
        title: "Сохранить в библиотеку профилей"
        modal: true
        width: 440
        anchors.centerIn: Overlay.overlay
        onAboutToShow: libName.text = "profile.cfg"
        header: Rectangle {
            implicitHeight: 42
            color: root.ink
            radius: 8
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 8; color: root.ink }
            Text { anchors.centerIn: parent; text: libSave.title; color: "#FFFFFF"; font.pixelSize: 14; font.bold: true }
        }
        background: Rectangle { color: "#FFFFFF"; border.color: root.line; radius: 8 }
        contentItem: ColumnLayout {
            spacing: 10
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: "#33424F"
                font.pixelSize: 13
                text: "Имя профиля (файл .cfg в папке profiles рядом с программой):"
            }
            TextField {
                id: libName
                Layout.fillWidth: true
                font.pixelSize: 14
                font.family: "Monospace"
                color: root.ink
                background: Rectangle {
                    radius: 6
                    color: libName.activeFocus ? "#FDF8EC" : "#FFFFFF"
                    border.color: libName.activeFocus ? root.accent : root.line
                }
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: "#6B7A88"
                font.pixelSize: 11
                text: "Файлы .cfg можно копировать на другие ПК и применять через «Загрузить .cfg…» или из библиотеки."
            }
        }
        footer: DialogButtonBox {
            Button {
                text: "Сохранить"
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: {
                    if (chartManager.saveToLibrary(libName.text)) {
                        root.profilesRev++
                        root.toast("Профиль сохранён в библиотеку")
                        libSave.close()
                    } else root.toast("Ошибка: " + chartManager.lastError())
                }
            }
            Button { text: "Отмена"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
    }
    TraceDialog { id: traceDialog }

    Dialog {
        id: resetConfirm
        title: "Сброс настроек отображения"
        modal: true
        width: 430
        anchors.centerIn: Overlay.overlay
        header: Rectangle {
            implicitHeight: 42
            color: root.ink
            radius: 8
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 8; color: root.ink }
            Text { anchors.centerIn: parent; text: resetConfirm.title; color: "#FFFFFF"; font.pixelSize: 14; font.bold: true }
        }
        background: Rectangle { color: "#FFFFFF"; border.color: root.line; radius: 8 }
        contentItem: Label {
            wrapMode: Text.Wrap
            color: "#33424F"
            font.pixelSize: 13
            text: "Колонки, параметры, цвета и диапазоны будут заменены стандартным набором. Продолжить?"
        }
        footer: DialogButtonBox {
            Button {
                text: "Сбросить"
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: {
                    chartManager.resetToDefaults()
                    root.toast("Настройки отображения сброшены к стандартным")
                    resetConfirm.close()
                }
            }
            Button { text: "Отмена"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
    }
}
