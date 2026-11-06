import QtQuick
import QtQuick.Controls.Basic
import "../editor"

// Writing desk foundation with the live scene navigator.
Rectangle {
    id: desk
    required property var screenplayLayout
    required property var commands
    property var exportJob: null
    property var fountainJob: null
    property string exportFormat: "pdf"
    property string fountainCheck: ""
    property string fountainPresentationNote: ""
    property string lastExport: "pdf"
    property var autosaver: null
    property var projectFlow: null
    property bool departing: projectFlow !== null && projectFlow.leaving
    property string exportDestination: ""
    property string documentTitle: "Screenplay"
    readonly property string shownTitle: commands.titleText.length > 0 ? commands.titleText : documentTitle
    readonly property var outlineScenes: commands.scenes
    property bool outlineVisible: false
    // Writer layout; remembered by the project flow when there is one.
    property bool sidebarVisible: true
    property bool toolbarVisible: true
    onProjectFlowChanged: if (projectFlow) { sidebarVisible = projectFlow.sidebarVisible; toolbarVisible = projectFlow.toolbarVisible }
    property bool includeTitle: true
    property bool showReloadStatus: false
    property string sessionNotice: ""
    property var structureIssues: commands.structureIssues
    property string issueKey: ""
    property string issueNotice: ""
    readonly property bool dialogOpen: shortcutPanel.visible || titlePanel.visible || importedPanel.visible
            || issuePanel.visible || exportPanel.visible || pdfWaitPanel.visible || fountainWaitPanel.visible || copyPanel.visible || reloadPanel.visible
            || failurePanel.visible || leavePanel.visible || editorPage.spellingOpen || editorPage.findOpen
            || elementTypeMenu.visible
    property var selectedIssue: {
        for (const issue of structureIssues) {
            if (issue.key === issueKey)
                return issue
        }
        return null
    }
    color: "#e8eaee"

    function setView(sidebar, toolbar) {
        sidebarVisible = sidebar
        toolbarVisible = toolbar
        if (projectFlow) projectFlow.set_view(sidebar, toolbar)
        editorPage.focusEditor()
    }
    function openElementPicker() {
        if (editorPage.enabled && !departing) elementTypeMenu.open()
    }
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
    function openExport(format) {
        exportFormat = fountainJob ? format : "pdf"
        refreshExportCheck()
        exportPanel.open()
    }
    // The modal panel stops writing, so one check per open/choice is current.
    function refreshExportCheck() {
        fountainCheck = fountainJob && exportFormat === "fountain" ? fountainJob.check(includeTitle) : ""
        fountainPresentationNote = fountainJob && exportFormat === "fountain" ? fountainJob.presentationNote() : ""
    }
    function exportFountain() {
        if (fountainJob && !fountainJob.busy && !departing)
            fountainJob.request(fountainDestination.text, includeTitle)
        exportPanel.close()
        editorPage.focusEditor()
    }
    function exportEligibility() {
        if (exportFormat === "pdf")
            return exportJob && exportJob.unavailable.length > 0 ? exportJob.unavailable : ""
        if (!fountainJob)
            return ""
        if (fountainCheck.length > 0) {
            const reasons = fountainCheck.split("\n")
            return "Not exportable as clean Fountain yet:\n• " + reasons.slice(0, 8).join("\n• ")
                    + (reasons.length > 8 ? "\n…and " + (reasons.length - 8) + " more" : "")
        }
        return "Ready. The current writing meets the canonical Fountain contract; later edits are not included in this snapshot."
                + (fountainPresentationNote.length > 0 ? "\n" + fountainPresentationNote : "")
                + (fountainJob.omissions.length > 0 ? "\n" + fountainJob.omissions : "")
    }
    function issueLabel(kind) {
        return kind === "emptySpeech" ? "Empty speech"
             : kind === "speechWithoutCue" ? "Dialogue without a cue"
             : kind === "adjacentDialogue" ? "Dialogue that runs together"
             : "Incomplete Dual Dialogue"
    }
    function selectIssue(key) {
        const range = commands.locateStructureIssue(key)
        issueNotice = ""
        if (range.start === undefined) {
            issueNotice = "This issue changed. Choose a current issue to review."
            return
        }
        issueKey = key
        editorPage.selectElement(range.start, range.end - range.start)
    }
    function correctSelectedIssue(action) {
        if (!selectedIssue)
            return
        const position = commands.correctStructureIssue(issueKey, action)
        if (position < 0) {
            issueNotice = "This issue changed or now needs a manual edit. Choose a current issue to review."
            return
        }
        issueKey = ""
        issueNotice = ""
        editorPage.selectElement(position, 0)
    }
    Shortcut {
        sequence: "F1"
        enabled: !desk.departing && !desk.dialogOpen && !editMenu.visible
                 && !pageBreakMenu.visible && !emphasisMenu.visible
        onActivated: shortcutPanel.open()
    }
    Shortcut { sequence: "Ctrl+E"; enabled: !desk.dialogOpen; onActivated: desk.exportPdf() }
    Shortcut { sequence: "Ctrl+Shift+E"; enabled: desk.fountainJob !== null && !desk.departing && !desk.dialogOpen; onActivated: desk.openExport("fountain") }
    Shortcut { sequence: "Ctrl+\\"; enabled: !desk.dialogOpen; onActivated: desk.toggleOutline() }
    Shortcut { sequence: "Ctrl+0"; enabled: !desk.dialogOpen && editorPage.enabled; onActivated: desk.openElementPicker() }
    Shortcut { sequence: "Ctrl+Shift+B"; enabled: !desk.dialogOpen; onActivated: desk.setView(!desk.sidebarVisible, desk.toolbarVisible) }
    Shortcut { sequence: "Ctrl+Shift+S"; enabled: !desk.departing && !desk.dialogOpen && desk.autosaver !== null && !desk.autosaver.copyBusy && !desk.autosaver.reloadBusy; onActivated: copyPanel.open() }
    Shortcut { sequence: "Ctrl+Shift+H"; enabled: desk.projectFlow !== null && !desk.departing && !desk.dialogOpen; onActivated: desk.projectFlow.home() }
    Shortcut { sequence: "Ctrl+Shift+T"; enabled: titleButton.enabled && !desk.dialogOpen; onActivated: titlePanel.open() }
    Shortcut { sequence: "Ctrl+F"; enabled: editorPage.enabled && !desk.dialogOpen; onActivated: editorPage.openFind() }
    Shortcut { sequence: "Ctrl+H"; enabled: editorPage.enabled && !desk.dialogOpen; onActivated: editorPage.openFind(true) }
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
            fountainDestination.text = desk.autosaver.location + ".fountain"
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
        function onChanged() {
            desk.showReloadStatus = false; desk.lastExport = "pdf"
            if (desk.exportJob.attention) {
                exportPanel.close()
                pdfWaitPanel.open()
            } else {
                pdfWaitPanel.close()
            }
        }
    }
    Connections {
        target: desk.fountainJob
        function onChanged() {
            desk.showReloadStatus = false; desk.lastExport = "fountain"
            if (desk.fountainJob.attention) {
                exportPanel.close()
                fountainWaitPanel.open()
            } else {
                fountainWaitPanel.close()
            }
        }
    }

    component DeskButton: Button {
        id: button
        property color fill: "transparent"
        property color ink: "#303843"
        // Hover/press darken the button's own fill; a transparent button gets
        // a light wash, so dark and light buttons both stay readable.
        readonly property color base: fill.a === 0 ? "#eceef3" : fill
        font.family: "Liberation Sans"
        font.pixelSize: 12
        padding: 10
        background: Rectangle {
            radius: 7
            color: button.down ? Qt.darker(button.base, 1.12) : button.hovered ? Qt.darker(button.base, 1.04) : button.fill
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

    // A labeled icon in the sidebar. Icons are bundled SVGs, so they never
    // depend on which symbol fonts happen to be installed.
    component SideButton: Button {
        id: side
        property string glyph
        property bool active: false
        width: 72; height: 54; padding: 0
        background: Rectangle {
            radius: 8
            color: side.down ? "#dadce8" : side.active ? "#e4e6f3" : side.hovered ? "#eceef3" : "transparent"
            border.width: side.visualFocus ? 2 : 0
            border.color: "#7a83af"
        }
        contentItem: Item {
            Column {
                anchors.centerIn: parent
                spacing: 3
                opacity: side.enabled ? 1 : 0.35
                Image {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 22; height: 22
                    source: "icons/" + side.glyph + ".svg"
                    sourceSize: Qt.size(44, 44)
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: side.text
                    color: side.active ? "#3f4677" : "#525a6b"
                    font { family: "Liberation Sans"; pixelSize: 11; weight: side.active ? Font.DemiBold : Font.Normal }
                }
            }
        }
        ToolTip.visible: hovered
        ToolTip.delay: 600
        ToolTip.text: Accessible.name
    }

    Rectangle {
        id: rail
        objectName: "sidebar"
        enabled: !desk.departing
        width: desk.sidebarVisible ? 84 : 0
        visible: desk.sidebarVisible
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
        color: "#f7f7f9"
        Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: "#dcdfe5" }
        Flickable {
            anchors.fill: parent
            contentHeight: Math.max(height, railTop.height + railBottom.height + 40)
            interactive: contentHeight > height
            clip: true
            Column {
                id: railTop
                anchors { top: parent.top; topMargin: 12; horizontalCenter: parent.horizontalCenter }
                spacing: 2
                SideButton {
                    objectName: "goHome"
                    glyph: "home"; text: "Home"
                    visible: desk.projectFlow !== null
                    enabled: desk.autosaver !== null && !desk.departing && !desk.autosaver.copyBusy && !desk.autosaver.reloadBusy
                    Accessible.name: "Save and return Home (Ctrl+Shift+H)"
                    onClicked: desk.projectFlow.home()
                }
                SideButton {
                    objectName: "outlineToggle"
                    glyph: "scenes"; text: "Scenes"; active: desk.outlineVisible
                    Accessible.name: (desk.outlineVisible ? "Hide scenes" : "Show scenes") + " (Ctrl+\\)"
                    Accessible.checkable: true
                    Accessible.checked: desk.outlineVisible
                    onClicked: desk.toggleOutline()
                }
                SideButton {
                    objectName: "sidebarElements"
                    glyph: "elements"; text: "Elements"; active: elementTypeMenu.visible
                    enabled: editorPage.enabled
                    Accessible.name: "Choose the element type (Ctrl+0)"
                    onClicked: desk.openElementPicker()
                }
                SideButton {
                    id: titleButton
                    objectName: "titlePageButton"
                    glyph: "title"; text: "Title page"
                    visible: desk.autosaver !== null
                    enabled: visible && editorPage.enabled && !desk.autosaver.copyBusy
                    Accessible.name: "Edit title page (Ctrl+Shift+T)"
                    onClicked: titlePanel.open()
                }
                SideButton {
                    objectName: "sidebarFind"
                    glyph: "find"; text: "Find"
                    enabled: editorPage.enabled
                    Accessible.name: "Find (Ctrl+F) / Find and Replace (Ctrl+H)"
                    onClicked: editorPage.openFind()
                }
                SideButton {
                    id: exportButton
                    objectName: "exportButton"
                    glyph: "export"
                    text: (desk.exportJob && desk.exportJob.busy) || (desk.fountainJob && desk.fountainJob.busy) ? "Exporting…" : "Export"
                    enabled: desk.exportJob !== null || desk.fountainJob !== null
                    Accessible.name: desk.fountainJob ? "Export PDF or Fountain (Ctrl+E exports PDF now; Ctrl+Shift+E opens Fountain options)" : "Export PDF (Ctrl+E exports PDF now)"
                    onClicked: desk.openExport(desk.exportFormat)
                }
                SideButton {
                    id: copyButton
                    objectName: "saveCopy"
                    glyph: "copy"
                    visible: desk.autosaver !== null
                    text: desk.autosaver && desk.autosaver.copyBusy ? "Copying…" : "Save copy"
                    enabled: desk.autosaver !== null && !desk.autosaver.copyBusy && !desk.autosaver.reloadBusy
                    Accessible.name: "Save a Copy (Ctrl+Shift+S)"
                    onClicked: copyPanel.open()
                }
                SideButton {
                    objectName: "importedMaterial"
                    glyph: "imported"; text: "Imported"
                    visible: desk.projectFlow !== null && desk.projectFlow.intakeWorking
                    Accessible.name: "Read imported notes, stash and original source"
                    onClicked: importedPanel.open()
                }
            }
            Column {
                id: railBottom
                y: parent.height - height - 12
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 2
                SideButton {
                    objectName: "toolbarToggle"
                    glyph: "toolbar"; text: "Toolbar"; active: desk.toolbarVisible
                    Accessible.name: desk.toolbarVisible ? "Hide the toolbar" : "Show the toolbar"
                    Accessible.checkable: true
                    Accessible.checked: desk.toolbarVisible
                    onClicked: desk.setView(desk.sidebarVisible, !desk.toolbarVisible)
                }
                SideButton {
                    objectName: "shortcutButton"
                    glyph: "shortcuts"; text: "Shortcuts"
                    Accessible.name: "Keyboard shortcuts (F1)"
                    onClicked: shortcutPanel.open()
                }
                SideButton {
                    objectName: "hideSidebar"
                    glyph: "hide"; text: "Hide"
                    Accessible.name: "Hide the sidebar (Ctrl+Shift+B)"
                    onClicked: desk.setView(false, desk.toolbarVisible)
                }
            }
        }
    }
    Button {
        id: showSidebar
        objectName: "showSidebar"
        visible: !desk.sidebarVisible
        z: 5
        x: 10; y: 10; width: 36; height: 36; padding: 0
        Accessible.name: "Show the sidebar (Ctrl+Shift+B)"
        ToolTip.visible: hovered; ToolTip.delay: 600; ToolTip.text: Accessible.name
        background: Rectangle { radius: 8; color: parent.hovered ? "#e4e6ee" : "#f7f7f9"; border.color: "#d3d6de" }
        contentItem: Item {
            Image { anchors.centerIn: parent; width: 20; height: 20; source: "icons/show.svg"; sourceSize: Qt.size(40, 40) }
        }
        onClicked: desk.setView(true, desk.toolbarVisible)
    }

    ShortcutReference {
        id: shortcutPanel
        parent: desk
        elementTypes: editorPage.elementTypes
        onClosed: editorPage.focusEditor()
    }

    TitlePageForm {
        id: titlePanel
        parent: desk
        commands: desk.commands
        onClosed: editorPage.focusEditor()
    }

    // Element picker: a short list beside the caret. Ctrl+0 or Elements opens
    // it; arrows/Enter, a number key or a click chooses; Esc closes.
    Popup {
        id: elementTypeMenu
        objectName: "elementTypeMenu"
        parent: desk
        property int highlighted: 0
        readonly property point anchor: {
            const caret = editorPage.caretRect
            return editorPage.mapToItem(desk, caret.x, caret.y)
        }
        // Beside the page when there is room, so the line being changed stays
        // readable; otherwise just right of the caret.
        readonly property real pageLeft: editorPage.mapToItem(desk, editorPage.pageLeft, 0).x
        width: 260
        x: Math.max(8, Math.min(pageLeft - width - 16 >= rail.width + outline.width + 8 ? pageLeft - width - 16 : anchor.x + 24,
                                desk.width - width - 8))
        y: Math.max(8, Math.min(anchor.y - 40, desk.height - height - 8))
        padding: 6
        modal: false; focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { color: "#23263a"; radius: 10; border.color: "#3a3e58" }
        onAboutToShow: {
            highlighted = 0
            for (let i = 0; i < editorPage.elementTypes.length; ++i)
                if (editorPage.elementTypes[i].type === editorPage.currentType) highlighted = i
        }
        onOpened: pickerList.forceActiveFocus()
        onClosed: editorPage.focusEditor()
        function choose(index) {
            if (index < 0 || index >= editorPage.elementTypes.length) return
            close()
            editorPage.setElementType(editorPage.elementTypes[index].type)
        }
        contentItem: Column {
            id: pickerList
            spacing: 1
            focus: true
            Keys.onPressed: (event) => {
                const count = editorPage.elementTypes.length
                if (event.key === Qt.Key_Down || event.key === Qt.Key_Tab) {
                    elementTypeMenu.highlighted = (elementTypeMenu.highlighted + 1) % count
                } else if (event.key === Qt.Key_Up || event.key === Qt.Key_Backtab) {
                    elementTypeMenu.highlighted = (elementTypeMenu.highlighted + count - 1) % count
                } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
                    elementTypeMenu.choose(elementTypeMenu.highlighted)
                } else if (event.key >= Qt.Key_1 && event.key < Qt.Key_1 + count) {
                    elementTypeMenu.choose(event.key - Qt.Key_1)
                } else {
                    return
                }
                event.accepted = true
            }
            Repeater {
                model: editorPage.elementTypes
                AbstractButton {
                    id: choice
                    required property var modelData
                    required property int index
                    objectName: "elementType" + modelData.name
                    width: 248; height: 34
                    checked: editorPage.currentType === modelData.type
                    focusPolicy: Qt.NoFocus
                    Accessible.name: modelData.label + " (Ctrl+" + (index + 1) + ")"
                    onHoveredChanged: if (hovered) elementTypeMenu.highlighted = index
                    onClicked: elementTypeMenu.choose(index)
                    hoverEnabled: true
                    background: Rectangle {
                        radius: 7
                        color: elementTypeMenu.highlighted === choice.index ? "#7a83af" : "transparent"
                    }
                    contentItem: Item {
                        Text {
                            x: 10; anchors.verticalCenter: parent.verticalCenter
                            text: "Ctrl " + (choice.index + 1)
                            color: elementTypeMenu.highlighted === choice.index ? "#e8eaf6" : "#8d92ad"
                            font { family: "Liberation Sans"; pixelSize: 11 }
                        }
                        Text {
                            x: 62; anchors.verticalCenter: parent.verticalCenter
                            text: choice.modelData.label
                            color: "#f2f3f8"
                            font { family: "Liberation Sans"; pixelSize: 14; weight: choice.checked ? Font.DemiBold : Font.Normal }
                        }
                        Text {
                            anchors { right: parent.right; rightMargin: 10; verticalCenter: parent.verticalCenter }
                            visible: choice.checked
                            text: "✓"; color: "#f2f3f8"
                            font { family: "Liberation Sans"; pixelSize: 13 }
                        }
                    }
                }
            }
            Rectangle { width: 248; height: 1; color: "#3a3e58" }
            Text {
                width: 248; padding: 8; wrapMode: Text.Wrap
                text: "Keeps the words; changes the whole element. Tab cycles the common types while writing."
                color: "#8d92ad"
                font { family: "Liberation Sans"; pixelSize: 11 }
            }
        }
    }

    Popup {
        id: importedPanel; objectName: "importedPanel"
        x: 24; y: 24; width: desk.width - 48; height: desk.height - 48
        modal: true; padding: 0; focus: true
        onClosed: editorPage.focusEditor()
        Loader {
            anchors.fill: parent; active: importedPanel.opened
            sourceComponent: Component {
                ImportedMaterial {
                    access: desk.projectFlow; writerMode: true
                    onBackToWriter: importedPanel.close()
                }
            }
        }
    }

    Rectangle {
        id: outline
        objectName: "sceneOutline"
        anchors { left: rail.right; top: parent.top; bottom: parent.bottom }
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
            text: desk.shownTitle; textFormat: Text.PlainText
            elide: Text.ElideRight; color: "#777f7c"
            font { family: "Liberation Sans"; pixelSize: 11 }
        }
        property string draggedId: ""
        property int dragRevision: -1
        property var dragSelection: null
        property bool dragging: false
        property real pressY: 0
        property real pointerX: 0
        property real pointerY: 0
        property int dropIndex: -1
        function cancelDrag() {
            draggedId = ""
            dragging = false
            dropIndex = -1
        }
        function beginDrag(id, y) {
            cancelDrag()
            if (!editorPage.sceneInteractionAllowed || desk.dialogOpen) return
            draggedId = id
            dragRevision = desk.commands.documentRevision
            dragSelection = editorPage.sceneSelection()
            pressY = y
        }
        function updateDrag(x, y) {
            if (draggedId.length === 0) return
            pointerX = x
            pointerY = y
            if (!dragging && Math.abs(y - pressY) < Qt.styleHints.startDragDistance) return
            dragging = true
            dropIndex = outlineList.count
            const contentY = y - outlineList.y + outlineList.contentY
            for (let i = 0; i < outlineList.count; ++i) {
                const item = outlineList.itemAtIndex(i)
                if (item && contentY < item.y + item.height / 2) {
                    dropIndex = i
                    break
                }
            }
        }
        function finishDrag() {
            const id = draggedId
            const revision = dragRevision
            const selection = dragSelection
            const valid = dragging && dropIndex >= 0 && pointerX >= outlineList.x
                    && pointerX <= outlineList.x + outlineList.width && pointerY >= outlineList.y - 12
                    && pointerY <= outlineList.y + outlineList.height + 12
            const before = dropIndex < outlineList.count && dropIndex >= 0 ? desk.outlineScenes[dropIndex].id : ""
            cancelDrag()
            if (valid)
                editorPage.moveScene(id, before, revision, selection.position, selection.anchor)
        }
        onVisibleChanged: if (!visible) cancelDrag()
        Connections {
            target: desk.commands
            function onDocumentStateChanged() { outline.cancelDrag() }
        }
        Connections {
            target: desk
            function onDialogOpenChanged() { if (desk.dialogOpen) outline.cancelDrag() }
            function onDepartingChanged() { if (desk.departing) outline.cancelDrag() }
        }
        Shortcut {
            sequence: "Esc"; enabled: outline.draggedId.length > 0
            onActivated: outline.cancelDrag()
        }
        Timer {
            interval: 40; repeat: true; running: outline.dragging
            onTriggered: {
                const y = outline.pointerY - outlineList.y
                const step = y < 28 ? -14 : y > outlineList.height - 28 ? 14 : 0
                if (step !== 0) {
                    outlineList.contentY = Math.max(0, Math.min(Math.max(0, outlineList.contentHeight - outlineList.height), outlineList.contentY + step))
                    outline.updateDrag(outline.pointerX, outline.pointerY)
                }
            }
        }
        Text {
            x: 24; y: 98; width: parent.width - 48
            text: desk.outlineScenes.length + " scenes · drag a handle to reorder"
            wrapMode: Text.Wrap
            color: "#777f7c"; font { family: "Liberation Sans"; pixelSize: 11 }
        }
        Text {
            x: 24; y: 150; width: parent.width - 48
            visible: desk.outlineScenes.length === 0
            text: "Scene headings will appear here as you write."
            wrapMode: Text.Wrap; color: "#69706d"
            font { family: "Liberation Sans"; pixelSize: 12 }
        }
        ListView {
            id: outlineList
            objectName: "outlineList"
            anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: 140; bottom: parent.bottom; margins: 18 }
            clip: true; spacing: 5
            model: desk.outlineScenes
            readonly property string currentId: editorPage.currentSceneId
            function followCursor() {
                if (outline.draggedId.length > 0) return
                for (let i = 0; i < count; ++i)
                    if (desk.outlineScenes[i].id === currentId) {
                        positionViewAtIndex(i, ListView.Contain)
                        return
                    }
            }
            onModelChanged: Qt.callLater(followCursor)
            onCurrentIdChanged: Qt.callLater(followCursor)
            onCountChanged: Qt.callLater(followCursor)
            onVisibleChanged: if (visible) Qt.callLater(followCursor)
            delegate: Rectangle {
                id: sceneRow
                required property var modelData
                required property int index
                objectName: "sceneRow" + index
                width: ListView.view.width
                height: sceneLabel.implicitHeight + 42
                radius: 8
                color: modelData.id === editorPage.currentSceneId ? "white" : "transparent"
                opacity: outline.dragging && outline.draggedId === modelData.id ? 0.5 : 1
                Button {
                    objectName: "sceneNavigate" + sceneRow.index
                    anchors { left: parent.left; right: handle.left; top: parent.top; bottom: parent.bottom }
                    enabled: editorPage.sceneInteractionAllowed && !desk.dialogOpen
                    Accessible.name: "Go to scene " + (sceneRow.index + 1) + ": " + sceneRow.modelData.text
                    background: Rectangle { color: "transparent"; radius: 8; border.color: parent.activeFocus ? "#637080" : "transparent" }
                    contentItem: Item {
                        Text {
                            id: sceneLabel
                            x: 12; y: 13; width: parent.width - 18
                            text: (sceneRow.index + 1) + "   " + sceneRow.modelData.text
                            textFormat: Text.PlainText; wrapMode: Text.Wrap
                            color: sceneRow.modelData.id === editorPage.currentSceneId ? "#3a4651" : "#69706d"
                            font { family: "Liberation Sans"; pixelSize: 11; weight: Font.DemiBold }
                        }
                        Text {
                            x: 12; y: sceneLabel.y + sceneLabel.implicitHeight + 6
                            text: "Scene " + (sceneRow.index + 1)
                            color: "#7b827d"; font { family: "Liberation Sans"; pixelSize: 11 }
                        }
                    }
                    padding: 0
                    onClicked: editorPage.navigateScene(sceneRow.modelData.id)
                }
                Item {
                    id: handle
                    objectName: "sceneHandle" + sceneRow.index
                    anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
                    width: 30
                    Accessible.role: Accessible.Button
                    Accessible.name: "Drag scene " + (sceneRow.index + 1) + " to reorder"
                    Text { anchors.centerIn: parent; text: "⋮⋮"; color: "#69706d"; font.pixelSize: 18 }
                    MouseArea {
                        anchors.fill: parent
                        enabled: editorPage.sceneInteractionAllowed && !desk.dialogOpen
                        preventStealing: true
                        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                        onPressed: (mouse) => outline.beginDrag(sceneRow.modelData.id, mapToItem(outline, mouse.x, mouse.y).y)
                        onPositionChanged: (mouse) => {
                            if (pressed) {
                                const point = mapToItem(outline, mouse.x, mouse.y)
                                outline.updateDrag(point.x, point.y)
                            }
                        }
                        onReleased: outline.finishDrag()
                        onCanceled: outline.cancelDrag()
                    }
                }
                Rectangle {
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    height: 3; color: "#637080"
                    visible: outline.dragging && outline.dropIndex === sceneRow.index
                }
            }
            footer: Rectangle {
                width: outlineList.width; height: 12; color: "transparent"
                Rectangle {
                    width: parent.width; height: 3; color: "#637080"
                    visible: outline.dragging && outline.dropIndex === outlineList.count
                }
            }
            ScrollBar.vertical: ScrollBar {}
        }
    }

    Popup {
        id: issuePanel
        objectName: "issuePanel"
        parent: desk
        x: Math.max(8, (desk.width - width) / 2); y: 24
        width: Math.min(600, desk.width - 16)
        height: Math.min(360, desk.height - 48)
        padding: 16; modal: true; focus: true
        background: Rectangle { color: "#f5f4f1"; radius: 8; border.color: "#b9c0c7" }
        onClosed: editorPage.focusEditor()
        Text {
            id: issueTitle
            text: desk.structureIssues.length + (desk.structureIssues.length === 1 ? " speech issue" : " speech issues")
            color: "#303843"; font { family: "Liberation Sans"; pixelSize: 16; weight: Font.DemiBold }
        }
        Text {
            id: issueHelp
            anchors { left: parent.left; right: parent.right; top: issueTitle.bottom; topMargin: 8 }
            text: "Choose an issue to go to it. Nothing changes until you edit the writing or choose a correction; each correction is one Undo step."
            wrapMode: Text.Wrap; color: "#4d5965"
            font { family: "Liberation Sans"; pixelSize: 12 }
        }
        ListView {
            objectName: "issueList"
            anchors { left: parent.left; right: parent.right; top: issueHelp.bottom; topMargin: 12; bottom: parent.bottom }
            clip: true; spacing: 4
            model: desk.structureIssues
            delegate: DeskButton {
                required property var modelData
                required property int index
                objectName: "structureIssue" + index
                width: ListView.view.width
                text: (index + 1) + ". " + desk.issueLabel(modelData.kind) + ": "
                      + (modelData.text.length ? modelData.text : "(empty)")
                ink: "#303843"; fill: modelData.key === desk.issueKey ? "#d4d9df" : "white"
                Accessible.name: "Go to issue " + text
                contentItem: Text {
                    text: parent.text; font: parent.font; color: "#303843"
                    textFormat: Text.PlainText
                    elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    desk.selectIssue(modelData.key)
                    issuePanel.close()
                }
            }
            ScrollBar.vertical: ScrollBar {}
        }
    }

    Item {
        id: workspace
        anchors { left: outline.right; right: parent.right; top: parent.top; bottom: footer.top }
        Item {
            id: workbar
            objectName: "toolbar"
            anchors { left: parent.left; right: parent.right; top: parent.top }
            visible: desk.toolbarVisible
            height: visible ? workbarFlow.height + 20 : 0
            DeskButton {
                objectName: "hideToolbar"
                anchors { right: parent.right; rightMargin: 12; top: parent.top; topMargin: 10 }
                text: "×"; font.pixelSize: 18; padding: 6; ink: "#69706d"
                Accessible.name: "Hide the toolbar"
                onClicked: desk.setView(desk.sidebarVisible, false)
            }
            Flow {
                id: workbarFlow
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 16; topMargin: 10; rightMargin: 52; leftMargin: !desk.sidebarVisible && outline.width === 0 ? 56 : 16 }
                spacing: 8
                DeskButton {
                    id: editButton
                    objectName: "editButton"
                    text: "Edit"; ink: "#303843"; fill: "#ffffff"
                    enabled: editorPage.enabled
                    Accessible.name: "Edit: Undo, Redo, clipboard, spelling, Find and Replace"
                    ToolTip.text: "Undo: Ctrl+Z. Redo: Ctrl+Shift+Z. Find: Ctrl+F. Find and Replace: Ctrl+H. Spelling: F7."
                    onClicked: editMenu.open()
                    Menu {
                        id: editMenu
                        objectName: "editMenu"
                        y: editButton.height
                        onClosed: editorPage.focusEditor()
                        MenuItem {
                            objectName: "editUndo"; text: "Undo\tCtrl+Z"
                            enabled: editorPage.canUndo
                            onTriggered: editorPage.edit("undo")
                        }
                        MenuItem {
                            objectName: "editRedo"; text: "Redo\tCtrl+Shift+Z"
                            enabled: editorPage.canRedo
                            onTriggered: editorPage.edit("redo")
                        }
                        MenuSeparator {}
                        MenuItem {
                            objectName: "editCut"; text: "Cut\tCtrl+X"
                            enabled: editorPage.hasSelection
                            onTriggered: editorPage.edit("cut")
                        }
                        MenuItem {
                            objectName: "editCopy"; text: "Copy\tCtrl+C"
                            enabled: editorPage.hasSelection
                            onTriggered: editorPage.edit("copy")
                        }
                        MenuItem {
                            objectName: "editPaste"; text: "Paste\tCtrl+V"
                            enabled: editorPage.canPaste
                            onTriggered: editorPage.edit("paste")
                        }
                        MenuItem {
                            objectName: "editSpelling"; text: "Spelling at caret…\tF7"
                            onTriggered: editorPage.checkSpelling()
                        }
                        MenuItem {
                            objectName: "editFind"; text: "Find…\tCtrl+F"
                            onTriggered: editorPage.openFind()
                        }
                        MenuItem {
                            objectName: "editReplace"; text: "Find and Replace…\tCtrl+H"
                            onTriggered: editorPage.openFind(true)
                        }
                        MenuItem {
                            objectName: "editSelectAll"; text: "Select all\tCtrl+A"
                            onTriggered: editorPage.edit("selectAll")
                        }
                    }
                }
                DeskButton {
                    id: elementTypeButton
                    objectName: "elementTypeButton"
                    text: editorPage.typeLabel
                    ink: "#303843"; fill: "#ffffff"
                    enabled: editorPage.enabled
                    Accessible.name: "Element type: " + text
                    ToolTip.text: "Change this element's type: Ctrl+0 opens the list; Ctrl+1–8 choose directly; Tab cycles."
                    onClicked: desk.openElementPicker()
                }
                DeskButton {
                    id: pageBreakButton
                    objectName: "pageBreakButton"
                    text: "Page break"
                    ink: "#303843"; fill: "#ffffff"
                    enabled: editorPage.enabled
                    Accessible.name: "Insert or remove a manual page break"
                    ToolTip.text: "Insert: Ctrl+Enter. Remove: Ctrl+Shift+Enter. Page breaks keep whole elements and speeches together."
                    onClicked: pageBreakMenu.open()
                    Menu {
                        id: pageBreakMenu
                        objectName: "pageBreakMenu"
                        y: pageBreakButton.height
                        onClosed: editorPage.focusEditor()
                        MenuItem {
                            objectName: "pageBreakInsert"
                            text: "Insert page break\tCtrl+Enter"
                            onTriggered: editorPage.insertPageBreak()
                        }
                        MenuItem {
                            objectName: "pageBreakRemove"
                            text: "Remove page break\tCtrl+Shift+Enter"
                            onTriggered: editorPage.removePageBreak()
                        }
                        MenuSeparator {}
                        Label {
                            width: 310; padding: 12; wrapMode: Text.Wrap
                            text: "Inserts before the whole element, speech or dual pair; at its final end, inserts after it. Selected writing is never replaced. Backspace or Delete beside a break removes only the marker."
                            font.pixelSize: 12
                        }
                    }
                }
                DeskButton {
                    id: emphasisButton
                    objectName: "emphasisButton"
                    text: "Format"
                    ink: "#303843"; fill: "#ffffff"
                    enabled: editorPage.enabled
                    Accessible.name: "Format screenplay: Dual Dialogue, Bold, Italic, Underline"
                    ToolTip.text: "Dual Dialogue: Ctrl+Shift+D in the second speech. Select text for Bold, Italic or Underline."
                    onClicked: emphasisMenu.open()
                    Menu {
                        id: emphasisMenu
                        objectName: "emphasisMenu"
                        y: emphasisButton.height
                        onClosed: editorPage.focusEditor()
                        MenuItem {
                            objectName: "dualDialogueToggle"
                            text: "Toggle Dual Dialogue\tCtrl+Shift+D"
                            enabled: editorPage.enabled
                            onTriggered: editorPage.toggleDualDialogue()
                        }
                        Label {
                            width: 310; padding: 12; wrapMode: Text.Wrap
                            text: "Dual Dialogue: put the caret in the second complete speech to pair it with the preceding speech, or turn the pair off. Page breaks cannot separate the speeches."
                            font.pixelSize: 12
                        }
                        MenuSeparator {}
                        MenuItem {
                            objectName: "emphasisBold"
                            text: "Bold\tCtrl+B"
                            font.bold: true
                            enabled: editorPage.enabled && editorPage.hasSelection
                            onTriggered: editorPage.toggleEmphasis("bold")
                        }
                        MenuItem {
                            objectName: "emphasisItalic"
                            text: "Italic\tCtrl+I"
                            font.italic: true
                            enabled: editorPage.enabled && editorPage.hasSelection
                            onTriggered: editorPage.toggleEmphasis("italic")
                        }
                        MenuItem {
                            objectName: "emphasisUnderline"
                            text: "Underline\tCtrl+U"
                            font.underline: true
                            enabled: editorPage.enabled && editorPage.hasSelection
                            onTriggered: editorPage.toggleEmphasis("underline")
                        }
                    }
                }
            }
        }
        Rectangle {
            id: issueBar
            objectName: "issueBar"
            anchors { left: parent.left; right: parent.right; top: workbar.bottom }
            readonly property bool explaining: desk.issueNotice.length > 0 || desk.selectedIssue !== null
            height: visible ? (explaining ? 70 : 44) : 0
            visible: desk.structureIssues.length > 0
            color: "#f3eee4"
            DeskButton {
                id: reviewIssues
                objectName: "reviewIssues"
                x: 12; y: 4
                text: desk.structureIssues.length + (desk.structureIssues.length === 1 ? " speech issue…" : " speech issues…")
                ink: "#303843"; fill: "#d4d9df"
                Accessible.name: "Review speech structure issues"
                onClicked: issuePanel.open()
            }
            Text {
                anchors { left: reviewIssues.right; leftMargin: 12; right: corrections.left; rightMargin: 12; verticalCenter: reviewIssues.verticalCenter }
                text: desk.selectedIssue ? desk.issueLabel(desk.selectedIssue.kind) + ": " + desk.selectedIssue.text
                                         : "Fountain export waits until these are fixed; PDF export is not affected."
                textFormat: Text.PlainText; elide: Text.ElideRight
                color: "#303843"; font { family: "Liberation Sans"; pixelSize: 12 }
            }
            Row {
                id: corrections
                anchors { right: parent.right; rightMargin: 12; top: parent.top; topMargin: 4 }
                spacing: 8
                Repeater {
                    model: desk.selectedIssue ? desk.selectedIssue.corrections : []
                    DeskButton {
                        required property var modelData
                        objectName: modelData.action
                        text: modelData.label
                        ink: "#303843"; fill: "#d4d9df"
                        enabled: modelData.enabled && (!desk.autosaver || !desk.autosaver.reloadBusy) && !desk.departing
                        Accessible.name: modelData.label + "; Undo restores the writing"
                        onClicked: desk.correctSelectedIssue(modelData.action)
                    }
                }
            }
            Text {
                visible: issueBar.explaining
                anchors { left: parent.left; right: parent.right; top: reviewIssues.bottom; margins: 12; topMargin: 3 }
                text: desk.issueNotice || (desk.selectedIssue ? desk.selectedIssue.explanation : "")
                textFormat: Text.PlainText; wrapMode: Text.Wrap
                color: "#4d5965"; font { family: "Liberation Sans"; pixelSize: 11 }
            }
        }
        ScreenplayEditor {
            id: editorPage
            objectName: "editorPage"
            enabled: (!desk.autosaver || !desk.autosaver.reloadBusy) && !desk.departing
            anchors { left: parent.left; right: parent.right; top: dualNotice.bottom; bottom: parent.bottom }
            screenplayLayout: desk.screenplayLayout
            commands: desk.commands
        }
        Rectangle {
            id: dualNotice
            anchors { left: parent.left; right: parent.right; top: issueBar.bottom }
            visible: notice.text.length > 0
            height: visible ? notice.implicitHeight + 20 : 0
            color: "#f4e4cf"
            Text {
                id: notice
                objectName: "dualDialogueNotice"
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }
                text: desk.screenplayLayout.error.indexOf("dual dialogue exceeds one page") >= 0
                      ? "Dual Dialogue exceeds this profile's one-page limit. All writing and pairing are kept in a reading preview; PDF export is refused. Shorten the speeches or explicitly turn Dual Dialogue off."
                      : desk.commands.dualMessage
                textFormat: Text.PlainText; wrapMode: Text.Wrap
                color: "#6b3d13"; font { family: "Liberation Sans"; pixelSize: 12 }
            }
        }
    }
    // One quiet line: what you are writing, whether it is saved, and the
    // latest operation. Save problems add their buttons here.
    Rectangle {
        id: footer
        objectName: "statusBar"
        anchors { left: outline.right; right: parent.right; bottom: parent.bottom }
        height: 30
        color: "#eceef1"
        Rectangle { width: parent.width; height: 1; color: "#dcdfe5" }
        Text {
            id: titleText
            objectName: "documentTitle"
            x: 20
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, parent.width * 0.3)
            text: desk.shownTitle; textFormat: Text.PlainText
            elide: Text.ElideRight
            color: "#303843"
            font { family: "Liberation Sans"; pixelSize: 12; weight: Font.DemiBold }
            ToolTip.visible: titleArea.containsMouse
            ToolTip.text: projectLocation.text.length > 0 ? projectLocation.text : text
            MouseArea { id: titleArea; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
        }
        // The folder is shown on hover over the title rather than as another line.
        Text {
            id: projectLocation
            objectName: "projectLocation"
            visible: false
            text: desk.autosaver ? desk.autosaver.location : ""
        }
        Text {
            id: saveStatus
            objectName: "saveStatus"
            anchors { left: titleText.right; leftMargin: 10; verticalCenter: parent.verticalCenter }
            width: Math.min(implicitWidth, parent.width * 0.25)
            elide: Text.ElideRight
            text: (desk.autosaver ? desk.autosaver.status : "Fountain file · edits are not saved")
                  + " · " + desk.screenplayLayout.pageCount + (desk.screenplayLayout.pageCount === 1 ? " page" : " pages")
            color: desk.autosaver && desk.autosaver.state === "failed" ? "#9d3030" : "#6b7480"
            font { family: "Liberation Sans"; pixelSize: 11 }
            ToolTip.visible: statusArea.containsMouse && saveStatus.truncated
            ToolTip.text: saveStatus.text
            MouseArea { id: statusArea; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
        }
        Text {
            objectName: "sessionNotice"
            anchors { left: saveStatus.right; leftMargin: 14; right: statusRight.left; rightMargin: 14; verticalCenter: parent.verticalCenter }
            visible: desk.sessionNotice.length > 0
            text: desk.sessionNotice; textFormat: Text.PlainText
            elide: Text.ElideRight; color: "#9d3030"
            font { family: "Liberation Sans"; pixelSize: 11 }
            ToolTip.visible: sessionArea.containsMouse
            ToolTip.text: text
            MouseArea { id: sessionArea; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
        }
        Row {
            id: statusRight
            anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
            spacing: 8
            Text {
                objectName: "operationStatus"
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(implicitWidth, footer.width * 0.35)
                text: desk.commands.typeMessage || desk.commands.pageBreakMessage || desk.screenplayLayout.error || desk.commands.spelling.error || (desk.autosaver && (desk.autosaver.copyBusy || desk.autosaver.copyError) ? desk.autosaver.copyStatus : "") || (desk.autosaver && desk.showReloadStatus ? desk.autosaver.reloadStatus : "") || (desk.lastExport === "fountain" && desk.fountainJob ? desk.fountainJob.status : desk.exportJob ? desk.exportJob.status : "")
                elide: Text.ElideMiddle
                ToolTip.visible: footerArea.containsMouse
                ToolTip.text: text
                MouseArea { id: footerArea; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
                color: desk.screenplayLayout.error ? "#9d3030" : "#6b7480"
                font { family: "Liberation Sans"; pixelSize: 11 }
            }
            DeskButton {
                id: retryButton
                objectName: "retrySave"
                visible: desk.autosaver !== null && desk.autosaver.state === "failed"
                text: "Retry save"; padding: 5
                fill: "#8f3a32"; ink: "#fbecea"
                Accessible.name: "Retry saving to " + (desk.autosaver ? desk.autosaver.location : "")
                onClicked: { desk.autosaver.retry(); editorPage.focusEditor() }
            }
            DeskButton {
                id: reloadButton
                objectName: "reloadProject"
                visible: retryButton.visible
                enabled: desk.autosaver !== null && !desk.autosaver.copyBusy && !desk.autosaver.reloadBusy
                text: "Reload…"; padding: 5; ink: "#303843"; fill: "#dcdfe5"
                onClicked: reloadPanel.open()
            }
        }
    }
    Popup {
        id: leavePanel
        objectName: "leavePanel"
        parent: desk
        x: Math.max(8, (desk.width - width) / 2); y: 24
        width: Math.min(650, desk.width - 16)
        height: Math.min(desk.height - 48, leaveContents.implicitHeight + 40)
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
        x: Math.max(8, (desk.width - width) / 2); y: 24
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
        x: Math.max(8, (desk.width - width) / 2); y: 24
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
        x: Math.max(8, (desk.width - width) / 2); y: 24
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
        id: pdfWaitPanel
        objectName: "pdfWaitPanel"
        parent: desk
        x: Math.max(8, (desk.width - width) / 2); y: 24
        width: Math.min(520, desk.width - 16)
        height: pdfWaitContents.implicitHeight + 40
        padding: 20; modal: true; focus: true
        closePolicy: Popup.NoAutoClose
        onClosed: editorPage.focusEditor()
        background: Rectangle { color: "#f5f4f1"; radius: 8; border.color: "#b9c0c7" }
        Column {
            id: pdfWaitContents
            width: parent.width; spacing: 14
            Label { text: "PDF export in progress"; font.pixelSize: 18 }
            Label {
                width: parent.width; wrapMode: Text.Wrap; textFormat: Text.PlainText
                text: desk.exportJob ? desk.exportJob.status : ""
            }
            Row {
                spacing: 10
                DeskButton {
                    objectName: "pdfWaitCancel"; text: "Cancel Export"; ink: "#303843"
                    enabled: desk.exportJob !== null && !desk.exportJob.cancelling
                    onClicked: desk.exportJob.cancel()
                }
                DeskButton {
                    objectName: "pdfKeepWaiting"; text: "Keep Waiting"; ink: "#303843"
                    onClicked: desk.exportJob.keepWaiting()
                }
            }
        }
    }
    Popup {
        id: fountainWaitPanel
        objectName: "fountainWaitPanel"
        parent: desk
        x: Math.max(8, (desk.width - width) / 2); y: 24
        width: Math.min(520, desk.width - 16)
        height: fountainWaitContents.implicitHeight + 40
        padding: 20; modal: true; focus: true
        closePolicy: Popup.NoAutoClose
        onClosed: editorPage.focusEditor()
        background: Rectangle { color: "#f5f4f1"; radius: 8; border.color: "#b9c0c7" }
        Column {
            id: fountainWaitContents
            width: parent.width; spacing: 14
            Label { text: "Fountain export in progress"; font.pixelSize: 18 }
            Label {
                width: parent.width; wrapMode: Text.Wrap; textFormat: Text.PlainText
                text: desk.fountainJob ? desk.fountainJob.status : ""
            }
            Row {
                spacing: 10
                DeskButton {
                    objectName: "fountainWaitCancel"; text: "Cancel Export"; ink: "#303843"
                    enabled: desk.fountainJob !== null && !desk.fountainJob.cancelling
                    onClicked: desk.fountainJob.cancel()
                }
                DeskButton {
                    objectName: "fountainKeepWaiting"; text: "Keep Waiting"; ink: "#303843"
                    onClicked: desk.fountainJob.keepWaiting()
                }
            }
        }
    }
    Popup {
        id: exportPanel
        objectName: "exportPanel"
        parent: desk
        x: Math.max(8, (desk.width - width) / 2); y: 24
        width: Math.min(520, desk.width - 16)
        height: Math.min(desk.height - y - 8, exportColumn.implicitHeight + 40)
        padding: 20
        modal: true; focus: true
        onClosed: editorPage.focusEditor()
        background: Rectangle { color: "#f5f4f1"; radius: 8; border.color: "#b9c0c7" }
        Column {
            id: exportColumn
            width: parent.width
            spacing: 10
            Text {
                text: desk.fountainJob ? "Export snapshot" : "Export PDF snapshot"
                color: "#303843"; font { family: "Liberation Sans"; pixelSize: 16; weight: Font.DemiBold }
            }
            Row {
                spacing: 8
                visible: desk.fountainJob !== null
                DeskButton {
                    objectName: "exportFormatPdf"
                    text: "PDF"
                    fill: desk.exportFormat === "pdf" ? "#3c434d" : "#dfe3e7"; ink: desk.exportFormat === "pdf" ? "#eff1f3" : "#303843"
                    Accessible.name: "PDF format"
                    Accessible.checkable: true; Accessible.checked: desk.exportFormat === "pdf"
                    onClicked: { desk.exportFormat = "pdf"; desk.refreshExportCheck() }
                }
                DeskButton {
                    objectName: "exportFormatFountain"
                    text: "Fountain"
                    fill: desk.exportFormat === "fountain" ? "#3c434d" : "#dfe3e7"; ink: desk.exportFormat === "fountain" ? "#eff1f3" : "#303843"
                    Accessible.name: "Clean Fountain format (Ctrl+Shift+E)"
                    Accessible.checkable: true; Accessible.checked: desk.exportFormat === "fountain"
                    onClicked: { desk.exportFormat = "fountain"; desk.refreshExportCheck() }
                }
            }
            TextField {
                id: destination
                objectName: "exportDestination"
                visible: desk.exportFormat === "pdf"
                width: parent.width
                text: desk.exportDestination
                selectByMouse: true; font.pixelSize: 12
                Accessible.name: "PDF destination"
            }
            TextField {
                id: fountainDestination
                objectName: "fountainDestination"
                visible: desk.exportFormat === "fountain"
                width: parent.width
                text: desk.exportDestination.replace(/\.pdf$/, "") + ".fountain"
                selectByMouse: true; font.pixelSize: 12
                Accessible.name: "Fountain destination"
            }
            CheckBox {
                objectName: "includeTitle"
                text: "Include title page"; checked: desk.includeTitle
                onToggled: { desk.includeTitle = checked; desk.refreshExportCheck() }
            }
            Text {
                objectName: "exportEligibility"
                width: parent.width
                visible: text.length > 0
                text: desk.exportEligibility()
                wrapMode: Text.Wrap
                color: desk.exportFormat === "fountain" && desk.fountainCheck.length === 0 ? "#3f5a45" : "#8a3b2c"
                font { family: "Liberation Sans"; pixelSize: 12 }
            }
            Label {
                visible: desk.exportFormat === "pdf" && desk.exportJob !== null && desk.exportJob.busy
                width: parent.width; wrapMode: Text.Wrap; textFormat: Text.PlainText
                text: desk.exportJob ? desk.exportJob.status : ""
            }
            Label {
                visible: desk.exportFormat === "fountain" && desk.fountainJob !== null && desk.fountainJob.busy
                width: parent.width; wrapMode: Text.Wrap; textFormat: Text.PlainText
                text: desk.fountainJob ? desk.fountainJob.status : ""
            }
            Item {
                width: parent.width; height: exportConfirm.height
                DeskButton {
                    objectName: "pdfCancel"; text: "Cancel Export"; ink: "#303843"
                    visible: desk.exportFormat === "pdf" && desk.exportJob !== null && desk.exportJob.busy
                    enabled: desk.exportJob !== null && !desk.exportJob.cancelling
                    onClicked: { desk.exportJob.cancel(); exportPanel.close() }
                }
                DeskButton {
                    objectName: "fountainCancel"; text: "Cancel Export"; ink: "#303843"
                    visible: desk.exportFormat === "fountain" && desk.fountainJob !== null && desk.fountainJob.busy
                    enabled: desk.fountainJob !== null && !desk.fountainJob.cancelling
                    onClicked: { desk.fountainJob.cancel(); exportPanel.close() }
                }
                DeskButton {
                    id: exportConfirm
                    objectName: "exportConfirm"
                    anchors.right: parent.right
                    text: desk.exportFormat === "fountain" ? "Export Fountain" : "Export PDF"
                    fill: "#3c434d"; ink: "#eff1f3"
                    enabled: desk.exportFormat === "fountain"
                             ? desk.fountainJob !== null && !desk.fountainJob.busy && desk.fountainCheck.length === 0
                             : desk.exportJob !== null && !desk.exportJob.busy
                    onClicked: desk.exportFormat === "fountain" ? desk.exportFountain() : desk.exportPdf()
                }
            }
        }
    }
}
