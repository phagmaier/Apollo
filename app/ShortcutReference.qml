import QtQuick
import QtQuick.Controls.Basic

Popup {
    id: reference
    objectName: "shortcutPanel"
    required property var elementTypes
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: Math.min(760, parent.width - 32)
    height: Math.min(720, parent.height - 32)
    padding: 20
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onOpened: closeButton.forceActiveFocus()
    background: Rectangle { color: "#f5f4f1"; radius: 8; border.color: "#b9c0c7" }

    component ShortcutRow: Item {
        required property string label
        required property string keys
        property string detail: ""
        property string keyName: ""
        implicitHeight: Math.max(description.implicitHeight, keyLabel.implicitHeight) + 12
        Column {
            id: description
            width: parent.width - keyLabel.width - 16
            spacing: 3
            Label {
                width: parent.width; text: label; wrapMode: Text.Wrap
                color: "#303843"; font { family: "Liberation Sans"; pixelSize: 13 }
            }
            Label {
                width: parent.width; text: detail; visible: text.length > 0; wrapMode: Text.Wrap
                color: "#626e78"; font { family: "Liberation Sans"; pixelSize: 12 }
            }
        }
        Label {
            id: keyLabel
            objectName: keyName
            anchors.right: parent.right
            width: Math.min(190, parent.width * 0.4)
            text: keys; wrapMode: Text.Wrap; horizontalAlignment: Text.AlignRight
            color: "#303843"; font { family: "Liberation Sans"; pixelSize: 13; weight: Font.DemiBold }
        }
    }
    component SectionTitle: Label {
        topPadding: 12; bottomPadding: 8
        color: "#303843"; font { family: "Liberation Sans"; pixelSize: 15; weight: Font.DemiBold }
    }

    Column {
        id: heading
        width: parent.width; spacing: 8
        Label {
            text: "Keyboard shortcuts"
            color: "#303843"; font { family: "Liberation Sans"; pixelSize: 20; weight: Font.DemiBold }
        }
        Label {
            width: parent.width; wrapMode: Text.Wrap
            text: "Use these while writing. Dialog fields have their own editing keys; actions depend on the current selection and project."
            color: "#626e78"; font { family: "Liberation Sans"; pixelSize: 12 }
        }
    }
    ScrollView {
        id: scroll
        objectName: "shortcutScroll"
        anchors { top: heading.bottom; topMargin: 12; bottom: closeButton.top; bottomMargin: 12; left: parent.left; right: parent.right }
        contentWidth: availableWidth
        clip: true
        focusPolicy: Qt.TabFocus
        KeyNavigation.tab: closeButton
        KeyNavigation.backtab: closeButton
        ScrollBar.vertical: ScrollBar { id: verticalBar; policy: ScrollBar.AlwaysOn }
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        Column {
            width: scroll.availableWidth - verticalBar.width - 12
            SectionTitle { text: "Editing" }
            ShortcutRow { width: parent.width; label: "Find / Find and Replace"; keys: "Ctrl+F / Ctrl+H"; detail: "Literal authored text, case/whole words and opening-selection scope. In Find, Enter/F3 goes next; Shift+Enter/Shift+F3 goes previous. Replace All is one undo step." }
            ShortcutRow { width: parent.width; label: "Spelling at caret"; keys: "F7 / Edit"; detail: "Review US English replacements or add a personal word for all scripts on this computer. No automatic corrections." }
            ShortcutRow { width: parent.width; label: "Undo"; keys: "Ctrl+Z"; keyName: "shortcutUndo" }
            ShortcutRow { width: parent.width; label: "Redo"; keys: "Ctrl+Shift+Z"; keyName: "shortcutRedo" }
            ShortcutRow { width: parent.width; label: "Cut / copy selection"; keys: "Ctrl+X / Ctrl+C" }
            ShortcutRow { width: parent.width; label: "Paste"; keys: "Ctrl+V"; detail: "External text keeps the current type. Apollo text keeps types and emphasis." }
            ShortcutRow { width: parent.width; label: "Select all"; keys: "Ctrl+A" }

            SectionTitle { text: "Writing and formatting" }
            ShortcutRow { width: parent.width; label: "Continue or split an element"; keys: "Enter / Shift+Enter"; detail: "Inside dialogue, insert a deliberate line break in the same speech. At its end, continue with action." }
            ShortcutRow { width: parent.width; label: "Next / previous common type"; keys: "Tab / Shift+Tab"; detail: "Scene heading → action → character → parenthetical → dialogue → transition." }
            ShortcutRow { width: parent.width; label: "Choose a writing suggestion"; keys: "↑ / ↓, then Tab"; detail: "Character names/extensions and individual scene-heading parts only. Enter keeps writing without accepting. Shift+Tab dismisses and cycles backward." }
            ShortcutRow { width: parent.width; label: "Dismiss / reopen suggestions"; keys: "Esc / Ctrl+Space"; detail: "Esc leaves text unchanged and suppresses this component's menu. Empty elements still allow Tab cycling." }
            ShortcutRow { width: parent.width; label: "Bold / italic / underline"; keys: "Ctrl+B / Ctrl+I / Ctrl+U"; detail: "Select text first. These keys do not set a style for future typing." }
            ShortcutRow { width: parent.width; label: "Choose element type"; keys: "Ctrl+0"; detail: "Opens the element list beside the page. Arrows and Enter, a number key or a click choose; Esc closes." }
            Repeater {
                model: reference.elementTypes
                ShortcutRow {
                    required property var modelData
                    required property int index
                    width: parent.width; label: modelData.label; keys: "Ctrl+" + (index + 1)
                }
            }
            ShortcutRow { width: parent.width; label: "Toggle Dual Dialogue"; keys: "Ctrl+Shift+D"; detail: "Use the second complete speech to pair with the preceding speech, or turn the pair off." }
            ShortcutRow { width: parent.width; label: "Insert manual page break"; keys: "Ctrl+Enter"; detail: "Keeps whole elements and speeches together; selected writing is retained." }
            ShortcutRow { width: parent.width; label: "Remove adjacent page break"; keys: "Ctrl+Shift+Enter"; detail: "Removes only the marker. Backspace / Delete beside a break also removes it." }

            SectionTitle { text: "Project and export" }
            ShortcutRow { width: parent.width; label: "Edit title page"; keys: "Ctrl+Shift+T"; detail: "In the title form, Ctrl+Enter applies changes; Esc cancels." }
            ShortcutRow { width: parent.width; label: "Save a Copy…"; keys: "Ctrl+Shift+S"; detail: "Writing saves automatically. A successful copy becomes the active project." }
            ShortcutRow { width: parent.width; label: "Save and return Home"; keys: "Ctrl+Shift+H" }
            ShortcutRow { width: parent.width; label: "Show / hide sidebar"; keys: "Ctrl+Shift+B"; detail: "The toolbar can be shown or hidden from the sidebar's Toolbar button. Both choices are remembered." }
            ShortcutRow { width: parent.width; label: "Show / hide scene outline"; keys: "Ctrl+\\"; detail: "Live headings follow writing and the caret. Click a heading to navigate; drag its handle to move the whole scene. Esc cancels a drag." }
            ShortcutRow { width: parent.width; label: "Move current scene up"; keys: "Alt+Shift+↑"; detail: "Moves the entire scene with its selection in one Undo step. Preamble and title fields stay put." }
            ShortcutRow { width: parent.width; label: "Export PDF immediately"; keys: "Ctrl+E"; keyName: "shortcutPdf"; detail: "Uses the current PDF destination and title-page choice. Use Export to review options." }
            ShortcutRow { width: parent.width; label: "Open Fountain export options"; keys: "Ctrl+Shift+E"; keyName: "shortcutFountain"; detail: "Clean export depends on the current writing's eligibility." }

            SectionTitle { text: "Help" }
            ShortcutRow { width: parent.width; label: "Keyboard shortcuts"; keys: "F1" }
            ShortcutRow { width: parent.width; label: "Close this reference"; keys: "Esc" }
        }
    }
    Button {
        id: closeButton
        objectName: "shortcutClose"
        anchors { right: parent.right; bottom: parent.bottom }
        text: "Back to writing"
        Accessible.name: "Close keyboard shortcuts (Esc)"
        KeyNavigation.tab: scroll
        KeyNavigation.backtab: scroll
        onClicked: reference.close()
    }
}
