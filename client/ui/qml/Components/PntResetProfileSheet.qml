import QtQuick
import QtQuick.Layouts
import "../Controls2"
import "../Controls2/TextTypes"

SheetType {
    id: root
    objectName: "pntResetConfirmation"
    property int digit: 1
    signal confirmed()
    expandedHeight: content.implicitHeight + 40 + PageController.safeAreaBottomMargin

    onAboutToShow: {
        digit = 1 + Math.floor(Math.random() * 9)
        confirmation.textField.text = ""
    }
    onClosed: {
        confirmation.textField.focus = false
        confirmation.textField.text = ""
        Qt.inputMethod.hide()
    }

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 20
        spacing: 16

        H2TextType {
            Layout.fillWidth: true
            text: qsTr("Сбросить профиль доступа")
            wrapMode: Text.Wrap
        }

        BodyTextType {
            Layout.fillWidth: true
            text: qsTr("Профиль будет удалён с этого устройства. Для подключения потребуется повторный импорт конфигурации.")
            wrapMode: Text.Wrap
        }

        TextFieldType {
            id: confirmation
            Layout.fillWidth: true
            headerText: qsTr("Для подтверждения введите цифру %1").arg(root.digit)
            textField.objectName: "pntResetDigit"
            textField.Accessible.name: headerText
            textField.inputMethodHints: Qt.ImhDigitsOnly
            textField.maximumLength: 1
            textField.validator: IntValidator { bottom: 1; top: 9 }
        }

        ButtonType {
            objectName: "pntResetConfirmButton"
            Layout.fillWidth: true
            variant: ButtonType.Destructive
            text: qsTr("Сбросить профиль")
            enabled: confirmation.textField.text === String(root.digit) && !PntController.busy
                     && !ConnectionController.isConnected && !ConnectionController.isConnectionInProgress
            onClicked: {
                root.close()
                root.confirmed()
            }
        }

        ButtonType {
            Layout.fillWidth: true
            variant: ButtonType.Text
            text: qsTr("Отмена")
            onClicked: root.close()
        }
    }
}
