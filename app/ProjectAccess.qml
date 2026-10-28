import QtQuick
import QtQuick.Controls.Basic

// Uses the accepted Writing desk palette/type. This is a bounded open/recovery
// surface, not the milestone's future project library or import workflow.
Rectangle {
    id: page
    required property var access
    property int selectedIndex: -1
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
            text: "Open a screenplay"; color: "#eff1f3"
            font { family: "Liberation Sans"; pixelSize: 16 }
        }
    }
    ScrollView {
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom; margins: 28 }
        contentWidth: availableWidth
        Column {
            width: Math.min(820, parent.width)
            spacing: 16
            Label {
                width: parent.width; wrapMode: Text.Wrap
                text: "Choose an Apollo project folder. Opening never replaces its writing with a recovery copy."
                font { family: "Liberation Sans"; pixelSize: 14 }
                color: "#303843"
            }
            TextField {
                id: projectFolder
                objectName: "openLocation"
                width: parent.width
                text: page.access.location
                enabled: !page.access.busy
                selectByMouse: true
                placeholderText: "/path/to/project"
                Accessible.name: "Existing Apollo project folder"
                onAccepted: if (!page.access.busy) page.access.open_project(text)
            }
            Button {
                objectName: "openProject"
                text: page.access.busy ? "Checking project…" : "Open Project"
                enabled: !page.access.busy && projectFolder.text.trim().length > 0
                onClicked: page.access.open_project(projectFolder.text)
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
                visible: !page.access.busy && !page.access.currentAvailable && page.access.error.length > 0 && page.access.candidates.length === 0
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
}
