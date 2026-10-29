import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs

Rectangle {
    id: page
    required property var access
    property bool writerMode: false
    signal backToWriter()
    property var selectedFinding: findings.currentIndex >= 0 ? access.intakeFindings[findings.currentIndex] : null
    property string selectedText: selectedFinding ? "Lines " + selectedFinding.firstLine + "–" + selectedFinding.lastLine
        + "; bytes [" + selectedFinding.startByte + ", " + selectedFinding.endByte + ")\n"
        + selectedFinding.message + "\n" + selectedFinding.text
        + (selectedFinding.truncated ? "\nExcerpt truncated; read the complete source below." : "") : ""
    color: "#d4d9df"
    Rectangle {
        id: header
        width: parent.width; height: 76; color: "#323842"
        Label {
            x: 28; anchors.verticalCenter: parent.verticalCenter
            text: page.access.intakePreview ? "Review Fountain import" : "Retained imported material"
            color: "#eff1f3"; font { family: "Liberation Sans"; pixelSize: 22; weight: Font.DemiBold }
        }
        Button {
            objectName: "intakeHome"
            anchors { right: parent.right; rightMargin: 28; verticalCenter: parent.verticalCenter }
            text: page.writerMode ? "Back to Writing" : page.access.intakePreview ? "Cancel Import" : "Home"
            enabled: !page.access.busy
            onClicked: page.writerMode ? page.backToWriter() : page.access.home()
        }
    }
    ScrollView {
        id: scroll
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom; margins: 28 }
        contentWidth: availableWidth
        Column {
            width: scroll.availableWidth; spacing: 16
            Label {
                width: parent.width; text: page.writerMode || page.access.intakeActive ? page.access.location : "Preview of the bytes that will be retained"
                textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: "#303843"
                font { family: "Liberation Sans"; pixelSize: 16; weight: Font.DemiBold }
            }
            Label {
                objectName: "intakeSummary"
                width: parent.width; text: page.access.intakeSummary; textFormat: Text.PlainText
                wrapMode: Text.Wrap; color: "#303843"; font { family: "Liberation Sans"; pixelSize: 14 }
            }
            Label {
                width: parent.width; visible: text.length > 0
                text: page.access.notice; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: "#9d3030"
            }
            Label {
                objectName: "intakeError"
                width: parent.width; visible: text.length > 0
                text: page.access.error; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: "#9d3030"
            }
            Column {
                visible: !page.writerMode
                width: parent.width; spacing: 10
                Label { text: page.access.intakeActive ? "Create a new project copy from this source" : "Retain or convert this source in a new project"; color: "#303843"; font.pixelSize: 17 }
                TextField {
                    id: name; objectName: "intakeProjectName"; width: parent.width
                    enabled: !page.access.busy; placeholderText: "Project folder name"; selectByMouse: true
                    Accessible.name: "Import project folder name"
                }
                TextField {
                    id: parentFolder; objectName: "intakeProjectParent"; width: parent.width
                    enabled: !page.access.busy; text: page.access.defaultLocation; selectByMouse: true
                    Accessible.name: "Parent folder for retained import"
                }
                Flow {
                    width: parent.width; spacing: 12
                    Button {
                        objectName: "intakeChooseParent"; text: "Choose Location…"; enabled: !page.access.busy
                        onClicked: { folders.currentFolder = page.access.folder_url(parentFolder.text); folders.open() }
                    }
                    Button {
                        objectName: "publishIntake"; text: page.access.busy ? "Retaining source…" : "Retain for Review"
                        visible: page.access.intakePreview
                        enabled: !page.access.busy && name.text.trim().length > 0 && parentFolder.text.length > 0
                        onClicked: page.access.publish_import(parentFolder.text, name.text)
                    }
                    Button {
                        objectName: "interpretIntake"; text: page.access.busy ? "Interpreting source…" : "Interpret in a New Copy"
                        visible: !page.access.intakeInterpreted
                        enabled: !page.access.busy && name.text.trim().length > 0 && parentFolder.text.length > 0
                        onClicked: page.access.interpret_import(parentFolder.text, name.text)
                    }
                    Button {
                        objectName: "editIntake"; text: page.access.busy ? "Creating editable copy…" : "Edit in a New Copy"
                        enabled: !page.access.busy && page.access.intakeEditSupported && name.text.trim().length > 0 && parentFolder.text.length > 0
                        onClicked: page.access.edit_import(parentFolder.text, name.text)
                    }
                }
                Label {
                    width: parent.width; textFormat: Text.PlainText; wrapMode: Text.Wrap
                    text: page.access.intakeEditSupported ? "This source meets the bounded writer subset. Editing creates a new copy; clean exports remain blocked."
                        : "Editable conversion is gated:\n" + page.access.intakeEditBlockers
                    color: "#303843"
                }
            }
            Label { text: "Review findings"; color: "#303843"; font { family: "Liberation Sans"; pixelSize: 18; weight: Font.DemiBold } }
            ComboBox {
                id: findings; objectName: "intakeFindings"
                width: parent.width; model: page.access.intakeFindings; textRole: "kind"
                displayText: currentIndex >= 0 ? model[currentIndex].kind + " · line " + model[currentIndex].firstLine : "No candidate findings; all source still needs conversion"
            }
            TextArea {
                objectName: "intakeFindingText"; width: parent.width; visible: page.selectedText.length > 0
                readOnly: true; selectByMouse: true; textFormat: TextEdit.PlainText
                text: page.selectedText; wrapMode: TextEdit.Wrap; color: "#303843"
                background: Rectangle { color: "#e3e6e9"; radius: 6 }
            }
            Label { text: "Complete retained source (read-only)"; color: "#303843"; font.pixelSize: 18 }
            ScrollView {
                width: parent.width; height: Math.max(260, Math.min(650, page.height - 300))
                TextArea {
                    objectName: "intakeSource"
                    readOnly: true; selectByMouse: true; textFormat: TextEdit.PlainText
                    text: page.access.intakeSource; wrapMode: TextEdit.NoWrap
                    font { family: "Courier Prime"; pixelSize: 15 }
                    color: "#303843"; padding: 18
                    background: Rectangle { color: "#f5f4f1"; radius: 6 }
                    Accessible.name: "Complete retained Fountain source, read-only"
                }
            }
        }
    }
    FolderDialog {
        id: folders; objectName: "intakeFolderDialog"
        title: "Retain an import inside this folder"
        onAccepted: parentFolder.text = page.access.local_folder(selectedFolder)
    }
}
