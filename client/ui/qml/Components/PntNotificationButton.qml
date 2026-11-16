import QtQuick
import Style 1.0
import "../Controls2"
import "../Controls2/TextTypes"

ButtonType {
    id: root
    property int unreadCount: 0

    variant: ButtonType.Text
    size: ButtonType.Medium
    text: "\uD83D\uDD14\uFE0E"
    Accessible.name: unreadCount > 0 ? qsTr("Непрочитанных уведомлений: %1").arg(unreadCount) : qsTr("Уведомления")

    Rectangle {
        anchors.top: parent.top
        anchors.right: parent.right
        visible: root.unreadCount > 0
        color: AmneziaStyle.color.statusDestructive
        width: Math.max(height, count.implicitWidth + 8)
        height: 18
        radius: height / 2

        CaptionTextType {
            id: count
            anchors.centerIn: parent
            color: AmneziaStyle.color.textInverted
            text: root.unreadCount > 99 ? "99+" : root.unreadCount.toString()
        }
    }
}
