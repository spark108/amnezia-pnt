import QtQuick
import QtQuick.Layouts
import "../Controls2/TextTypes"

ColumnLayout {
    spacing: 6

    Repeater {
        model: [
            qsTr("Подключение: %1").arg(PntController.connectionMode),
            qsTr("Пинг: %1").arg(PntController.ping),
            qsTr("IP внутри VPN: %1").arg(PntController.vpnAddress || "—"),
            qsTr("Внешний IP: %1").arg(PntController.externalAddress || qsTr("Проверка…")),
            qsTr("Рукопожатие: %1").arg(PntController.handshake),
            qsTr("Принято: %1").arg(PntController.received),
            qsTr("Отправлено: %1").arg(PntController.sent),
            qsTr("Скорость: ↓ %1 · ↑ %2").arg(PntController.receiveSpeed).arg(PntController.sendSpeed)
        ]
        delegate: BodyTextType {
            required property string modelData
            Layout.fillWidth: true
            text: modelData
            wrapMode: Text.Wrap
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }
    }
}
