import QtQuick
import QtQuick.Controls
import "../Controls2"
import "../Config"

SheetType {
    id: root
    expandedHeight: windowHeight - 24

    onClosed: settingsStack.pop(null, StackView.Immediate)

    StackView {
        id: settingsStack
        anchors.fill: parent
        initialItem: PntSplitTunnelMenu { onCloseRequested: root.close() }
    }

    Connections {
        target: PageController
        enabled: root.visible
        function onClosePage() {
            if (settingsStack.depth > 1) settingsStack.pop()
            else root.close()
        }
        function onEscapePressed() {
            if (settingsStack.depth > 1) settingsStack.pop()
            else root.close()
        }
    }

    Connections {
        target: Dialogs
        enabled: root.visible
        function onQuestionRequested(header, description, yes, no, accept, reject) {
            question.headerText = header
            question.descriptionText = description
            question.yesButtonText = yes
            question.noButtonText = no
            question.yesButtonFunction = function() { question.close(); if (accept) accept() }
            question.noButtonFunction = function() { question.close(); if (reject) reject() }
            question.open()
        }
    }

    QuestionDrawer { id: question }
}
