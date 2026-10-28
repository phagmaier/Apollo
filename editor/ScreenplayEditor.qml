import QtQuick

// The spike editor: a stock TextEdit showing a QTextDocument laid out by
// ScreenplayLayout. Page cards and generated text are separate items because
// they are not document text. Keys with screenplay rules go to the C++
// commands; everything else is TextEdit's. Coordinates are 96 dpi pixels.
Rectangle {
    id: root
    required property var screenplayLayout
    required property var commands
    color: "#d0d0d0"

    Flickable {
        id: flick
        objectName: "flick"
        anchors.fill: parent
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
