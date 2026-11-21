import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Qt.labs.folderlistmodel 2.15

Dialog {
    id: dlg
    property string mode: "open"
    property url curFolder: ""
    property string selectedFile: ""     // локальный путь выбранного файла (строка)
    property int selIndex: -1
    property bool showAll: false
    signal chosen(string path)

    title: mode === "save" ? "Сохранить профиль как .cfg" : "Загрузить профиль (.cfg / .json)"
    modal: true
    width: 680
    height: 560
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape

    readonly property var win: ApplicationWindow.window

    function localPath(u) {
        var s = u.toString()
        if (s.indexOf("file://") === 0) s = s.substring(7)
        return decodeURIComponent(s)
    }
    function baseName(p) {
        var i = p.lastIndexOf("/")
        return i < 0 ? p : p.substring(i + 1)
    }
    function refresh() {
        var f = dlg.curFolder
        fm.folder = ""
        fm.folder = f
    }
    function clearSel() {
        selectedFile = ""
        selIndex = -1
    }

    onAboutToShow: {
        clearSel()
        showAll = false
        curFolder = "file://" + chartManager.appDir()
        if (mode === "save") nameField.text = "profile.cfg"
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
        spacing: 8

        RowLayout {
            spacing: 6
            Button {
                text: "↑ Вверх"
                font.pixelSize: 12
                onClicked: if (fm.parentFolder) { dlg.curFolder = fm.parentFolder; dlg.clearSel() }
            }
            Label {
                Layout.fillWidth: true
                text: dlg.localPath(dlg.curFolder)
                color: dlg.win.inkSoft
                font.pixelSize: 11
                font.family: "Monospace"
                elide: Text.ElideMiddle
            }
            Label { text: fm.count + " файлов"; color: "#8894A0"; font.pixelSize: 11 }
        }

        RowLayout {
            spacing: 6
            Button { text: "Программа"; font.pixelSize: 11
                     onClicked: { dlg.curFolder = "file://" + chartManager.appDir(); dlg.clearSel() } }
            Button { text: "Библиотека профилей"; font.pixelSize: 11
                     onClicked: { dlg.curFolder = "file://" + chartManager.libraryDir(); dlg.clearSel() } }
            Button { text: "Домой"; font.pixelSize: 11
                     onClicked: { dlg.curFolder = "file://" + chartManager.homeDir(); dlg.clearSel() } }
            Item { Layout.fillWidth: true }
            CheckBox {
                id: allChk
                visible: dlg.mode === "open"
                text: "Все файлы"
                font.pixelSize: 11
                checked: dlg.showAll
                onToggled: dlg.showAll = checked
            }
            Label {
                visible: dlg.mode === "open" && !dlg.showAll
                text: "Показаны *.cfg и *.json"
                color: "#8894A0"; font.pixelSize: 11
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 6
            color: "#FBFCFD"
            border.color: dlg.win.line

            Text {
                anchors.centerIn: parent
                width: parent.width - 60
                visible: fm.count === 0
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: "#8894A0"
                font.pixelSize: 12
                text: dlg.mode === "open"
                      ? "В этой папке нет профилей.\nСохраните профиль («Сохранить как .cfg…»), включите «Все файлы» или выберите другую папку."
                      : "Папка пуста — введите имя файла ниже и нажмите «Сохранить»."
            }

            ListView {
                id: fileView
                anchors.fill: parent
                anchors.margins: 4
                clip: true
                model: FolderListModel {
                    id: fm
                    folder: dlg.curFolder
                    showDirs: true
                    showFiles: true
                    showDotAndDotDot: false
                    nameFilters: dlg.mode === "open"
                                 ? (dlg.showAll ? ["*"] : ["*.cfg", "*.json"])
                                 : ["*"]
                    sortField: FolderListModel.Name
                }
                delegate: Rectangle {
                    id: row
                    required property var model
                    required property int index
                    width: fileView.width
                    height: 30
                    radius: 5
                    readonly property bool isSel: dlg.selIndex === index && !model.fileIsDir
                    color: ma.pressed ? "#F6E3B8"
                         : ma.hovered ? "#FDF3DC"
                         : isSel ? "#E8F0F7" : "transparent"
                    border.color: isSel ? dlg.win.accent : "transparent"
                    border.width: 1
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        anchors.rightMargin: 8
                        spacing: 6
                        Text { text: model.fileIsDir ? "📁" : "📄"; font.pixelSize: 12 }
                        Text {
                            Layout.fillWidth: true
                            text: model.fileName
                            color: dlg.win.ink
                            font.pixelSize: 12
                            font.bold: model.fileIsDir || row.isSel || model.fileName === "charts.json"
                            elide: Text.ElideRight
                        }
                        Text {
                            visible: !model.fileIsDir
                            text: (model.fileSize / 1024).toFixed(1) + " КБ"
                            color: "#8894A0"; font.pixelSize: 10
                        }
                    }
                    MouseArea {
                        id: ma
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: model.fileIsDir ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: {
                            if (model.fileIsDir) {
                                dlg.curFolder = model.fileURL
                                dlg.clearSel()
                            } else {
                                dlg.selIndex = index
                                dlg.selectedFile = dlg.localPath(model.fileURL)
                            }
                        }
                        onDoubleClicked: {
                            if (!model.fileIsDir && dlg.mode === "open") {
                                dlg.chosen(dlg.localPath(model.fileURL))
                                dlg.close()
                            }
                        }
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

        RowLayout {
            visible: dlg.mode === "save"
            spacing: 8
            Label { text: "Имя файла:"; color: dlg.win.ink; font.pixelSize: 13; font.bold: true }
            TextField {
                id: nameField
                Layout.fillWidth: true
                font.pixelSize: 14
                font.family: "Monospace"
                color: dlg.win.ink
                background: Rectangle {
                    radius: 6
                    color: nameField.activeFocus ? "#FDF8EC" : "#FFFFFF"
                    border.color: nameField.activeFocus ? dlg.win.accent : dlg.win.line
                }
            }
            Label { text: ".cfg добавится автоматически"; color: "#8894A0"; font.pixelSize: 11 }
        }

        RowLayout {
            spacing: 8
            Label {
                Layout.fillWidth: true
                elide: Text.ElideMiddle
                font.pixelSize: 11
                color: dlg.selectedFile !== "" ? dlg.win.inkSoft : "#8894A0"
                text: dlg.mode === "open"
                      ? (dlg.selectedFile !== "" ? "Выбран: " + dlg.baseName(dlg.selectedFile)
                                                 : "Кликните файл в списке (или двойной клик — сразу загрузить)")
                      : "Файл будет сохранён в текущей папке и в библиотеке профилей"
            }
            Button {
                text: "Удалить"
                font.pixelSize: 12
                enabled: dlg.selectedFile !== ""
                onClicked: {
                    if (chartManager.deleteFile(dlg.selectedFile)) {
                        dlg.clearSel()
                        dlg.refresh()
                        dlg.win.toast("Файл удалён")
                    } else dlg.win.toast("Ошибка: " + chartManager.lastError())
                }
            }
            Button {
                id: actBtn
                text: dlg.mode === "save" ? "Сохранить" : "Загрузить"
                font.pixelSize: 13
                font.bold: true
                enabled: dlg.mode === "save" ? nameField.text.trim() !== "" : dlg.selectedFile !== ""
                contentItem: Text {
                    text: parent.text
                    color: parent.enabled ? "#FFFFFF" : "#9AA7B2"
                    font: parent.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    implicitHeight: 34
                    implicitWidth: 130
                    color: parent.enabled ? (parent.down ? dlg.win.ink : dlg.win.inkSoft) : "#E1E6EA"
                    border.color: parent.enabled ? "transparent" : dlg.win.line
                }
                onClicked: {
                    if (dlg.mode === "save") {
                        var n = nameField.text.trim()
                        if (n.toLowerCase().indexOf(".cfg", n.length - 4) === -1) n += ".cfg"
                        dlg.chosen(dlg.localPath(dlg.curFolder) + "/" + n)
                    } else {
                        dlg.chosen(dlg.selectedFile)
                    }
                    dlg.close()
                }
            }
            Button { text: "Отмена"; font.pixelSize: 13; onClicked: dlg.close() }
        }
    }
}
