import QtQuick
import QtQuick.Controls
import "../Controls2"
import "../Config"

SheetType {
    id: root
    expandedHeight: windowHeight - 24

    Loader {
        id: settings
        anchors.fill: parent
        active: root.visible

        sourceComponent: StackView {
            initialItem: PntSplitTunnelMenu { onCloseRequested: root.close() }
        }
    }

    Connections {
        target: PageController
        enabled: root.visible && settings.item !== null

        function goBack() {
            if (settings.item.depth > 1) settings.item.popCurrentItem()
            else root.close()
        }

        function onClosePage() {
            goBack()
        }
        function onEscapePressed() {
            goBack()
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
