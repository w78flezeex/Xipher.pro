import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Xipher.Desktop
import "views"
import "components"
import "i18n/Translations.js" as I18n

ApplicationWindow {
    id: root
    width: 1280
    height: 800
    minimumWidth: 900
    minimumHeight: 600
    visible: true
    title: "Xipher Desktop"
    color: Theme.bgPrimary
    font.family: Theme.fontFamily

    Component.onCompleted: {
        Theme.mode = SettingsStore.effectiveTheme
    }

    Connections {
        target: SettingsStore
        function onThemeChanged() {
            Theme.mode = SettingsStore.effectiveTheme
        }
    }

    menuBar: MenuBar {
        Menu {
            title: I18n.t("menu.file", SettingsStore.language)
            MenuItem {
                text: I18n.t("menu.settings", SettingsStore.language)
                onTriggered: root.openSettings()
            }
            MenuItem {
                text: I18n.t("menu.quit", SettingsStore.language)
                onTriggered: Qt.quit()
            }
        }
    }

    SettingsView {
        id: settingsPopup
    }

    function openSettings() {
        settingsPopup.open()
    }

    Loader {
        id: contentLoader
        anchors.fill: parent
        sourceComponent: Session.isRestoring
            ? loadingComponent
            : (Session.isAuthed ? shellComponent : authComponent)
    }

    Component {
        id: loadingComponent
        Item {
            anchors.fill: parent
            
            // Simple background - Telegram style
            Rectangle {
                anchors.fill: parent
                color: Theme.bgPrimary
            }

            Column {
                anchors.centerIn: parent
                spacing: Theme.spacingLg

                // Logo - Telegram style
                Rectangle {
                    width: 80
                    height: 80
                    radius: 40
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: Theme.primary

                    Text {
                        anchors.centerIn: parent
                        text: "X"
                        font.family: Theme.fontFamily
                        font.pixelSize: 40
                        font.weight: Font.Bold
                        color: "#FFFFFF"
                    }
                }

                Text {
                    text: "Xipher"
                    font.family: Theme.fontFamily
                    font.pixelSize: 24
                    font.weight: Font.Medium
                    color: Theme.textPrimary
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Item { height: Theme.spacingMd; width: 1 }

                // Loading indicator - simple dots
                Row {
                    spacing: Theme.spacingSm
                    anchors.horizontalCenter: parent.horizontalCenter

                    Repeater {
                        model: 3
                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: Theme.primary

                            SequentialAnimation on opacity {
                                loops: Animation.Infinite
                                PauseAnimation { duration: index * 150 }
                                NumberAnimation { from: 0.3; to: 1; duration: 300 }
                                NumberAnimation { from: 1; to: 0.3; duration: 300 }
                                PauseAnimation { duration: (2 - index) * 150 }
                            }
                        }
                    }
                }

                Text {
                    text: I18n.t("auth.restoring", SettingsStore.language)
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: 14
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }
    }

    Component {
        id: authComponent
        AuthView {
            onOpenSettings: root.openSettings()
        }
    }

    Component {
        id: shellComponent
        ShellView {
            onOpenSettings: root.openSettings()
        }
    }
}
