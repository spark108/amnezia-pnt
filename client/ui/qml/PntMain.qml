import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Controls
import Style 1.0
import "Controls2"
import "Controls2/TextTypes"
import "Components"

Window {
    id: root
    property string errorMessage: ""
    visible: true
    width: 440
    height: 800
    minimumWidth: 360
    minimumHeight: 540
    title: qsTr("PNT VPN")
    color: AmneziaStyle.color.bgBase

    onClosing: function(event) {
        if (Qt.platform.os === "android") {
            event.accepted = false
            root.hide()
        } else PntController.disconnectVpn()
    }

    Connections {
        target: PageController
        function onRaiseMainWindow() { root.show(); root.raise(); root.requestActivate() }
        function onShowErrorMessage(message) { root.errorMessage = message }
        function onShowNotificationMessage(message) { notification.show(message) }
        function onShowBusyIndicator(visible) { loading.visible = visible }
    }

    Connections {
        target: PntController
        function onServersReady() { if (PntController.servers.length > 0) serverSheet.open() }
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        contentWidth: availableWidth
        contentHeight: Math.max(content.implicitHeight + 56, availableHeight)
        clip: true

        ColumnLayout {
            id: content
            x: 28
            y: 28
            width: scroll.availableWidth - 56
            height: scroll.contentHeight - 56
            spacing: 20

            H1TextType {
                text: qsTr("PNT VPN")
                Layout.fillWidth: true
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
                onClicked: { root.errorMessage = ""; profileFile.open() }
            }

            ButtonType {
                Layout.fillWidth: true
                visible: PntController.hasProfile
                text: ConnectionController.isConnected || ConnectionController.isConnectionInProgress ? qsTr("Отключить") : qsTr("Подключить")
                enabled: !PntController.busy
                loading: PntController.busy
                onClicked: {
                    root.errorMessage = ""
                    if (ConnectionController.isConnected || ConnectionController.isConnectionInProgress) PntController.disconnectVpn()
                    else PntController.loadServers()
                }
            }

            ButtonType {
                objectName: "pntSplitButton"
                Layout.fillWidth: true
                variant: ButtonType.Text
                text: qsTr("Раздельное туннелирование")
                visible: !ConnectionController.isConnected && !ConnectionController.isConnectionInProgress
                enabled: !PntController.busy
                onClicked: splitSettings.open()
            }

            ButtonType {
                objectName: "pntResetButton"
                Layout.fillWidth: true
                visible: PntController.hasProfile && !ConnectionController.isConnected && !ConnectionController.isConnectionInProgress
                enabled: !PntController.busy
                variant: ButtonType.Text
                destructive: true
                text: qsTr("Сбросить профиль доступа")
                onClicked: resetConfirmation.open()
            }

            BodyTextType {
                objectName: "pntConnectionStatus"
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: root.errorMessage || PntController.status
                visible: text.length > 0 && text !== qsTr("Отключено") && text !== qsTr("Выберите VPN-сервер")
                         && text !== qsTr("Профиль PNT импортирован.")
                Accessible.role: Accessible.StaticText
                Accessible.name: text
            }

            PntConnectionStats {
                Layout.fillWidth: true
                visible: ConnectionController.isConnected
            }

            Item { Layout.fillHeight: true }

            PntUpdatePanel {
                Layout.fillWidth: true
            }
        }
    }

    FileDialog {
        id: profileFile
        title: qsTr("Импорт конфигурации PNT")
        nameFilters: [qsTr("Конфигурация PNT (*.pnt *.json)")]
        onAccepted: PntController.importProfile(selectedFile)
    }

    PntResetProfileSheet {
        id: resetConfirmation
        onConfirmed: PntController.resetProfile()
    }

    PntServerSheet { id: serverSheet }
    PntSplitTunnelSheet { id: splitSettings }
    SnackbarType { id: notification }
    LoadingOverlayType {
        id: loading
        anchors.centerIn: parent
        blurSource: scroll
    }
}
