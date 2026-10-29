import QtQuick
import QtQuick.Controls.Basic

// The spike editor: a stock TextEdit showing a QTextDocument laid out by
// ScreenplayLayout. Page cards and generated text are separate items because
// they are not document text. Keys with screenplay rules go to the C++
// commands; everything else is TextEdit's. Coordinates are 96 dpi pixels.
Rectangle {
    id: root
    required property var screenplayLayout
    required property var commands
    // Presentation scale only: the engine and TextEdit keep 96 dpi document
    // coordinates. 0.75 gives the prototype's 612 px Letter page and 12 px type.
    property real pageScale: 0.75
    color: "#d4d9df"
    function focusEditor() { editor.forceActiveFocus() }
    readonly property bool hasSelection: editor.selectionStart !== editor.selectionEnd
    function toggleEmphasis(style) {
        if (enabled)
            commands.toggleEmphasis(editor.selectionStart, editor.selectionEnd, style)
        editor.forceActiveFocus()
    }
    function selectElement(position, length) {
        if (position < 0)
            return
        editor.select(position, position + length)
        editor.forceActiveFocus()
        editor.revealCursor()
    }

    Flickable {
        id: flick
        objectName: "flick"
        anchors.fill: parent
        contentWidth: Math.max(width, root.screenplayLayout.pageWidth * root.pageScale + 64)
        contentHeight: root.screenplayLayout.documentHeight * root.pageScale + 36
        clip: true

        Item {
            id: pages
            x: (flick.contentWidth - width * scale) / 2
            y: 12
            width: root.screenplayLayout.pageWidth
            height: root.screenplayLayout.documentHeight
            scale: root.pageScale
            transformOrigin: Item.TopLeft

            Repeater {
                model: root.screenplayLayout.pageCount
                Rectangle {
                    required property int index
                    y: index * (root.screenplayLayout.pageHeight + root.screenplayLayout.pageGap)
                    width: root.screenplayLayout.pageWidth
                    height: root.screenplayLayout.pageHeight
                    color: "white"
                    Rectangle {
                        z: -1; x: 0; y: 5
                        width: parent.width; height: parent.height
                        color: "#172331"; opacity: 0.08
                    }
                    Rectangle {
                        z: -2; x: -4; y: 9
                        width: parent.width + 8; height: parent.height
                        color: "#172331"; opacity: 0.035
                    }
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
                textFormat: TextEdit.PlainText
                color: "black"
                selectionColor: "#8fb4e8"
                selectedTextColor: "black"
                function revealCursor() {
                    const rect = mapToItem(flick.contentItem, cursorRectangle)
                    if (rect.y < flick.contentY)
                        flick.contentY = Math.max(0, rect.y - 12)
                    else if (rect.y + rect.height > flick.contentY + flick.height)
                        flick.contentY = rect.y + rect.height - flick.height
                    if (rect.x < flick.contentX)
                        flick.contentX = Math.max(0, rect.x - 12)
                    else if (rect.x + rect.width > flick.contentX + flick.width)
                        flick.contentX = rect.x + rect.width - flick.width + 12
                }
                onCursorRectangleChanged: revealCursor()

                function place(position) {
                    if (position >= 0)
                        cursorPosition = position
                }

                Keys.priority: Keys.BeforeItem

                Keys.onPressed: (event) => {
                    const shift = event.modifiers & Qt.ShiftModifier
                    const command = event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)
                    const code = event.text.length > 0 ? event.text.charCodeAt(0) : 0
                    if (event.modifiers === Qt.ControlModifier
                            && (event.key === Qt.Key_B || event.key === Qt.Key_I || event.key === Qt.Key_U)) {
                        // Always consume these keys, even without a selection:
                        // Qt must never create an out-of-model typing mode.
                        root.toggleEmphasis(event.key === Qt.Key_B ? "bold" : event.key === Qt.Key_I ? "italic" : "underline")
                    } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        // Commands keep deliberate dialogue breaks in one speech.
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
        onWidthChanged: Qt.callLater(editor.revealCursor)
        onHeightChanged: Qt.callLater(editor.revealCursor)
        ScrollBar.vertical: ScrollBar {
            contentItem: Rectangle { implicitWidth: 5; radius: 2; color: "#89939e"; opacity: parent.active ? 0.8 : 0.4 }
        }
        ScrollBar.horizontal: ScrollBar {
            contentItem: Rectangle { implicitHeight: 5; radius: 2; color: "#89939e"; opacity: parent.active ? 0.8 : 0.4 }
        }
    }
}
