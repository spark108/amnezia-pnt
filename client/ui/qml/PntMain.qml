import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Dialogs
import Style 1.0
import "Controls2"
import "Controls2/TextTypes"
import "Components"

Window {
    id: root
    visible: true
    width: 440
    height: 640
    minimumWidth: 360
    minimumHeight: 540
    title: qsTr("PNT VPN")
    color: AmneziaStyle.color.bgBase

    onClosing: PntController.disconnectVpn()

    Connections {
        target: PageController
        function onRaiseMainWindow() { root.show(); root.raise(); root.requestActivate() }
    }

    Connections {
        target: PntController
        function onServersReady() { if (PntController.servers.length > 0) serverSheet.open() }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 20

        H1TextType {
            text: qsTr("PNT VPN")
            Layout.fillWidth: true
        }

        BodyTextType {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: PntController.hasProfile ? qsTr("Профиль доступа добавлен. Выберите конечный сервер при подключении.")
                                          : qsTr("Импортируйте конфигурацию доступа, выданную администратором PNT.")
        }

        Item { Layout.fillHeight: true }

        H2TextType {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: PntController.selectedServer
            wrapMode: Text.WordWrap
        }

        ButtonType {
            Layout.fillWidth: true
            visible: !PntController.hasProfile
            text: qsTr("Добавить конфигурацию")
            enabled: !PntController.busy
            loading: PntController.busy
            onClicked: profileFile.open()
        }

        ButtonType {
            Layout.fillWidth: true
            visible: PntController.hasProfile
            text: ConnectionController.isConnected || ConnectionController.isConnectionInProgress ? qsTr("Отключить") : qsTr("Подключить")
            enabled: !PntController.busy
            loading: PntController.busy
            onClicked: {
                if (ConnectionController.isConnected || ConnectionController.isConnectionInProgress) PntController.disconnectVpn()
                else PntController.loadServers()
            }
        }

        BodyTextType {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: PntController.status
        }

        Item { Layout.fillHeight: true }

        ButtonType {
            Layout.fillWidth: true
            visible: PntController.hasProfile
            enabled: !PntController.busy && !ConnectionController.isConnected && !ConnectionController.isConnectionInProgress
            variant: ButtonType.Text
            text: qsTr("Сбросить профиль доступа")
            onClicked: resetConfirmation.open()
        }
    }

    FileDialog {
        id: profileFile
        title: qsTr("Импорт конфигурации PNT")
        nameFilters: [qsTr("Конфигурация PNT (*.pnt *.json)")]
        onAccepted: PntController.importProfile(selectedFile)
    }

    MessageDialog {
        id: resetConfirmation
        title: qsTr("Сбросить профиль доступа")
        text: qsTr("Удалить импортированный профиль с этого устройства? Потребуется повторный импорт конфигурации.")
        buttons: MessageDialog.Yes | MessageDialog.No
        onButtonClicked: function(button) { if (button === MessageDialog.Yes) PntController.resetProfile() }
    }

    PntServerSheet { id: serverSheet }
}
