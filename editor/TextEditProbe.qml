import QtQuick

// Feasibility probe: a stock TextEdit showing a QTextDocument laid out by
// ScreenplayLayout. Page cards and generated text are separate items because
// they are not document text. Coordinates are 96 dpi pixels.
Rectangle {
    id: root
    required property var screenplayLayout
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
        }

        Repeater {
            model: root.screenplayLayout.decorations
            Text {
                required property var modelData
                x: modelData.x
                y: modelData.baseline - baselineOffset
                text: modelData.text
                font: root.screenplayLayout.font
                color: "black"
            }
        }
    }
}
