import QtQuick
import QtQuick.Controls.Basic

// Draft controls are deliberately separate from the screenplay document.
// Apply is the single undo/autosave boundary; Escape/Cancel changes nothing.
Popup {
    id: form
    required property var commands
    objectName: "titlePagePanel"
    width: Math.min(620, parent.width - 32)
    height: Math.min(760, parent.height - 32)
    x: (parent.width - width) / 2; y: (parent.height - height) / 2
    padding: 20; modal: true; focus: true
    closePolicy: Popup.CloseOnEscape
    property string error: ""
    readonly property var fields: [
        { key: "title", label: "Title" },
        { key: "credit", label: "Writing credit" },
        { key: "author", label: "Authors" },
        { key: "source", label: "Source / based-on credit" },
        { key: "contact", label: "Contact information" },
        { key: "draftDate", label: "Draft date" }
    ]
    background: Rectangle { color: "#f5f4f1"; radius: 8; border.color: "#b9c0c7" }
    ListModel { id: entries }

    function resetDraft() {
        entries.clear()
        const current = commands.titlePage()
        for (const entry of current)
            entries.append({ entryId: entry.id, field: entry.field, value: entry.text, removed: false })
        for (const field of fields) {
            if (!current.some(entry => entry.field === field.key))
                entries.append({ entryId: "", field: field.key, value: "", removed: false })
        }
        error = ""
        scroll.contentItem.contentY = 0
    }
    function applyDraft() {
        const desired = []
        for (let i = 0; i < entries.count; ++i) {
            const entry = entries.get(i)
            if (!entry.removed && (entry.entryId.length > 0 || entry.value.length > 0))
                desired.push({ id: entry.entryId, field: entry.field, text: entry.value })
        }
        if (commands.setTitlePage(desired))
            close()
        else
            error = "Title fields could not be applied. Keep this text, or cancel and reopen the form to load current fields."
    }
    function reveal(item) {
        const flick = scroll.contentItem
        const point = item.mapToItem(flick.contentItem, 0, 0)
        if (point.y < flick.contentY)
            flick.contentY = point.y
        else if (point.y + item.height > flick.contentY + flick.height)
            flick.contentY = point.y + item.height - flick.height
    }
    onAboutToShow: resetDraft()
    onOpened: {
        const first = sections.itemAt(0).firstInput()
        if (first) first.forceActiveFocus()
    }
    Shortcut { sequence: "Ctrl+Return"; enabled: form.opened; onActivated: form.applyDraft() }

    Column {
        id: heading
        width: parent.width; spacing: 8
        Label { text: "Title page"; color: "#303843"; font { family: "Liberation Sans"; pixelSize: 18; weight: Font.DemiBold } }
        Label {
            width: parent.width; wrapMode: Text.Wrap
            text: "Apollo places these fields. Apply saves the changes with your writing; Cancel leaves it unchanged."
            color: "#4d5965"; font { family: "Liberation Sans"; pixelSize: 12 }
        }
        Label {
            width: parent.width; wrapMode: Text.Wrap
            text: "Leave unused fields empty. Existing entries are kept even when blank; Remove deletes an entry. Multiline text is saved, but clean Fountain still requires nonblank single-line entries up to 48 characters and a title in capitals."
            color: "#4d5965"; font { family: "Liberation Sans"; pixelSize: 12 }
        }
    }
    ScrollView {
        id: scroll
        anchors { left: parent.left; right: parent.right; top: heading.bottom; topMargin: 16; bottom: actions.top; bottomMargin: 12 }
        clip: true; contentWidth: availableWidth
        Column {
            width: scroll.availableWidth; spacing: 16
            Repeater {
                id: sections
                model: form.fields
                delegate: Column {
                    id: section
                    required property var modelData
                    width: parent.width; spacing: 6
                    function firstInput() {
                        for (let i = 0; i < inputs.count; ++i) {
                            const row = inputs.itemAt(i)
                            if (row.visible) return row.input
                        }
                        return null
                    }
                    Label {
                        text: section.modelData.label; color: "#303843"
                        font { family: "Liberation Sans"; pixelSize: 13; weight: Font.DemiBold }
                    }
                    Repeater {
                        id: inputs
                        model: entries
                        delegate: Column {
                            id: row
                            required property int index
                            required property string entryId
                            required property string field
                            required property string value
                            required property bool removed
                            property alias input: input
                            width: section.width; spacing: 4
                            visible: field === section.modelData.key && !removed
                            TextArea {
                                id: input
                                objectName: row.visible ? "titleField_" + row.field + "_" + row.index : ""
                                width: parent.width - 12
                                height: Math.max(58, implicitHeight)
                                text: row.value; textFormat: TextEdit.PlainText
                                wrapMode: TextEdit.Wrap; selectByMouse: true
                                color: "#242733"; selectionColor: "#8fb4e8"; selectedTextColor: "black"
                                font { family: "Liberation Sans"; pixelSize: 14 }
                                Accessible.name: section.modelData.label + (row.entryId.length > 0 ? " entry" : " new entry")
                                Accessible.description: "Plain text. Enter inserts a line break. Tab moves to the next control. Apply commits all title fields as one undo step."
                                background: Rectangle {
                                    color: "white"; radius: 4
                                    border.width: input.activeFocus ? 2 : 1
                                    border.color: input.activeFocus ? "#506da9" : "#b9c0c7"
                                }
                                onTextChanged: if (text !== row.value) entries.setProperty(row.index, "value", text)
                                onActiveFocusChanged: if (activeFocus) Qt.callLater(() => form.reveal(row))
                                Keys.onTabPressed: (event) => { nextItemInFocusChain(true).forceActiveFocus(Qt.TabFocusReason); event.accepted = true }
                                Keys.onBacktabPressed: (event) => { nextItemInFocusChain(false).forceActiveFocus(Qt.BacktabFocusReason); event.accepted = true }
                            }
                            Button {
                                objectName: row.visible ? "titleRemove_" + row.index : ""
                                visible: row.entryId.length > 0 || row.value.length > 0
                                text: "Remove entry"
                                Accessible.name: "Remove " + section.modelData.label + " entry"
                                onClicked: {
                                    entries.setProperty(row.index, "removed", true)
                                    addEntry.forceActiveFocus()
                                }
                            }
                        }
                    }
                    Button {
                        id: addEntry
                        objectName: "titleAdd_" + section.modelData.key
                        text: "Add another entry"
                        Accessible.name: "Add another " + section.modelData.label + " entry"
                        onClicked: {
                            entries.append({ entryId: "", field: section.modelData.key, value: "", removed: false })
                            Qt.callLater(() => inputs.itemAt(entries.count - 1).input.forceActiveFocus())
                        }
                    }
                }
            }
        }
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    }
    Column {
        id: actions
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        spacing: 8
        Label { width: parent.width; text: form.error; visible: text.length > 0; wrapMode: Text.Wrap; color: "#9d3030" }
        Row {
            spacing: 12
            Button { objectName: "titleCancel"; text: "Cancel"; onClicked: form.close() }
            Button { objectName: "titleApply"; text: "Apply"; Accessible.name: "Apply title-page changes (Ctrl+Enter)"; onClicked: form.applyDraft() }
        }
    }
}
