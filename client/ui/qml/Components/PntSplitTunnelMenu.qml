import QtQuick
import QtQuick.Layouts
import "../Controls2"
import "../Controls2/TextTypes"

PageType {
    signal closeRequested()

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        NavigationBarType {
            Layout.fillWidth: true
            title: qsTr("Раздельное туннелирование")
            backButtonVisible: false
            ButtonType {
                text: qsTr("Закрыть")
                variant: ButtonType.Text
                onClicked: closeRequested()
            }
        }

        ListItemType {
            Layout.fillWidth: true
            text: qsTr("Сайты и IP-адреса")
            onClicked: stackView.push("qrc:/ui/qml/Pages2/PageSettingsSplitTunneling.qml")
        }

        ListItemType {
            Layout.fillWidth: true
            text: qsTr("Приложения")
            onClicked: stackView.push("qrc:/ui/qml/Pages2/PageSettingsAppSplitTunneling.qml")
        }

        BodyTextType {
            Layout.fillWidth: true
            Layout.margins: 16
            text: qsTr("Изменения применяются при следующем подключении.")
            wrapMode: Text.Wrap
        }
        Item { Layout.fillHeight: true }
    }
}
