import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
import "../Controls2"
import "../Controls2/TextTypes"

ColumnLayout {
    spacing: 8

    BodyTextType {
        Layout.fillWidth: true
        text: qsTr("Версия %1").arg(PntUpdateController.currentVersion)
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    BodyTextType {
        Layout.fillWidth: true
        visible: text.length > 0
        wrapMode: Text.Wrap
        text: PntUpdateController.status
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    ButtonType {
        Layout.fillWidth: true
        variant: ButtonType.Text
        loading: PntUpdateController.busy
        enabled: !PntUpdateController.busy
        text: PntUpdateController.ready ? qsTr("Установить обновление")
              : PntUpdateController.busy && PntUpdateController.available ? qsTr("Загрузка: %1%").arg(PntUpdateController.progress)
              : PntUpdateController.available ? qsTr("Скачать версию %1").arg(PntUpdateController.latestVersion)
              : qsTr("Проверить обновления")
        onClicked: {
            if (PntUpdateController.ready) confirmation.open()
            else if (PntUpdateController.available) PntUpdateController.download()
            else PntUpdateController.check()
        }
    }

    MessageDialog {
        id: confirmation
        title: qsTr("Обновление PNT VPN")
        text: ConnectionController.isConnected || ConnectionController.isConnectionInProgress
              ? qsTr("Перед установкой отключите VPN.")
              : qsTr("Установить версию %1? Приложение будет перезапущено. Профиль и настройки сохранятся.").arg(PntUpdateController.latestVersion)
        buttons: ConnectionController.isConnected || ConnectionController.isConnectionInProgress
                 ? MessageDialog.Ok : MessageDialog.Yes | MessageDialog.No
        onButtonClicked: function(button) {
            if (button === MessageDialog.Yes) PntUpdateController.install()
        }
    }
}
