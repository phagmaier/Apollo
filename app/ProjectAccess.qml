import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs

// Bounded project home and safe-access chooser, using the Writing desk palette.
Rectangle {
    id: page
    required property var access
    property int selectedIndex: -1
    property bool creating: false
    property bool importing: false
    color: "#d4d9df"
    Connections {
        target: page.access
        function onChanged() {
            if (page.access.busy) page.selectedIndex = -1
        }
    }
    Rectangle {
        id: header
        width: parent.width; height: 76; color: "#323842"
        Text {
            x: 28; anchors.verticalCenter: parent.verticalCenter
            text: "apollo"; color: "#eff1f3"
            font { family: "Liberation Sans"; pixelSize: 23; weight: Font.Bold; letterSpacing: -1.2 }
        }
        Text {
            x: 130; anchors.verticalCenter: parent.verticalCenter
            text: page.access.location.length > 0 ? "Open a screenplay" : "Your screenplays"
            color: "#eff1f3"
            font { family: "Liberation Sans"; pixelSize: 16 }
        }
        Button {
            objectName: "accessHome"
            anchors { right: parent.right; rightMargin: 28; verticalCenter: parent.verticalCenter }
            visible: page.access.location.length > 0
            enabled: !page.access.busy
            text: "Home"
            onClicked: page.access.home()
        }
    }
    ScrollView {
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom; margins: 28 }
        contentWidth: availableWidth
        Column {
            width: Math.min(820, parent.width)
            spacing: 16
            Label {
                visible: page.creating
                text: "New screenplay"
                font { family: "Liberation Sans"; pixelSize: 26; weight: Font.DemiBold }
                color: "#303843"
            }
            Flow {
                width: parent.width; spacing: 12
                visible: page.access.location.length === 0
                Button {
                    objectName: "showNewProject"
                    text: page.creating ? "Cancel New Project" : "New Project"
                    enabled: !page.access.busy
                    onClicked: { page.creating = !page.creating; page.importing = false }
                }
                Button {
                    objectName: "showImport"
                    text: page.importing ? "Cancel Import" : "Import Fountain for Review…"
                    enabled: !page.access.busy
                    onClicked: { page.importing = !page.importing; page.creating = false }
                }
            }
            Column {
                width: parent.width; spacing: 12; visible: page.importing
                Label {
                    width: parent.width; wrapMode: Text.Wrap; color: "#303843"
                    text: "Retain and read an exact copy of your Fountain file in a new project. Review the source before retaining it. Writing in the verified subset can then be edited in a new copy and exported as clean Fountain; PDF export of imported writing is still pending."
                }
                TextField {
                    id: importSource; objectName: "importSourcePath"
                    width: parent.width; selectByMouse: true; enabled: !page.access.busy
                    placeholderText: "/path/to/script.fountain"; Accessible.name: "Fountain source file"
                }
                Flow {
                    width: parent.width; spacing: 12
                    Button {
                        objectName: "chooseImportSource"; text: "Choose Fountain File…"; enabled: !page.access.busy
                        onClicked: importFiles.open()
                    }
                    Button {
                        objectName: "previewImport"; text: "Review Import"
                        enabled: !page.access.busy && importSource.text.length > 0
                        onClicked: page.access.preview_import(importSource.text)
                    }
                }
            }
            Column {
                width: parent.width; spacing: 12
                visible: page.creating
                Label { text: "Project name"; color: "#303843" }
                TextField {
                    id: newName; objectName: "newProjectName"
                    width: parent.width; enabled: !page.access.busy; selectByMouse: true
                    placeholderText: "Untitled screenplay"
                    Accessible.name: "New project folder name"
                }
                Label { text: "Create inside"; color: "#303843" }
                TextField {
                    id: newParent; objectName: "newProjectParent"
                    width: parent.width; text: page.access.defaultLocation
                    enabled: !page.access.busy; selectByMouse: true
                    Accessible.name: "Parent folder for the new project"
                }
                Label {
                    width: parent.width; wrapMode: Text.Wrap; color: "#4d5965"
                    text: "Apollo creates a complete project in a new folder. Existing folders and files stay untouched; a number is added to the name when needed."
                }
                Flow {
                    width: parent.width; spacing: 12
                    Button {
                        objectName: "chooseNewParent"; text: "Choose Location…"
                        enabled: !page.access.busy
                        onClicked: { folders.forCreation = true; folders.currentFolder = page.access.folder_url(newParent.text); folders.open() }
                    }
                    Button {
                        objectName: "createProject"
                        text: page.access.busy ? "Creating…" : "Create and Write"
                        enabled: !page.access.busy && newName.text.trim().length > 0 && newParent.text.trim().length > 0
                        onClicked: page.access.create_project(newParent.text, newName.text)
                    }
                }
            }
            Label {
                width: parent.width; wrapMode: Text.Wrap
                visible: !page.creating && !page.importing
                text: "Choose an Apollo project folder. Opening never replaces its writing with a recovery copy."
                font { family: "Liberation Sans"; pixelSize: 14 }
                color: "#303843"
            }
            TextField {
                id: projectFolder
                objectName: "openLocation"
                width: parent.width
                visible: !page.creating && !page.importing
                text: page.access.location
                enabled: !page.access.busy
                selectByMouse: true
                placeholderText: "/path/to/project"
                Accessible.name: "Existing Apollo project folder"
                onAccepted: if (!page.access.busy) page.access.open_project(text)
            }
            Flow {
                width: parent.width; spacing: 12
                visible: !page.creating && !page.importing
                Button {
                    objectName: "chooseOpenFolder"; text: "Choose Project Folder…"
                    enabled: !page.access.busy
                    onClicked: { folders.forCreation = false; folders.currentFolder = page.access.folder_url(projectFolder.text || page.access.defaultLocation); folders.open() }
                }
                Button {
                    objectName: "openProject"
                    text: page.access.busy ? "Checking project…" : "Open Project"
                    enabled: !page.access.busy && projectFolder.text.trim().length > 0
                    onClicked: page.access.open_project(projectFolder.text)
                }
            }
            Label {
                visible: !page.creating && page.access.location.length === 0
                text: "Recent projects"; color: "#303843"
                font { family: "Liberation Sans"; pixelSize: 18; weight: Font.DemiBold }
            }
            Label {
                width: parent.width; wrapMode: Text.Wrap; color: "#4d5965"
                visible: !page.creating && page.access.location.length === 0 && page.access.recentProjects.length === 0
                text: "No recent projects yet. Create a screenplay or open an Apollo project folder."
            }
            Repeater {
                model: !page.creating && page.access.location.length === 0 ? page.access.recentProjects : []
                ItemDelegate {
                    required property var modelData
                    required property int index
                    objectName: "recentProject" + index
                    width: parent.width; enabled: !page.access.busy
                    focusPolicy: Qt.StrongFocus
                    onClicked: page.access.open_recent(index)
                    contentItem: Label {
                        text: modelData.name + "\n" + modelData.location
                        textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere
                        font { family: "Liberation Sans"; pixelSize: 14 }
                        color: "#303843"
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#eff1f3" : "#e3e6e9"; radius: 7
                        border.width: parent.visualFocus ? 2 : 0; border.color: "#7a83af"
                    }
                }
            }
            Label {
                objectName: "accessNotice"
                width: parent.width; wrapMode: Text.WrapAnywhere; textFormat: Text.PlainText
                visible: text.length > 0; text: page.access.notice; color: "#9d3030"
            }
            Label {
                objectName: "accessError"
                width: parent.width; wrapMode: Text.WrapAnywhere; textFormat: Text.PlainText
                visible: text.length > 0; text: page.access.error; color: "#9d3030"
            }
            Label {
                objectName: "currentSummary"
                width: parent.width; wrapMode: Text.WrapAnywhere; textFormat: Text.PlainText
                visible: page.access.currentAvailable
                text: page.access.currentSummary; color: "#303843"
            }
            Button {
                objectName: "openCurrent"
                visible: page.access.currentAvailable
                enabled: !page.access.busy
                text: "Open Current Project Without Recovery"
                onClicked: page.access.open_current()
            }
            Label {
                width: parent.width; wrapMode: Text.Wrap
                visible: page.access.candidates.length > 0
                text: "Verified recovery copies"
                font { family: "Liberation Sans"; pixelSize: 18; weight: Font.DemiBold }
                color: "#303843"
            }
            Label {
                width: parent.width; wrapMode: Text.Wrap
                visible: page.access.candidates.length > 0
                text: "Compare revisions and capture times before choosing. Recovery creates a new project at the location below; the original and every recovery copy remain untouched. Capture time alone does not prove which writing is newer."
                color: "#303843"
            }
            Repeater {
                model: page.access.candidates
                RadioDelegate {
                    required property var modelData
                    required property int index
                    objectName: "recoveryChoice" + index
                    width: parent.width
                    checked: page.selectedIndex === index
                    enabled: !page.access.busy
                    onClicked: page.selectedIndex = index
                    contentItem: Label {
                        leftPadding: 38
                        text: "Revision " + modelData.revision + " — " + modelData.relation + "\n"
                            + modelData.timestamp + " · " + modelData.reason + "\n"
                            + modelData.location + "\nOriginal location: " + modelData.origin
                        textFormat: Text.PlainText
                        wrapMode: Text.WrapAnywhere
                        color: "#303843"
                        font { family: "Liberation Sans"; pixelSize: 13 }
                    }
                    background: Rectangle {
                        color: parent.checked ? "#f5f4f1" : "transparent"
                        radius: 7
                        border.width: parent.visualFocus ? 2 : 1
                        border.color: parent.visualFocus ? "#7a83af" : "#b9c0c7"
                    }
                }
            }
            Label {
                width: parent.width; wrapMode: Text.Wrap
                visible: !page.creating && page.access.location.length > 0 && !page.access.busy && !page.access.currentAvailable && page.access.error.length > 0 && page.access.candidates.length === 0
                text: "No verified recovery copy is available. Nothing has been overwritten or replaced with an empty screenplay. Keep this folder and its recovery material for investigation, or open another project."
                color: "#303843"
            }
            Label {
                visible: page.access.candidates.length > 0
                text: "New recovered-project folder"; color: "#303843"
            }
            TextField {
                id: recoveryFolder
                objectName: "recoveryDestination"
                width: parent.width
                visible: page.access.candidates.length > 0
                enabled: !page.access.busy
                selectByMouse: true
                placeholderText: "Absolute path to a new folder inside an existing folder"
                Accessible.name: "New recovered project folder (absolute path)"
            }
            Button {
                objectName: "recoverProject"
                visible: page.access.candidates.length > 0
                text: "Recover Selected Copy as New Project"
                enabled: !page.access.busy && page.selectedIndex >= 0 && recoveryFolder.text.trim().length > 0
                onClicked: page.access.recover(page.selectedIndex, recoveryFolder.text)
            }
        }
    }
    FolderDialog {
        id: folders
        objectName: "projectFolderDialog"
        property bool forCreation: false
        title: forCreation ? "Create a project inside this folder" : "Choose an Apollo project folder"
        onAccepted: {
            const path = page.access.local_folder(selectedFolder)
            if (path.length > 0) {
                if (forCreation) newParent.text = path
                else projectFolder.text = path
            }
        }
    }
    FileDialog {
        id: importFiles; objectName: "importFileDialog"
        title: "Choose a Fountain source to retain"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Fountain scripts (*.fountain)", "All files (*)"]
        onAccepted: importSource.text = page.access.local_folder(selectedFile)
    }
}
