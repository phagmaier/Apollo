import QtQuick
import QtQuick.Controls.Basic
import "../editor"

// Bounded look check: the rail and opening scene list reproduce prototype B.
// The outline is read-only; navigation/reordering and other workspaces wait.
Rectangle {
    id: desk
    required property var screenplayLayout
    required property var commands
    property var exportJob: null
    property var autosaver: null
    property string exportDestination: ""
    property string documentTitle: "Screenplay"
    property var outlineScenes: []
    property bool outlineVisible: false
    property bool includeTitle: true
    color: "#d4d9df"

    function toggleOutline() {
        outlineVisible = !outlineVisible
        editorPage.focusEditor()
    }
    function exportPdf() {
        if (exportJob && !exportJob.busy)
            exportJob.request(destination.text, includeTitle)
        exportPanel.close()
        editorPage.focusEditor()
    }
    Shortcut { sequence: "Ctrl+E"; onActivated: desk.exportPdf() }
    Shortcut { sequence: "Ctrl+\\"; onActivated: desk.toggleOutline() }

    component DeskButton: Button {
        id: button
        property color fill: "transparent"
        property color ink: "#b7c0c8"
        font.family: "Liberation Sans"
        font.pixelSize: 12
        padding: 10
        background: Rectangle {
            radius: 7
            color: button.down ? "#7a8796" : button.hovered ? "#637080" : button.fill
            border.width: button.visualFocus ? 2 : 0
            border.color: "#90a9e1"
        }
        contentItem: Text {
            text: button.text
            font: button.font
            color: button.ink
            opacity: button.enabled ? 1 : 0.35
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        ToolTip.visible: hovered
        ToolTip.delay: 600
        ToolTip.text: Accessible.name
    }

    Rectangle {
        id: header
        height: 58
        anchors { left: parent.left; right: parent.right; top: parent.top }
        color: "#323842"
        Text {
            id: logo
            x: 28; anchors.verticalCenter: parent.verticalCenter
            text: "ap<span style='color:#7a83af'>o</span>llo"
            textFormat: Text.StyledText
            font { family: "Liberation Sans"; pixelSize: 23; weight: Font.Bold; letterSpacing: -1.2 }
            color: "#eff1f3"
        }
        Rectangle { x: 110; y: 17; width: 1; height: 25; color: "#515761" }
        Text {
            x: 130; y: 13; width: Math.max(60, (retryButton.visible ? retryButton.x : exportButton.x) - x - 24)
            text: desk.documentTitle
            elide: Text.ElideRight
            color: "#eff1f3"
            font { family: "Liberation Sans"; pixelSize: 14; weight: Font.DemiBold }
        }
        Text {
            id: saveStatus
            objectName: "saveStatus"
            x: 130; y: 34
            width: Math.max(60, (retryButton.visible ? retryButton.x : exportButton.x) - x - 16)
            elide: Text.ElideRight
            text: desk.autosaver ? desk.autosaver.status : "Fountain file · edits are not saved"
            color: desk.autosaver && desk.autosaver.state === "failed" ? "#f19a8f" : "#aeb7c0"
            font { family: "Liberation Sans"; pixelSize: 11 }
            ToolTip.visible: statusArea.containsMouse && saveStatus.truncated
            ToolTip.text: saveStatus.text
            MouseArea { id: statusArea; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
        }
        DeskButton {
            id: retryButton
            objectName: "retrySave"
            anchors { right: exportButton.left; rightMargin: 12; verticalCenter: parent.verticalCenter }
            visible: desk.autosaver !== null && desk.autosaver.state === "failed"
            text: "Retry save"
            fill: "#8f3a32"; ink: "#fbecea"
            Accessible.name: "Retry saving to " + (desk.autosaver ? desk.autosaver.location : "")
            onClicked: { desk.autosaver.retry(); editorPage.focusEditor() }
        }
        DeskButton {
            id: exportButton
            objectName: "exportButton"
            anchors { right: parent.right; rightMargin: 28; verticalCenter: parent.verticalCenter }
            text: desk.exportJob && desk.exportJob.busy ? "Exporting…" : "Export"
            fill: "#e9edf0"; ink: "#303843"
            font.weight: Font.DemiBold
            enabled: desk.exportJob !== null && !desk.exportJob.busy
            Accessible.name: "Export PDF options"
            onClicked: exportPanel.open()
        }
    }

    Rectangle {
        id: rail
        width: desk.width < 700 ? 52 : 74
        anchors { left: parent.left; top: header.bottom; bottom: parent.bottom }
        color: "#3c434d"
        Column {
            anchors { top: parent.top; topMargin: 24; horizontalCenter: parent.horizontalCenter }
            spacing: 12
            DeskButton {
                width: 44; height: 44; text: "▤"; font.pixelSize: 22
                fill: "#637080"; ink: "white"
                Accessible.name: "Screenplay"
                onClicked: editorPage.focusEditor()
            }
            DeskButton {
                objectName: "outlineToggle"
                width: 44; height: 44; text: "☰"; font.pixelSize: 21
                fill: desk.outlineVisible ? "#637080" : "transparent"
                ink: desk.outlineVisible ? "white" : "#b7c0c8"
                Accessible.name: desk.outlineVisible ? "Hide scene outline" : "Show scene outline"
                Accessible.checkable: true
                Accessible.checked: desk.outlineVisible
                onClicked: desk.toggleOutline()
            }
            DeskButton {
                width: 44; height: 44; text: "▧"; font.pixelSize: 22; enabled: false
                Accessible.name: "Notes (outside this spike)"
            }
            DeskButton {
                width: 44; height: 44; text: "◇"; font.pixelSize: 24; enabled: false
                Accessible.name: "Stash (outside this spike)"
            }
        }
    }

    Rectangle {
        id: outline
        objectName: "sceneOutline"
        anchors { left: rail.right; top: header.bottom; bottom: parent.bottom }
        width: desk.outlineVisible ? Math.min(280, desk.width * 0.36) : 0
        visible: desk.outlineVisible
        clip: true
        color: "#f5f4f1"
        Rectangle { anchors.right: parent.right; height: parent.height; width: 1; color: "#c9cdd2" }
        Text {
            x: 24; y: 26; text: "Scenes"; color: "#242733"
            font { family: "Liberation Sans"; pixelSize: 16; weight: Font.DemiBold }
        }
        DeskButton {
            objectName: "outlineClose"
            anchors { right: parent.right; rightMargin: 12; top: parent.top; topMargin: 16 }
            width: 36; height: 36; text: "×"; font.pixelSize: 23; ink: "#69706d"
            Accessible.name: "Close scene outline"
            onClicked: desk.toggleOutline()
        }
        Text {
            x: 24; y: 64; width: parent.width - 48
            text: desk.documentTitle
            elide: Text.ElideRight; color: "#777f7c"
            font { family: "Liberation Sans"; pixelSize: 11 }
        }
        Text {
            x: 24; y: 98
            text: desk.outlineScenes.length + " scenes · at opening"
            color: "#777f7c"; font { family: "Liberation Sans"; pixelSize: 11 }
        }
        ListView {
            objectName: "outlineList"
            anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: 125; bottom: parent.bottom; margins: 18 }
            clip: true; spacing: 5
            model: desk.outlineScenes
            delegate: Rectangle {
                required property string modelData
                required property int index
                width: ListView.view.width
                height: sceneLabel.implicitHeight + 42
                radius: 8
                color: index === 0 ? "white" : "transparent"
                Text {
                    id: sceneLabel
                    x: 12; y: 13; width: parent.width - 24
                    text: (index + 1) + "   " + modelData
                    wrapMode: Text.Wrap
                    color: index === 0 ? "#3a4651" : "#69706d"
                    font { family: "Liberation Sans"; pixelSize: 11; weight: Font.DemiBold }
                }
                Text {
                    x: 12; y: sceneLabel.y + sceneLabel.implicitHeight + 6
                    text: "Scene " + (index + 1)
                    color: "#7b827d"; font { family: "Liberation Sans"; pixelSize: 11 }
                }
            }
            ScrollBar.vertical: ScrollBar {}
        }
    }

    Item {
        id: workspace
        anchors { left: outline.right; right: parent.right; top: header.bottom; bottom: footer.top }
        Item {
            id: workbar
            anchors { left: parent.left; right: parent.right; top: parent.top }
            height: 57
            Row {
                anchors.centerIn: parent
                spacing: 12
                Text { text: "Screenplay"; color: "#4d5965"; font { family: "Liberation Sans"; pixelSize: 12; weight: Font.DemiBold } }
                Text { text: desk.screenplayLayout.pageCount + (desk.screenplayLayout.pageCount === 1 ? " page" : " pages"); color: "#626e78"; font { family: "Liberation Sans"; pixelSize: 12 } }
                Text { text: "100%"; color: "#626e78"; font { family: "Liberation Sans"; pixelSize: 12 } }
            }
        }
        ScreenplayEditor {
            id: editorPage
            anchors { left: parent.left; right: parent.right; top: workbar.bottom; bottom: parent.bottom }
            screenplayLayout: desk.screenplayLayout
            commands: desk.commands
        }
    }
    Rectangle {
        id: footer
        anchors { left: outline.right; right: parent.right; bottom: parent.bottom }
        height: 30
        color: "#d4d9df"
        Text {
            anchors { left: parent.left; right: parent.right; margins: 20; verticalCenter: parent.verticalCenter }
            text: desk.screenplayLayout.error || (desk.exportJob ? desk.exportJob.status : "")
            elide: Text.ElideMiddle
            color: desk.screenplayLayout.error ? "#9d3030" : "#4d5965"
            font { family: "Liberation Sans"; pixelSize: 11 }
        }
    }
    Popup {
        id: exportPanel
        objectName: "exportPanel"
        parent: desk
        x: Math.max(8, desk.width - width - 28); y: header.height + 8
        width: Math.min(460, desk.width - 16); height: 200
        padding: 20
        modal: true; focus: true
        onClosed: editorPage.focusEditor()
        background: Rectangle { color: "#f5f4f1"; radius: 8; border.color: "#b9c0c7" }
        Text { text: "Export PDF snapshot"; color: "#303843"; font { family: "Liberation Sans"; pixelSize: 16; weight: Font.DemiBold } }
        TextField {
            id: destination
            objectName: "exportDestination"
            y: 36; width: parent.width
            text: desk.exportDestination
            selectByMouse: true; font.pixelSize: 12
            Accessible.name: "PDF destination"
        }
        CheckBox {
            y: 77; text: "Include title page"; checked: desk.includeTitle
            onToggled: desk.includeTitle = checked
        }
        DeskButton {
            anchors { right: parent.right; bottom: parent.bottom }
            text: "Export PDF"; fill: "#3c434d"; ink: "#eff1f3"
            enabled: desk.exportJob !== null && !desk.exportJob.busy
            onClicked: desk.exportPdf()
        }
    }
}
