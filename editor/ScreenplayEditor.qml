import QtQuick

// The spike editor: a stock TextEdit showing a QTextDocument laid out by
// ScreenplayLayout. Page cards and generated text are separate items because
// they are not document text. Keys with screenplay rules go to the C++
// commands; everything else is TextEdit's. Coordinates are 96 dpi pixels.
Rectangle {
    id: root
    required property var screenplayLayout
    required property var commands
    property var exportJob: null
    property string exportDestination: ""
    property bool includeTitle: true
    color: "#d0d0d0"

    function exportPdf() {
        if (exportJob && !exportJob.busy) {
            exportJob.request(destination.text, includeTitle)
            editor.forceActiveFocus()
        }
    }
    Shortcut { sequence: "Ctrl+E"; onActivated: root.exportPdf() }

    // Temporary spike controls; the approved look screen is separate work.
    Rectangle {
        id: exportBar
        width: parent.width
        height: root.exportJob ? 76 : 0
        visible: root.exportJob !== null
        color: "#eeeeee"
        Rectangle {
            x: 8; y: 8; width: 112; height: 28
            color: root.exportJob && root.exportJob.busy ? "#cccccc" : "#ffffff"
            border.color: "#999999"
            Text { anchors.centerIn: parent; text: "Export PDF (Ctrl+E)"; color: "black"; font.pixelSize: 11 }
            MouseArea { anchors.fill: parent; onClicked: root.exportPdf() }
        }
        TextInput {
            id: destination
            objectName: "exportDestination"
            x: 130; y: 13; width: Math.max(40, parent.width - 250); height: 22
            text: root.exportDestination
            selectByMouse: true; clip: true; color: "black"; font.pixelSize: 12
        }
        Text {
            x: parent.width - 112; y: 13
            text: (root.includeTitle ? "☑" : "☐") + " Title page"
            color: "black"; font.pixelSize: 12
            MouseArea { anchors.fill: parent; onClicked: root.includeTitle = !root.includeTitle }
        }
        Text {
            x: 8; y: 43; width: parent.width - 16
            text: root.exportJob ? root.exportJob.status : ""
            color: "#333333"; font.pixelSize: 12; elide: Text.ElideMiddle
        }
    }

    Flickable {
        id: flick
        objectName: "flick"
        anchors { left: parent.left; right: parent.right; top: exportBar.bottom; bottom: parent.bottom }
        contentWidth: root.screenplayLayout.pageWidth
        contentHeight: root.screenplayLayout.documentHeight
        clip: true

        Repeater {
            model: root.screenplayLayout.pageCount
            Rectangle {
                required property int index
                y: index * (root.screenplayLayout.pageHeight + root.screenplayLayout.pageGap)
                width: root.screenplayLayout.pageWidth
                height: root.screenplayLayout.pageHeight
                color: "white"
            }
        }

        TextEdit {
            id: editor
            objectName: "editor"
            width: root.screenplayLayout.pageWidth
            height: root.screenplayLayout.documentHeight
            focus: true
            selectByMouse: true
            persistentSelection: true
            color: "black"
            selectionColor: "#8fb4e8"
            selectedTextColor: "black"
            onCursorRectangleChanged: {
                if (cursorRectangle.y < flick.contentY)
                    flick.contentY = cursorRectangle.y
                else if (cursorRectangle.y + cursorRectangle.height > flick.contentY + flick.height)
                    flick.contentY = cursorRectangle.y + cursorRectangle.height - flick.height
            }

            function place(position) {
                if (position >= 0)
                    cursorPosition = position
            }

            Keys.onPressed: (event) => {
                const shift = event.modifiers & Qt.ShiftModifier
                const command = event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)
                const code = event.text.length > 0 ? event.text.charCodeAt(0) : 0
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    // Shift+Enter included: TextEdit would insert a line
                    // separator, which the engine does not break at.
                    place(root.commands.enter(cursorPosition, selectionStart, selectionEnd))
                } else if (event.key === Qt.Key_Tab || event.key === Qt.Key_Backtab) {
                    root.commands.cycleType(cursorPosition, event.key === Qt.Key_Backtab || shift)
                } else if (event.matches(StandardKey.Paste)) {
                    place(root.commands.paste(cursorPosition, selectionStart, selectionEnd))
                } else if (event.matches(StandardKey.Copy)) {
                    root.commands.copy(selectionStart, selectionEnd)
                } else if (event.matches(StandardKey.Cut)) {
                    place(root.commands.cut(selectionStart, selectionEnd))
                } else if (event.key === Qt.Key_Up && (event.modifiers & Qt.AltModifier) && shift) {
                    place(root.commands.moveSceneUp(cursorPosition))
                } else if (code >= 32 && code !== 127 && !command) {
                    place(root.commands.typeText(cursorPosition, selectionStart, selectionEnd, event.text))
                } else {
                    return
                }
                event.accepted = true
            }
        }

        Repeater {
            model: root.screenplayLayout.decorations
            Text {
                required property string label
                required property real runX
                required property real runBaseline
                required property bool bold
                required property bool italic
                required property bool underline
                x: runX
                y: runBaseline - baselineOffset
                text: label
                textFormat: Text.PlainText
                font.family: root.screenplayLayout.font.family
                font.pixelSize: root.screenplayLayout.font.pixelSize
                font.kerning: false
                font.hintingPreference: Font.PreferNoHinting
                font.bold: bold
                font.italic: italic
                font.underline: underline
                color: "black"
            }
        }
    }
}
