import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Xipher.Desktop
import "../components"
import "../i18n/Translations.js" as I18n

// Telegram-style auth view
Item {
    id: root
    signal openSettings()

    // Simple background - Telegram style
    Rectangle {
        anchors.fill: parent
        color: Theme.bgPrimary
    }

    // Login panel - Telegram style (clean, simple)
    Rectangle {
        id: loginPanel
        width: 400
        height: loginContent.implicitHeight + Theme.spacingXl * 2
        anchors.centerIn: parent
        radius: Theme.radiusMd
        color: Theme.bgSidebar
        border.color: Theme.borderColor
        border.width: 1

        ColumnLayout {
            id: loginContent
            anchors.fill: parent
            anchors.margins: Theme.spacingXl
            spacing: Theme.spacingMd

            // Logo - Telegram style
            Column {
                Layout.alignment: Qt.AlignHCenter
                spacing: Theme.spacingMd

                // Logo icon
                Rectangle {
                    width: 100
                    height: 100
                    radius: 50
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: Theme.primary

                    Text {
                        anchors.centerIn: parent
                        text: "X"
                        color: "#FFFFFF"
                        font.family: Theme.fontFamily
                        font.pixelSize: 48
                        font.weight: Font.Bold
                    }
                }
                }

                Text {
                    text: "Xipher"
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: 28
                    font.weight: Font.Bold
                }

                Text {
                    text: I18n.t("auth.subtitle", SettingsStore.language)
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: 14
                }
            }

            Item { height: Theme.spacingSm }

            // Username field
            Column {
                Layout.fillWidth: true
                spacing: Theme.spacingXs

                Text {
                    text: I18n.t("auth.usernameLabel", SettingsStore.language)
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: 13
                    font.weight: Font.Medium
                }

                XTextField {
                    id: usernameField
                    width: parent.width
                    placeholderText: I18n.t("auth.username", SettingsStore.language)
                    text: AuthViewModel.username
                    onTextChanged: AuthViewModel.username = text
                    leftPadding: 40

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: Theme.spacingMd
                        anchors.verticalCenter: parent.verticalCenter
                        text: "👤"
                        font.pixelSize: 16
                        opacity: 0.6
                    }
                }
            }

            // Password field
            Column {
                Layout.fillWidth: true
                spacing: Theme.spacingXs

                Text {
                    text: I18n.t("auth.passwordLabel", SettingsStore.language)
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: 13
                    font.weight: Font.Medium
                }

                XTextField {
                    id: passwordField
                    width: parent.width
                    placeholderText: I18n.t("auth.password", SettingsStore.language)
                    echoMode: showPassword ? TextInput.Normal : TextInput.Password
                    text: AuthViewModel.password
                    onTextChanged: AuthViewModel.password = text
                    leftPadding: 40
                    rightPadding: 40

                    property bool showPassword: false

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: Theme.spacingMd
                        anchors.verticalCenter: parent.verticalCenter
                        text: "🔒"
                        font.pixelSize: 16
                        opacity: 0.6
                    }

                    MouseArea {
                        anchors.right: parent.right
                        anchors.rightMargin: Theme.spacingSm
                        anchors.verticalCenter: parent.verticalCenter
                        width: 28
                        height: 28
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true
                        onClicked: passwordField.showPassword = !passwordField.showPassword

                        Image {
                            anchors.centerIn: parent
                            width: 18
                            height: 18
                            source: passwordField.showPassword 
                                ? "qrc:/qt/qml/Xipher/Desktop/assets/icons/eye-off.svg" 
                                : "qrc:/qt/qml/Xipher/Desktop/assets/icons/eye.svg"
                            sourceSize: Qt.size(18, 18)
                            opacity: parent.containsMouse ? 0.9 : 0.6

                            Behavior on opacity {
                                NumberAnimation { duration: Theme.animFast }
                            }
                        }
                    }

                    Keys.onReturnPressed: AuthViewModel.login()
                }
            }

            // Error message
            Rectangle {
                Layout.fillWidth: true
                height: errorText.implicitHeight + Theme.spacingSm * 2
                radius: Theme.radiusSm
                color: Qt.rgba(Theme.error.r, Theme.error.g, Theme.error.b, 0.15)
                visible: AuthViewModel.errorKey.length > 0 || AuthViewModel.error.length > 0

                Row {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingSm
                    spacing: Theme.spacingSm

                    Text {
                        text: "⚠️"
                        font.pixelSize: 13
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        id: errorText
                        text: AuthViewModel.errorKey.length > 0
                              ? I18n.t(AuthViewModel.errorKey, SettingsStore.language)
                              : AuthViewModel.error
                        color: Theme.error
                        font.family: Theme.fontFamily
                        font.pixelSize: 13
                        wrapMode: Text.Wrap
                        width: parent.width - 30
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            Item { height: Theme.spacingXs }

            // Login button
            XButton {
                Layout.fillWidth: true
                text: AuthViewModel.busy
                      ? I18n.t("auth.loading", SettingsStore.language)
                      : I18n.t("auth.login", SettingsStore.language)
                enabled: !AuthViewModel.busy && usernameField.text.length > 0 && passwordField.text.length > 0
                
                onClicked: AuthViewModel.login()

                BusyIndicator {
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.spacingMd
                    anchors.verticalCenter: parent.verticalCenter
                    width: 24
                    height: 24
                    running: AuthViewModel.busy
                    visible: AuthViewModel.busy
                }
            }

            // Divider
            Row {
                Layout.fillWidth: true
                spacing: Theme.spacingMd

                Rectangle {
                    width: (parent.width - orText.width - Theme.spacingMd * 2) / 2
                    height: 1
                    color: Theme.borderColor
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    id: orText
                    text: I18n.t("auth.or", SettingsStore.language)
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: 12
                }

                Rectangle {
                    width: (parent.width - orText.width - Theme.spacingMd * 2) / 2
                    height: 1
                    color: Theme.borderColor
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            // Register button
            XButton {
                Layout.fillWidth: true
                text: I18n.t("auth.register", SettingsStore.language)
                variant: "secondary"
                onClicked: {
                    // TODO: Open registration
                }
            }

            // Settings link
            MouseArea {
                Layout.alignment: Qt.AlignHCenter
                width: settingsText.width
                height: settingsText.height
                cursorShape: Qt.PointingHandCursor
                onClicked: root.openSettings()

                Text {
                    id: settingsText
                    text: I18n.t("auth.settings", SettingsStore.language)
                    color: Theme.primary
                    font.family: Theme.fontFamily
                    font.pixelSize: 13

                    Behavior on color {
                        ColorAnimation { duration: 100 }
                    }
                }
            }
        }
    }

    // Version info
    Text {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: Theme.spacingMd
        text: "Xipher Desktop v1.0.0"
        color: Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: 11
    }
}
