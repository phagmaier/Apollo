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
    readonly property var suggestions: commands.suggestions
    readonly property var spelling: commands.spelling
    readonly property bool spellingOpen: spellingMenu.visible
    property int spellingRevision: 0
    Connections {
        target: root.spelling
        function onChanged() { ++root.spellingRevision }
        function onMenuChanged() { if (root.spelling.word.length === 0) spellingMenu.close() }
    }
    Connections {
        target: root.screenplayLayout
        function onLayoutChanged() { ++root.spellingRevision }
    }
    // Replacement removes its delegate; finish in the surviving root context.
    function applySpelling(replacement) {
        const position = spelling.replace(replacement)
        spellingMenu.close()
        editor.place(position)
    }
    function checkSpelling() {
        if (!enabled || editor.inputMethodComposing || editor.preeditText.length > 0) return
        suggestions.dismiss()
        spelling.request(editor.cursorPosition, editor.selectionStart, editor.selectionEnd)
        if (spelling.word.length > 0) spellingMenu.open()
    }
    onEnabledChanged: { if (!enabled) { suggestions.dismiss(); spelling.dismiss() } }
    color: "#d4d9df"
    function focusEditor() { if (!spellingMenu.visible) editor.forceActiveFocus() }
    readonly property bool hasSelection: editor.selectionStart !== editor.selectionEnd
    // Observe the installed document's history directly: TextEdit's property
    // notifications do not refresh bindings for these external cursor edits.
    readonly property bool canUndo: commands.canUndo
    readonly property bool canRedo: commands.canRedo
    readonly property bool canPaste: editor.canPaste
    function edit(action) {
        if (!enabled)
            return
        suggestions.dismiss()
        switch (action) {
        case "undo": editor.undo(); break
        case "redo": editor.redo(); break
        case "cut": editor.place(commands.cut(editor.selectionStart, editor.selectionEnd)); break
        case "copy": commands.copy(editor.selectionStart, editor.selectionEnd); break
        case "paste": editor.place(commands.paste(editor.cursorPosition, editor.selectionStart, editor.selectionEnd)); break
        case "selectAll": editor.selectAll(); break
        }
        editor.forceActiveFocus()
    }
    readonly property var elementTypes: [
        { type: "sceneHeading", label: "Scene heading", name: "Heading" },
        { type: "action", label: "Action", name: "Action" },
        { type: "character", label: "Character", name: "Character" },
        { type: "parenthetical", label: "Parenthetical", name: "Parenthetical" },
        { type: "dialogue", label: "Dialogue", name: "Dialogue" },
        { type: "transition", label: "Transition", name: "Transition" },
        { type: "lyric", label: "Lyric", name: "Lyric" },
        { type: "centered", label: "Centered / act label", name: "Centered" }
    ]
    readonly property string currentType: {
        commands.documentRevision // Also refresh after type changes, Undo and Redo at a stationary caret.
        return commands.elementTypeAt(editor.cursorPosition)
    }
    readonly property string typeLabel: {
        for (const entry of elementTypes)
            if (entry.type === currentType) return entry.label
        return currentType === "pageBreak" ? "Page break" : "Title page"
    }
    function setElementType(type) {
        if (enabled)
            commands.setElementType(editor.cursorPosition, editor.selectionStart, editor.selectionEnd, type)
        editor.forceActiveFocus()
    }
    function toggleDualDialogue() {
        if (enabled)
            commands.toggleDualDialogue(editor.cursorPosition, editor.selectionStart, editor.selectionEnd)
        editor.forceActiveFocus()
    }
    function toggleEmphasis(style) {
        if (enabled)
            commands.toggleEmphasis(editor.selectionStart, editor.selectionEnd, style)
        editor.forceActiveFocus()
    }
    function insertPageBreak() {
        editor.place(commands.insertPageBreak(editor.cursorPosition, editor.selectionStart, editor.selectionEnd))
        editor.forceActiveFocus()
    }
    function removePageBreak(position) {
        if (position === undefined)
            editor.place(commands.removePageBreak(editor.cursorPosition, editor.selectionStart, editor.selectionEnd))
        else
            editor.place(commands.removePageBreak(position, position, position))
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
                readonly property bool suggestionsOpen: suggestionMenu.visible
                readonly property var suggestionChoices: root.suggestions.choices
                readonly property int suggestionIndex: root.suggestions.selected
                property int suggestionTrigger: 0
                function offerSuggestions(explicitRequest = false) {
                    if (activeFocus && root.enabled && !inputMethodComposing && preeditText.length === 0)
                        root.suggestions.request(cursorPosition, selectionStart, selectionEnd, explicitRequest)
                }
                function offerCommittedSuggestions(position, revision, trigger) {
                    if (cursorPosition === position && (revision < 0 || root.commands.documentRevision === revision)
                            && suggestionTrigger === trigger)
                        offerSuggestions()
                }
                onCursorPositionChanged: { root.suggestions.cursorChanged(cursorPosition); root.spelling.dismiss() }
                onSelectionStartChanged: { root.suggestions.dismiss(); root.spelling.dismiss() }
                onSelectionEndChanged: { root.suggestions.dismiss(); root.spelling.dismiss() }
                onActiveFocusChanged: { if (!activeFocus) { root.suggestions.dismiss(); if (!spellingMenu.visible) root.spelling.dismiss() } }
                onInputMethodComposingChanged: { if (inputMethodComposing) { root.suggestions.dismiss(); root.spelling.dismiss() } }
                Connections {
                    target: root.commands
                    function onTextCommitted(position, revision) {
                        Qt.callLater(editor.offerCommittedSuggestions, position, revision, editor.suggestionTrigger)
                    }
                }
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
                    ++suggestionTrigger
                    if (event.key === Qt.Key_F7 && event.modifiers === Qt.NoModifier) {
                        root.checkSpelling()
                        event.accepted = true
                        return
                    }
                    const shift = event.modifiers & Qt.ShiftModifier
                    const command = event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)
                    const code = event.text.length > 0 ? event.text.charCodeAt(0) : 0
                    // Never accept/cycle or insert raw tabs/paragraphs during
                    // preedit. Other editing keys keep the protected command
                    // routes below, including clipboard and selected emphasis.
                    if ((inputMethodComposing || preeditText.length > 0)
                            && (event.key === Qt.Key_Tab || event.key === Qt.Key_Backtab
                                || event.key === Qt.Key_Return || event.key === Qt.Key_Enter)) {
                        // If the input method lets a structural key reach us,
                        // do not let stock TextEdit insert a raw tab/paragraph.
                        event.accepted = true
                        return
                    }
                    if (event.key === Qt.Key_Space && event.modifiers === Qt.ControlModifier) {
                        offerSuggestions(true)
                        event.accepted = true
                        return
                    }
                    if (suggestionsOpen && !command) {
                        if (event.key === Qt.Key_Up || event.key === Qt.Key_Down
                                || event.key === Qt.Key_Tab || event.key === Qt.Key_Backtab
                                || event.key === Qt.Key_Escape) {
                            if (event.key === Qt.Key_Up || event.key === Qt.Key_Down)
                                root.suggestions.move(event.key === Qt.Key_Up ? -1 : 1)
                            else if (event.key === Qt.Key_Tab && !shift)
                                place(root.suggestions.accept(cursorPosition, selectionStart, selectionEnd))
                            else if (event.key === Qt.Key_Escape)
                                root.suggestions.dismiss(true)
                            else {
                                root.suggestions.dismiss()
                                root.commands.cycleType(cursorPosition, true)
                            }
                            event.accepted = true
                            return
                        }
                    }
                    // Enter and commands dismiss; they retain their normal
                    // writing behavior and never accept a highlighted option.
                    root.suggestions.dismiss()
                    if (event.modifiers === Qt.ControlModifier && event.key >= Qt.Key_1 && event.key <= Qt.Key_8) {
                        root.setElementType(root.elementTypes[event.key - Qt.Key_1].type)
                        event.accepted = true
                        return
                    }
                    if (event.modifiers === (Qt.ControlModifier | Qt.ShiftModifier) && event.key === Qt.Key_D) {
                        root.toggleDualDialogue()
                        event.accepted = true
                        return
                    }
                    if ((event.modifiers & Qt.ControlModifier)
                            && (event.key === Qt.Key_Return || event.key === Qt.Key_Enter)) {
                        if (shift)
                            root.removePageBreak()
                        else
                            root.insertPageBreak()
                        event.accepted = true
                        return
                    }
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
                    } else if (event.key === Qt.Key_Backspace || event.key === Qt.Key_Delete
                               || event.matches(StandardKey.DeleteEndOfLine)) {
                        const result = root.commands.deleteText(cursorPosition, selectionStart, selectionEnd,
                                                                event.key === Qt.Key_Backspace)
                        if (result === -1)
                            return
                        place(result)
                    } else if (code >= 32 && code !== 127 && !command) {
                        place(root.commands.typeText(cursorPosition, selectionStart, selectionEnd, event.text))
                    } else {
                        return
                    }
                    event.accepted = true
                }
            }

            Repeater {
                model: {
                    root.spellingRevision
                    return root.spelling.marks((flick.contentY - pages.y) / root.pageScale,
                        (flick.contentY + flick.height - pages.y) / root.pageScale)
                }
                Rectangle {
                    required property var modelData
                    objectName: "spellingUnderline"
                    x: modelData.x; y: modelData.y
                    width: modelData.width; height: 1.5
                    color: "#bc4545"
                }
            }
            Repeater {
                model: root.screenplayLayout.pageBreaks
                Row {
                    required property var modelData
                    x: 144
                    y: modelData.y
                    spacing: 12
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.hasText ? "Page break — contains saved text" : "Page break"
                        font { family: "Liberation Sans"; pixelSize: 14 }
                        color: "#626e78"
                    }
                    Button {
                        objectName: "removePageBreakAt" + modelData.position
                        height: 24
                        text: "Remove"
                        font { family: "Liberation Sans"; pixelSize: 14 }
                        Accessible.name: modelData.hasText ? "Review page break containing saved text" : "Remove page break"
                        background: Rectangle {
                            color: parent.hovered ? "#e9edf0" : "transparent"
                            radius: 3
                            border.width: parent.visualFocus ? 2 : 0
                            border.color: "#90a9e1"
                        }
                        contentItem: Text { text: parent.text; font: parent.font; color: "#626e78"; verticalAlignment: Text.AlignVCenter }
                        onClicked: root.removePageBreak(modelData.position)
                        ToolTip.visible: hovered
                        ToolTip.text: "Remove only this marker, keeping both surrounding elements. Ctrl+Shift+Enter beside a break."
                    }
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

    // A non-focusing overlay keeps caret, selection and input-method ownership
    // in TextEdit. Coordinates include presentation scale and both scroll axes.
    Rectangle {
        id: suggestionMenu
        objectName: "writingSuggestions"
        visible: editor.activeFocus && root.enabled && !editor.inputMethodComposing
                 && root.suggestions.choices.length > 0
        z: 10
        readonly property rect caret: {
            flick.contentX; flick.contentY; pages.x; pages.y
            return editor.mapToItem(root, editor.cursorRectangle)
        }
        width: Math.min(320, root.width - 16)
        height: suggestionColumn.implicitHeight + 16
        x: Math.max(8, Math.min(caret.x, root.width - width - 8))
        y: caret.y + caret.height + height + 8 <= root.height
           ? caret.y + caret.height + 4 : Math.max(8, caret.y - height - 4)
        color: "#f5f4f1"; radius: 6; border.color: "#b9c0c7"
        clip: true
        Column {
            id: suggestionColumn
            x: 8; y: 8; width: parent.width - 16
            Label {
                width: parent.width; height: 26
                text: root.suggestions.component
                color: "#626e78"; font { family: "Liberation Sans"; pixelSize: 12 }
            }
            ListView {
                id: suggestionList
                objectName: "writingSuggestionList"
                width: parent.width
                height: Math.min(6 * 28, count * 28, Math.max(28, root.height - 90))
                clip: true
                model: root.suggestions.choices
                currentIndex: root.suggestions.selected
                onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)
                delegate: Rectangle {
                    required property string modelData
                    required property int index
                    objectName: "writingSuggestion" + index
                    width: suggestionColumn.width; height: 28; radius: 3
                    color: index === root.suggestions.selected ? "#dce4f2" : "transparent"
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            root.suggestions.select(index)
                            editor.place(root.suggestions.accept(editor.cursorPosition, editor.selectionStart, editor.selectionEnd))
                        }
                    }
                    Label {
                        anchors { fill: parent; leftMargin: 7; rightMargin: 7 }
                        text: modelData; textFormat: Text.PlainText; elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                        color: "#303843"; font { family: "Liberation Sans"; pixelSize: 13 }
                    }
                }
            }
            Label {
                width: parent.width; height: 26
                text: "↑↓ select · Tab accept · Esc dismiss"
                color: "#626e78"; font { family: "Liberation Sans"; pixelSize: 11 }
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
    Popup {
        id: spellingMenu
        objectName: "spellingMenu"
        parent: root
        x: Math.max(8, (root.width - width) / 2)
        y: Math.max(8, Math.min(60, root.height - height - 8))
        width: Math.min(390, root.width - 16)
        height: Math.min(root.height - 16, spellingContents.implicitHeight + 32)
        padding: 16; modal: true; focus: true
        onClosed: { root.spelling.dismiss(); editor.forceActiveFocus() }
        background: Rectangle { color: "#f5f4f1"; radius: 6; border.color: "#b9c0c7" }
        ScrollView {
            anchors.fill: parent
            contentWidth: availableWidth
            Column {
                id: spellingContents
                width: parent.width; spacing: 8
                Label { width: parent.width; text: "Spelling · US English"; font.pixelSize: 16 }
                Label { width: parent.width; text: root.spelling.word; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; font.bold: true }
                Label { width: parent.width; text: root.spelling.message; wrapMode: Text.Wrap }
                Repeater {
                    model: root.spelling.choices
                    Button {
                        required property string modelData
                        required property int index
                        objectName: "spellingReplacement" + index
                        width: spellingContents.width
                        text: modelData
                        onClicked: root.applySpelling(modelData)
                    }
                }
                Button {
                    objectName: "spellingAddWord"; text: "Add to personal dictionary"
                    enabled: root.spelling.word.length > 0 && root.spelling.word.length <= 128
                    onClicked: { if (root.spelling.addWord()) spellingMenu.close() }
                }
                Label { width: parent.width; text: root.spelling.error; visible: text.length > 0; wrapMode: Text.Wrap; color: "#9d3030" }
                Button { text: "Close"; objectName: "spellingClose"; onClicked: spellingMenu.close() }
            }
        }
    }

}
