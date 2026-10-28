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
    property var projectFlow: null
    property bool departing: projectFlow !== null && projectFlow.leaving
    property string exportDestination: ""
    property string documentTitle: "Screenplay"
    property var outlineScenes: []
    property bool outlineVisible: false
    property bool includeTitle: true
    property bool showReloadStatus: false
    property string sessionNotice: ""
    color: "#d4d9df"

    function toggleOutline() {
        outlineVisible = !outlineVisible
        editorPage.focusEditor()
    }
    function exportPdf() {
        if (exportJob && !exportJob.busy && !departing)
            exportJob.request(destination.text, includeTitle)
        exportPanel.close()
        editorPage.focusEditor()
    }
    Shortcut { sequence: "Ctrl+E"; onActivated: desk.exportPdf() }
    Shortcut { sequence: "Ctrl+\\"; onActivated: desk.toggleOutline() }
    Shortcut { sequence: "Ctrl+Shift+S"; enabled: !desk.departing && desk.autosaver !== null && !desk.autosaver.copyBusy && !desk.autosaver.reloadBusy; onActivated: copyPanel.open() }
    Shortcut { sequence: "Ctrl+Shift+H"; enabled: desk.projectFlow !== null && !desk.departing; onActivated: desk.projectFlow.home() }
    Connections {
        target: desk.projectFlow
        function onChanged() {
            if (desk.projectFlow.leaveFailed)
                leavePanel.open()
            else
                leavePanel.close()
        }
    }
    Connections {
        target: desk.autosaver
        function onLocationChanged() {
            desk.exportDestination = desk.autosaver.location + ".pdf"
            destination.text = desk.exportDestination
            copyDestination.text = desk.autosaver.location + " copy"
        }
        function onCopyCompleted(success) {
            if (!success)
                copyPanel.open()
        }
        function onRecoveryChanged() {
            if (desk.autosaver.reloadBusy)
                desk.showReloadStatus = true
            if (desk.autosaver.closeDialog && !failurePanel.opened)
                failurePanel.open()
            else if (!desk.autosaver.closeDialog && failurePanel.opened)
                failurePanel.close()
        }
        function onReloadCompleted(success) { desk.showReloadStatus = true }
    }
    Connections {
        target: desk.exportJob
        function onChanged() { desk.showReloadStatus = false }
    }

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
        enabled: !desk.departing
        height: (desk.autosaver ? 76 : 58) + (desk.sessionNotice.length > 0 ? 22 : 0)
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
            x: 130; y: 13; width: Math.max(60, (retryButton.visible ? retryButton.x : copyButton.visible ? copyButton.x : exportButton.x) - x - 24)
            text: desk.documentTitle
            elide: Text.ElideRight
            color: "#eff1f3"
            font { family: "Liberation Sans"; pixelSize: 14; weight: Font.DemiBold }
        }
        Text {
            id: saveStatus
            objectName: "saveStatus"
            x: 130; y: 34
            width: Math.max(60, (retryButton.visible ? retryButton.x : copyButton.visible ? copyButton.x : exportButton.x) - x - 16)
            elide: Text.ElideRight
            text: desk.autosaver ? desk.autosaver.status : "Fountain file · edits are not saved"
            color: desk.autosaver && desk.autosaver.state === "failed" ? "#f19a8f" : "#aeb7c0"
            font { family: "Liberation Sans"; pixelSize: 11 }
            ToolTip.visible: statusArea.containsMouse && saveStatus.truncated
            ToolTip.text: saveStatus.text
            MouseArea { id: statusArea; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
        }
        Text {
            id: projectLocation
            objectName: "projectLocation"
            x: 130; y: 54; width: parent.width - x - 24
            text: desk.autosaver ? desk.autosaver.location : ""
            elide: Text.ElideMiddle
            color: "#aeb7c0"
            font { family: "Liberation Sans"; pixelSize: 11 }
            ToolTip.visible: locationArea.containsMouse
            ToolTip.text: text
            MouseArea { id: locationArea; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
        }
        Text {
            objectName: "sessionNotice"
            x: 130; y: 76; width: parent.width - x - 24
            visible: desk.sessionNotice.length > 0
            text: desk.sessionNotice; textFormat: Text.PlainText
            elide: Text.ElideRight; color: "#f19a8f"
            font { family: "Liberation Sans"; pixelSize: 11 }
            ToolTip.visible: sessionArea.containsMouse
            ToolTip.text: text
            MouseArea { id: sessionArea; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
        }
        DeskButton {
            id: retryButton
            objectName: "retrySave"
            anchors { right: reloadButton.left; rightMargin: 8; top: parent.top; topMargin: 10 }
            visible: desk.autosaver !== null && desk.autosaver.state === "failed"
            text: "Retry save"
            fill: "#8f3a32"; ink: "#fbecea"
            Accessible.name: "Retry saving to " + (desk.autosaver ? desk.autosaver.location : "")
            onClicked: { desk.autosaver.retry(); editorPage.focusEditor() }
        }
        DeskButton {
            id: reloadButton
            objectName: "reloadProject"
            anchors { right: copyButton.left; rightMargin: 8; top: parent.top; topMargin: 10 }
            visible: retryButton.visible
            enabled: desk.autosaver !== null && !desk.autosaver.copyBusy && !desk.autosaver.reloadBusy
            text: "Reload…"; ink: "#eff1f3"
            onClicked: reloadPanel.open()
        }
        DeskButton {
            id: copyButton
            objectName: "saveCopy"
            anchors { right: exportButton.left; rightMargin: 8; top: parent.top; topMargin: 10 }
            visible: desk.autosaver !== null
            text: desk.autosaver && desk.autosaver.copyBusy ? "Copying…" : "Save a Copy…"
            enabled: desk.autosaver !== null && !desk.autosaver.copyBusy && !desk.autosaver.reloadBusy
            ink: "#eff1f3"
            Accessible.name: "Save a Copy (Ctrl+Shift+S)"
            onClicked: copyPanel.open()
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
        enabled: !desk.departing
        width: desk.width < 700 ? 52 : 74
        anchors { left: parent.left; top: header.bottom; bottom: parent.bottom }
        color: "#3c434d"
        Column {
            anchors { top: parent.top; topMargin: 24; horizontalCenter: parent.horizontalCenter }
            spacing: 12
            DeskButton {
                objectName: "goHome"
                width: 44; height: 44; text: "Home"; ink: "#eff1f3"; padding: 0
                visible: desk.projectFlow !== null
                enabled: desk.autosaver !== null && !desk.departing && !desk.autosaver.copyBusy && !desk.autosaver.reloadBusy
                Accessible.name: "Save and return Home (Ctrl+Shift+H)"
                onClicked: desk.projectFlow.home()
            }
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
            enabled: (!desk.autosaver || !desk.autosaver.reloadBusy) && !desk.departing
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
            objectName: "operationStatus"
            text: desk.screenplayLayout.error || (desk.autosaver && (desk.autosaver.copyBusy || desk.autosaver.copyError) ? desk.autosaver.copyStatus : "") || (desk.autosaver && desk.showReloadStatus ? desk.autosaver.reloadStatus : "") || (desk.exportJob ? desk.exportJob.status : "")
            elide: Text.ElideMiddle
            ToolTip.visible: footerArea.containsMouse
            ToolTip.text: text
            MouseArea { id: footerArea; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
            color: desk.screenplayLayout.error ? "#9d3030" : "#4d5965"
            font { family: "Liberation Sans"; pixelSize: 11 }
        }
    }
    Popup {
        id: leavePanel
        objectName: "leavePanel"
        parent: desk
        x: Math.max(8, (desk.width - width) / 2); y: header.height + 8
        width: Math.min(650, desk.width - 16)
        height: Math.min(desk.height - header.height - 16, leaveContents.implicitHeight + 40)
        padding: 20; modal: true; focus: true
        onClosed: {
            if (desk.projectFlow) desk.projectFlow.keep_writing()
            editorPage.focusEditor()
        }
        background: Rectangle { color: "#f5f4f1"; radius: 8; border.color: "#b9c0c7" }
        ScrollView {
            id: leaveScroll
            anchors.fill: parent
            contentWidth: availableWidth
            Column {
                id: leaveContents; width: leaveScroll.availableWidth; spacing: 14
                Label { text: "Save before leaving"; font.pixelSize: 18 }
                Label {
                    width: parent.width; wrapMode: Text.WrapAnywhere; textFormat: Text.PlainText
                    text: desk.autosaver ? desk.autosaver.status : ""
                }
                Label {
                    width: parent.width; wrapMode: Text.Wrap
                    text: "Your writing is still open here. Retry the project save, or save and use a new copy, then return Home. An emergency copy alone does not leave this project."
                }
                Flow {
                    width: parent.width; spacing: 12
                    DeskButton {
                        objectName: "leaveKeepWriting"; text: "Keep Writing"; ink: "#303843"
                        onClicked: desk.projectFlow.keep_writing()
                    }
                    DeskButton {
                        objectName: "leaveRetry"; text: "Retry and Leave"; fill: "#3c434d"; ink: "#eff1f3"
                        onClicked: desk.projectFlow.retry_leave()
                    }
                    DeskButton {
                        objectName: "leaveSaveCopy"; text: "Save a Copy…"; ink: "#303843"
                        onClicked: { desk.projectFlow.keep_writing(); copyPanel.open() }
                    }
                }
            }
        }
    }
    Label {
        anchors.centerIn: parent
        visible: desk.departing
        padding: 20; text: "Saving writing and finishing background work…"
        color: "#303843"
        background: Rectangle { color: "#f5f4f1"; radius: 8; border.color: "#b9c0c7" }
    }
    Popup {
        id: reloadPanel
        objectName: "reloadPanel"
        parent: desk
        x: Math.max(8, (desk.width - width) / 2); y: header.height + 8
        width: Math.min(560, desk.width - 16)
        height: reloadContents.implicitHeight + 40
        padding: 20; modal: true; focus: true
        background: Rectangle { color: "#f5f4f1"; radius: 8; border.color: "#b9c0c7" }
        onClosed: editorPage.focusEditor()
        Column {
            id: reloadContents; width: parent.width; spacing: 14
            Label { text: "Preserve Writing and Reload"; font.pixelSize: 16 }
            Label {
                width: parent.width; wrapMode: Text.Wrap
                text: "First save a verified emergency copy of the writing in this window, then read the project from disk. Reload replaces this window’s writing and clears undo history. If preservation or reading fails, keep the current writing."
            }
            Flow {
                width: parent.width; spacing: 12
                DeskButton { text: "Keep Writing"; ink: "#303843"; onClicked: reloadPanel.close() }
                DeskButton {
                    objectName: "reloadConfirm"
                    text: "Preserve and Reload"; fill: "#3c434d"; ink: "#eff1f3"
                    onClicked: { if (desk.autosaver.reload()) reloadPanel.close() }
                }
            }
        }
    }
    Popup {
        id: failurePanel
        objectName: "failurePanel"
        parent: desk
        x: Math.max(8, (desk.width - width) / 2); y: header.height + 8
        width: Math.min(700, desk.width - 16)
        height: failureContents.implicitHeight + 40
        padding: 20; modal: true; focus: true
        onClosed: { if (desk.autosaver) desk.autosaver.keep_open(); editorPage.focusEditor() }
        background: Rectangle { color: "#f5f4f1"; radius: 8; border.color: "#b9c0c7" }
        Column {
            id: failureContents; width: parent.width; spacing: 14
            Label { text: "Project Not Saved"; font.pixelSize: 16 }
            Label {
                objectName: "closeProtection"
                width: parent.width; wrapMode: Text.WrapAnywhere
                text: desk.autosaver ? desk.autosaver.status : ""
            }
            Flow {
                width: parent.width; spacing: 12
                DeskButton { text: "Keep Open"; ink: "#303843"; onClicked: desk.autosaver.keep_open() }
                DeskButton { text: "Retry"; ink: "#303843"; onClicked: desk.autosaver.retry() }
                DeskButton { text: "Save a Copy…"; ink: "#303843"; onClicked: { failurePanel.close(); copyPanel.open() } }
                DeskButton {
                    objectName: "closeExplicit"
                    text: desk.autosaver && desk.autosaver.currentProtected ? "Close with Emergency Copy" : "Discard Unprotected Writing and Close"
                    fill: "#8f3a32"; ink: "#fbecea"
                    onClicked: desk.autosaver.close_with_copy_or_discard()
                }
            }
        }
    }
    Popup {
        id: copyPanel
        objectName: "copyPanel"
        parent: desk
        x: Math.max(8, (desk.width - width) / 2); y: header.height + 8
        width: Math.min(520, desk.width - 16)
        height: copyContents.implicitHeight + 40
        padding: 20
        modal: true; focus: true
        onOpened: {
            if (!desk.autosaver.copyBusy) {
                if (copyDestination.text.length === 0)
                    copyDestination.text = desk.autosaver.location + " copy"
                copyDestination.forceActiveFocus()
                copyDestination.selectAll()
            }
        }
        onClosed: editorPage.focusEditor()
        background: Rectangle { color: "#f5f4f1"; radius: 8; border.color: "#b9c0c7" }
        Column {
            id: copyContents
            width: parent.width
            spacing: 14
            Text { text: "Save a Copy"; color: "#303843"; font { family: "Liberation Sans"; pixelSize: 16; weight: Font.DemiBold } }
            Label {
                width: parent.width
                text: "Choose a new project folder inside an existing folder. Writing will continue in the copy."
                wrapMode: Text.Wrap
            }
            TextField {
                id: copyDestination
                objectName: "copyDestination"
                width: parent.width
                enabled: !desk.autosaver || !desk.autosaver.copyBusy
                selectByMouse: true
                Accessible.name: "New project folder (absolute path)"
                onAccepted: copyConfirm.clicked()
            }
            Label {
                objectName: "copyMessage"
                width: parent.width
                visible: text.length > 0
                text: desk.autosaver ? desk.autosaver.copyStatus : ""
                wrapMode: Text.WrapAnywhere
                color: desk.autosaver && desk.autosaver.copyError ? "#9d3030" : "#4d5965"
            }
            Row {
                spacing: 12
                DeskButton {
                    text: "Keep Writing"; ink: "#303843"
                    onClicked: copyPanel.close()
                }
                DeskButton {
                    id: copyConfirm
                    objectName: "copyConfirm"
                    text: "Save and Use Copy"; fill: "#3c434d"; ink: "#eff1f3"
                    enabled: desk.autosaver !== null && !desk.autosaver.copyBusy && !desk.autosaver.reloadBusy && copyDestination.text.trim().length > 0
                    onClicked: {
                        if (enabled && desk.autosaver.save_copy(copyDestination.text))
                            copyPanel.close()
                    }
                }
            }
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
