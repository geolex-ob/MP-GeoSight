import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Qt.labs.folderlistmodel 2.15

Dialog {
    id: dlg
    property string mode: "open"
    property url curFolder: ""
    property string selectedFile: ""
    signal chosen(string path)

    title: mode === "save" ? "Сохранить профиль как .cfg" : "Загрузить профиль .cfg"
    modal: true
    width: 660
    height: 540
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape

    readonly property var win: ApplicationWindow.window

    function localPath(u) {
        var s = u.toString()
        if (s.indexOf("file://") === 0) s = s.substring(7)
        return decodeURIComponent(s)
    }
    function refresh() {
        var f = dlg.curFolder
        fm.folder = ""
        fm.folder = f
    }

    onAboutToShow: {
        selectedFile = ""
        curFolder = mode === "save" ? "file://" + chartManager.appDir()
                                    : "file://" + chartManager.libraryDir()
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
                onClicked: if (fm.parentFolder) { dlg.curFolder = fm.parentFolder; dlg.selectedFile = "" }
            }
            Label {
                Layout.fillWidth: true
                text: dlg.localPath(dlg.curFolder)
                color: dlg.win.inkSoft
                font.pixelSize: 11
                font.family: "Monospace"
                elide: Text.ElideMiddle
            }
        }

        RowLayout {
            spacing: 6
            Button { text: "Программа"; font.pixelSize: 11
                     onClicked: { dlg.curFolder = "file://" + chartManager.appDir(); dlg.selectedFile = "" } }
            Button { text: "Библиотека профилей"; font.pixelSize: 11
                     onClicked: { dlg.curFolder = "file://" + chartManager.libraryDir(); dlg.selectedFile = "" } }
            Button { text: "Домой"; font.pixelSize: 11
                     onClicked: { dlg.curFolder = "file://" + chartManager.homeDir(); dlg.selectedFile = "" } }
            Item { Layout.fillWidth: true }
            Label {
                text: dlg.mode === "open" ? "Показаны файлы *.cfg" : "Все файлы"
                color: "#8894A0"; font.pixelSize: 11
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 6
            color: "#FBFCFD"
            border.color: dlg.win.line
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
                    nameFilters: dlg.mode === "open" ? ["*.cfg"] : ["*"]
                    sortField: FolderListModel.Name
                }
                delegate: Rectangle {
                    required property var model
                    required property int index
                    width: fileView.width
                    height: 28
                    radius: 4
                    color: ma.hovered ? "#FDF3DC"
                         : (dlg.selectedFile === model.fileURL ? "#E8F0F7" : "transparent")
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 4
                        spacing: 6
                        Text { text: model.fileIsDir ? "📁" : "📄"; font.pixelSize: 12 }
                        Text {
                            Layout.fillWidth: true
                            text: model.fileName
                            color: dlg.win.ink
                            font.pixelSize: 12
                            font.bold: model.fileIsDir
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
                        onClicked: {
                            if (model.fileIsDir) { dlg.curFolder = model.fileURL; dlg.selectedFile = "" }
                            else dlg.selectedFile = model.fileURL
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
            Button {
                text: "Удалить файл"
                font.pixelSize: 12
                enabled: dlg.selectedFile !== ""
                onClicked: {
                    if (chartManager.deleteFile(dlg.localPath(dlg.selectedFile))) {
                        dlg.selectedFile = ""
                        dlg.refresh()
                        dlg.win.toast("Файл удалён")
                    } else dlg.win.toast("Ошибка: " + chartManager.lastError())
                }
            }
            Item { Layout.fillWidth: true }
            Button {
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
                }
                onClicked: {
                    if (dlg.mode === "save") {
                        var n = nameField.text.trim()
                        if (n.toLowerCase().indexOf(".cfg", n.length - 4) === -1) n += ".cfg"
                        dlg.chosen(dlg.localPath(dlg.curFolder) + "/" + n)
                    } else {
                        dlg.chosen(dlg.localPath(dlg.selectedFile))
                    }
                    dlg.close()
                }
            }
            Button { text: "Отмена"; font.pixelSize: 13; onClicked: dlg.close() }
        }
    }
}
