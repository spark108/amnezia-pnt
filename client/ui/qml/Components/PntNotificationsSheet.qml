import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Style 1.0
import "../Controls2"
import "../Controls2/TextTypes"

SheetType {
    id: root
    expandedHeight: windowHeight - 24

    onOpened: PntController.setNotificationsOpen(true)
    onAboutToHide: PntController.setNotificationsOpen(false)

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        NavigationBarType {
            Layout.fillWidth: true
            title: qsTr("Уведомления")
            backButtonVisible: false

            ButtonType {
                text: qsTr("Закрыть")
                variant: ButtonType.Text
                onClicked: root.close()
            }
        }

        BodyTextType {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            visible: notices.count === 0
            text: PntController.notificationsStatus || qsTr("Нет уведомлений")
            color: AmneziaStyle.color.textSecondary
            wrapMode: Text.WordWrap
        }

        ListView {
            id: notices
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            clip: true
            spacing: 12
            model: PntController.notifications
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBarType {}

            delegate: ColumnLayout {
                required property var modelData
                width: ListView.view.width
                spacing: 4

                CaptionTextType {
                    Layout.fillWidth: true
                    color: AmneziaStyle.color.textSecondary
                    text: new Date(modelData.created).toLocaleString(Qt.locale(), Locale.ShortFormat)
                }

                BodyTextType {
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    text: modelData.text
                    wrapMode: Text.Wrap
                }
            }
        }
    }
}
