import QtQuick
import QtQuick.Layouts
import "../Controls2"
import "../Controls2/TextTypes"

SheetType {
    id: root
    expandedHeight: Math.min(480, root.windowHeight - 80)

    ColumnLayout {
        width: parent.width
        spacing: 12

        H2TextType {
            Layout.fillWidth: true
            Layout.margins: 16
            text: qsTr("Выберите конечный сервер")
        }

        ListView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(100, root.expandedHeight - 130)
            clip: true
            model: PntController.servers
            delegate: ListItemType {
                required property var modelData
                width: ListView.view.width
                text: modelData.name
                subtitle: (modelData.ip || "—") + " · "
                          + (modelData.pingMs >= 0 ? qsTr("%1 ms").arg(modelData.pingMs) : "—")
                onClicked: {
                    root.close()
                    PntController.connectServer(modelData.id)
                }
            }
        }

        ButtonType {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            text: qsTr("Отмена")
            variant: ButtonType.Text
            onClicked: root.close()
        }
    }
}
