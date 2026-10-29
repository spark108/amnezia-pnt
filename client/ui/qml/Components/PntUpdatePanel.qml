import QtQuick
import Style 1.0
import "../Controls2"

ButtonType {
    id: root
    objectName: "pntVersionButton"
    property bool updateRequested: false
    property bool manualCheck: false
    property bool waitingForDisconnect: false
    signal notificationRequested(string message)

    variant: ButtonType.Text
    size: ButtonType.Small
    enabled: !PntUpdateController.busy
    loading: PntUpdateController.busy && PntUpdateController.available
    text: "<font color=\"" + AmneziaStyle.color.textSecondary + "\">"
          + PntUpdateController.currentVersion + "</font>"
          + (PntUpdateController.available ? " → <font color=\"" + AmneziaStyle.color.statusSuccess
             + "\">" + PntUpdateController.latestVersion + "</font>" : "")
          + (loading ? " · " + PntUpdateController.progress + "%" : "")
    Accessible.name: qsTr("Версия %1").arg(PntUpdateController.currentVersion)
                     + (PntUpdateController.available ? " → " + PntUpdateController.latestVersion : "")

    function installReadyUpdate() {
        if (!root.updateRequested || !PntUpdateController.ready) return
        if (ConnectionController.isConnected || ConnectionController.isConnectionInProgress) {
            if (!root.waitingForDisconnect) {
                root.waitingForDisconnect = true
                PntController.disconnectVpn()
            }
            return
        }
        root.updateRequested = false
        root.waitingForDisconnect = false
        PntUpdateController.install()
        root.notificationRequested(PntUpdateController.status)
    }

    onClicked: {
        if (PntUpdateController.ready) {
            root.updateRequested = true
            Qt.callLater(root.installReadyUpdate)
        } else if (PntUpdateController.available) {
            root.updateRequested = true
            PntUpdateController.download()
        } else {
            root.manualCheck = true
            PntUpdateController.check()
        }
    }

    Connections {
        target: PntUpdateController
        function onChanged() {
            if (PntUpdateController.busy) return
            if (root.updateRequested && PntUpdateController.ready) {
                Qt.callLater(root.installReadyUpdate)
            } else if (root.updateRequested || root.manualCheck) {
                root.updateRequested = false
                root.manualCheck = false
                if (!PntUpdateController.available || !PntUpdateController.ready)
                    root.notificationRequested(PntUpdateController.status)
            }
        }
    }

    Connections {
        target: ConnectionController
        function onConnectionStateChanged() {
            if (root.waitingForDisconnect) Qt.callLater(root.installReadyUpdate)
        }
    }
}