import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Xipher.Desktop
import "../components"
import "../i18n/Translations.js" as I18n

// Telegram-style settings popup
Popup {
    id: root
    modal: true
    focus: true
    width: Math.min(parent.width * 0.9, 900)
    height: Math.min(parent.height * 0.9, 700)
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 0

    property int activeSection: 0

    // Load profile when opened
    onOpened: {
        SettingsViewModel.loadProfile()
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.animNormal }
    }

    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.animFast }
    }

    background: Rectangle {
        radius: Theme.radiusMd
        color: Theme.bgSidebar
        border.color: Theme.borderColor
        border.width: 1
    }

    Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, 0.5)
    }

    contentItem: Item {
        anchors.fill: parent

        // Top bar - Telegram style
        Rectangle {
            id: topBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: Theme.headerHeight
            color: Theme.bgHeader

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.borderColor
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingMd
                anchors.rightMargin: Theme.spacingMd
                spacing: Theme.spacingMd

                XIconButton {
                    size: 40
                    iconText: "←"
                    variant: "ghost"
                    onClicked: root.close()
                }

                Text {
                    text: I18n.t("settings.title", SettingsStore.language)
                    font.family: Theme.fontFamily
                    font.pixelSize: 16
                    font.weight: Font.Medium
                    color: Theme.textPrimary
                }

                Item { Layout.fillWidth: true }

                XIconButton {
                    size: 40
                    iconText: "⋮"
                    variant: "ghost"
                    onClicked: moreMenu.open()

                    Menu {
                        id: moreMenu
                        y: parent.height

                        background: Rectangle {
                            implicitWidth: 180
                            color: Theme.bgSecondary
                            border.color: Theme.borderColor
                            radius: Theme.radiusSm
                        }

                        MenuItem {
                            text: I18n.t("settings.editProfile", SettingsStore.language) || "Edit profile"
                            onTriggered: {}
                        }

                        MenuItem {
                            text: I18n.t("settings.logout", SettingsStore.language) || "Sign out"
                            onTriggered: {
                                console.log("Logout requested")
                                root.close()
                            }
                        }
                    }
                }

                XIconButton {
                    size: 40
                    iconText: "✕"
                    variant: "ghost"
                    onClicked: root.close()
                }
            }
        }

        // Main body with nav + content
        Row {
            anchors.top: topBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom

            // Left Navigation
            Rectangle {
                id: navPane
                width: 220
                height: parent.height
                color: Theme.bgSecondary

                Rectangle {
                    anchors.right: parent.right
                    width: 1
                    height: parent.height
                    color: Theme.borderColor
                }

                ListView {
                    id: navList
                    anchors.fill: parent
                    anchors.margins: Theme.spacingSm
                    spacing: 2
                    clip: true
                    currentIndex: root.activeSection

                    model: ListModel {
                        ListElement { icon: "👤"; textKey: "settings.nav.account"; section: "account" }
                        ListElement { icon: "🔔"; textKey: "settings.nav.notifications"; section: "notifications" }
                        ListElement { icon: "📞"; textKey: "settings.nav.calls"; section: "calls" }
                        ListElement { icon: "🔒"; textKey: "settings.nav.privacy"; section: "privacy" }
                        ListElement { icon: "📱"; textKey: "settings.nav.sessions"; section: "sessions" }
                        ListElement { icon: "🚫"; textKey: "settings.nav.blocked"; section: "blocked" }
                        ListElement { icon: "🌐"; textKey: "settings.nav.language"; section: "language" }
                        ListElement { icon: "⭐"; textKey: "settings.nav.premium"; section: "premium" }
                        ListElement { icon: "❓"; textKey: "settings.nav.faq"; section: "faq" }
                    }

                    delegate: Rectangle {
                        width: navList.width
                        height: 42
                        radius: Theme.radiusMd
                        color: navList.currentIndex === index 
                            ? Qt.rgba(Theme.purplePrimary.r, Theme.purplePrimary.g, Theme.purplePrimary.b, 0.18)
                            : (navItemMouse.containsMouse ? Theme.bgTertiary : "transparent")

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spacingMd
                            spacing: Theme.spacingMd

                            Text {
                                text: model.icon
                                font.pixelSize: 16
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Text {
                                text: I18n.t(model.textKey, SettingsStore.language) || model.section
                                font.family: Theme.fontFamily
                                font.pixelSize: 14
                                color: navList.currentIndex === index ? Theme.textPrimary : Theme.textSecondary
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        MouseArea {
                            id: navItemMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.activeSection = index
                                navList.currentIndex = index
                            }
                        }
                    }
                }
            }

            // Right Content Area
            Rectangle {
                width: parent.width - navPane.width
                height: parent.height
                color: "transparent"

                StackLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingLg
                    currentIndex: root.activeSection

                    // Account Section (0)
                    Flickable {
                        contentHeight: accountContent.implicitHeight
                        clip: true
                        ColumnLayout {
                            id: accountContent
                            width: parent.width
                            spacing: Theme.spacingMd

                            // Profile Card
                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: profileCol.implicitHeight + Theme.spacingLg * 2
                                radius: Theme.radiusMd
                                color: Theme.bgTertiary
                                border.color: Theme.borderSubtle

                                Column {
                                    id: profileCol
                                    anchors.fill: parent
                                    anchors.margins: Theme.spacingLg
                                    spacing: Theme.spacingMd

                                    Text {
                                        text: I18n.t("settings.account.profile", SettingsStore.language) || "Профиль"
                                        color: Theme.textPrimary
                                        font.pixelSize: 15
                                        font.weight: Font.Medium
                                    }

                                    Row {
                                        spacing: Theme.spacingLg

                                        XAvatar {
                                            size: 72
                                            text: Session.username.length > 0 ? Session.username.charAt(0).toUpperCase() : "U"
                                            imageUrl: SettingsViewModel.avatarUrl || ""
                                        }

                                        Column {
                                            spacing: Theme.spacingSm
                                            anchors.verticalCenter: parent.verticalCenter

                                            Text {
                                                text: I18n.t("settings.account.photoEdit", SettingsStore.language) || "Редактирование фото"
                                                color: Theme.textMuted
                                                font.pixelSize: 12
                                            }

                                            XButton {
                                                text: I18n.t("settings.account.changePhoto", SettingsStore.language) || "Сменить фото"
                                                variant: "secondary"
                                            }
                                        }
                                    }
                                }
                            }

                            // Basic Info Card
                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: basicCol.implicitHeight + Theme.spacingLg * 2
                                radius: Theme.radiusMd
                                color: Theme.bgTertiary
                                border.color: Theme.borderSubtle

                                Column {
                                    id: basicCol
                                    anchors.fill: parent
                                    anchors.margins: Theme.spacingLg
                                    spacing: Theme.spacingMd

                                    Text {
                                        text: I18n.t("settings.account.basic", SettingsStore.language) || "Основные данные"
                                        color: Theme.textPrimary
                                        font.pixelSize: 15
                                        font.weight: Font.Medium
                                    }

                                    GridLayout {
                                        columns: 2
                                        columnSpacing: Theme.spacingLg
                                        rowSpacing: Theme.spacingSm
                                        width: parent.width

                                        Text { text: I18n.t("settings.account.firstName", SettingsStore.language) || "Имя"; color: Theme.textSecondary; font.pixelSize: 14 }
                                        XTextField { 
                                            id: firstNameField
                                            Layout.preferredWidth: 200
                                            placeholderText: "Имя"
                                            text: SettingsViewModel.firstName
                                            onTextChanged: SettingsViewModel.firstName = text
                                        }

                                        Text { text: I18n.t("settings.account.lastName", SettingsStore.language) || "Фамилия"; color: Theme.textSecondary; font.pixelSize: 14 }
                                        XTextField { 
                                            id: lastNameField
                                            Layout.preferredWidth: 200
                                            placeholderText: "Фамилия"
                                            text: SettingsViewModel.lastName
                                            onTextChanged: SettingsViewModel.lastName = text
                                        }

                                        Text { text: I18n.t("settings.account.username", SettingsStore.language) || "Username"; color: Theme.textSecondary; font.pixelSize: 14 }
                                        XTextField { Layout.preferredWidth: 200; text: "@" + (Session.username || ""); enabled: false }
                                    }

                                    Text {
                                        text: I18n.t("settings.account.usernameNote", SettingsStore.language) || "Смена username будет доступна позже"
                                        color: Theme.textMuted
                                        font.pixelSize: 12
                                    }
                                }
                            }

                            XButton {
                                text: SettingsViewModel.profileLoading 
                                    ? (I18n.t("settings.loading", SettingsStore.language) || "Загрузка...")
                                    : (I18n.t("settings.save", SettingsStore.language) || "Сохранить")
                                enabled: !SettingsViewModel.profileLoading
                                onClicked: SettingsViewModel.saveProfile()
                            }
                        }
                    }

                    // Notifications Section (1)
                    Flickable {
                        contentHeight: notifContent.implicitHeight
                        clip: true
                        ColumnLayout {
                            id: notifContent
                            width: parent.width
                            spacing: Theme.spacingMd

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: notifCol.implicitHeight + Theme.spacingLg * 2
                                radius: Theme.radiusMd
                                color: Theme.bgTertiary
                                border.color: Theme.borderSubtle

                                Column {
                                    id: notifCol
                                    anchors.fill: parent
                                    anchors.margins: Theme.spacingLg
                                    spacing: Theme.spacingMd

                                    Text {
                                        text: I18n.t("settings.notifications.title", SettingsStore.language) || "Уведомления"
                                        color: Theme.textPrimary
                                        font.pixelSize: 15
                                        font.weight: Font.Medium
                                    }

                                    ToggleRow { 
                                        label: I18n.t("settings.notifications.desktop", SettingsStore.language) || "Уведомления рабочего стола"
                                        checked: SettingsViewModel.desktopNotifications
                                        onCheckedChanged: SettingsViewModel.desktopNotifications = checked
                                    }
                                    ToggleRow { 
                                        label: I18n.t("settings.notifications.sound", SettingsStore.language) || "Звук сообщений"
                                        checked: SettingsViewModel.soundNotifications
                                        onCheckedChanged: SettingsViewModel.soundNotifications = checked
                                    }
                                    ToggleRow { 
                                        label: I18n.t("settings.notifications.preview", SettingsStore.language) || "Показывать превью"
                                        checked: SettingsViewModel.showPreview
                                        onCheckedChanged: SettingsViewModel.showPreview = checked
                                    }
                                }
                            }
                        }
                    }

                    // Calls Section (2)
                    Flickable {
                        contentHeight: callsContent.implicitHeight
                        clip: true
                        ColumnLayout {
                            id: callsContent
                            width: parent.width
                            spacing: Theme.spacingMd

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: callsCol.implicitHeight + Theme.spacingLg * 2
                                radius: Theme.radiusMd
                                color: Theme.bgTertiary
                                border.color: Theme.borderSubtle

                                Column {
                                    id: callsCol
                                    anchors.fill: parent
                                    anchors.margins: Theme.spacingLg
                                    spacing: Theme.spacingMd

                                    Text {
                                        text: I18n.t("settings.calls.title", SettingsStore.language) || "Звонки"
                                        color: Theme.textPrimary
                                        font.pixelSize: 15
                                        font.weight: Font.Medium
                                    }

                                    ToggleRow { label: I18n.t("settings.calls.accept", SettingsStore.language) || "Принимать звонки"; checked: true }

                                    Row {
                                        width: parent.width
                                        spacing: Theme.spacingLg

                                        Text {
                                            text: I18n.t("settings.calls.who", SettingsStore.language) || "Кто может звонить"
                                            color: Theme.textSecondary
                                            font.pixelSize: 14
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        ComboBox {
                                            width: 150
                                            model: ["Все", "Друзья", "Никто"]
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Privacy Section (3)
                    Flickable {
                        contentHeight: privacyContent.implicitHeight
                        clip: true
                        ColumnLayout {
                            id: privacyContent
                            width: parent.width
                            spacing: Theme.spacingMd

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: privacyCol.implicitHeight + Theme.spacingLg * 2
                                radius: Theme.radiusMd
                                color: Theme.bgTertiary
                                border.color: Theme.borderSubtle

                                Column {
                                    id: privacyCol
                                    anchors.fill: parent
                                    anchors.margins: Theme.spacingLg
                                    spacing: Theme.spacingMd

                                    Text {
                                        text: I18n.t("settings.privacy.autoDelete", SettingsStore.language) || "Авто-удаление сообщений"
                                        color: Theme.textPrimary
                                        font.pixelSize: 15
                                        font.weight: Font.Medium
                                    }

                                    Row {
                                        spacing: Theme.spacingLg
                                        Text {
                                            text: I18n.t("settings.privacy.interval", SettingsStore.language) || "Интервал"
                                            color: Theme.textSecondary
                                            font.pixelSize: 14
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                        ComboBox {
                                            width: 150
                                            model: ["Выкл", "1 день", "1 неделя", "1 месяц"]
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Sessions Section (4)
                    Flickable {
                        contentHeight: sessionsContent.implicitHeight
                        clip: true
                        ColumnLayout {
                            id: sessionsContent
                            width: parent.width
                            spacing: Theme.spacingMd

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: sessionsCol.implicitHeight + Theme.spacingLg * 2
                                radius: Theme.radiusMd
                                color: Theme.bgTertiary
                                border.color: Theme.borderSubtle

                                Column {
                                    id: sessionsCol
                                    anchors.fill: parent
                                    anchors.margins: Theme.spacingLg
                                    spacing: Theme.spacingMd

                                    Text {
                                        text: I18n.t("settings.sessions.title", SettingsStore.language) || "Активные сеансы"
                                        color: Theme.textPrimary
                                        font.pixelSize: 15
                                        font.weight: Font.Medium
                                    }

                                    Row {
                                        spacing: Theme.spacingMd

                                        Rectangle {
                                            width: 40; height: 40; radius: 8
                                            color: Theme.bgSecondary
                                            Text { anchors.centerIn: parent; text: "💻"; font.pixelSize: 20 }
                                        }

                                        Column {
                                            Text { text: "Xipher Desktop"; color: Theme.textPrimary; font.pixelSize: 14 }
                                            Text { text: "Текущий сеанс"; color: Theme.success; font.pixelSize: 12 }
                                        }
                                    }
                                }
                            }

                            XButton {
                                text: I18n.t("settings.sessions.terminateAll", SettingsStore.language) || "Завершить все другие сеансы"
                                variant: "danger"
                            }
                        }
                    }

                    // Blocked Section (5)
                    Flickable {
                        contentHeight: blockedContent.implicitHeight
                        clip: true
                        ColumnLayout {
                            id: blockedContent
                            width: parent.width
                            spacing: Theme.spacingMd

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: blockedCol.implicitHeight + Theme.spacingLg * 2
                                radius: Theme.radiusMd
                                color: Theme.bgTertiary
                                border.color: Theme.borderSubtle

                                Column {
                                    id: blockedCol
                                    anchors.fill: parent
                                    anchors.margins: Theme.spacingLg
                                    spacing: Theme.spacingMd

                                    Text {
                                        text: I18n.t("settings.blocked.title", SettingsStore.language) || "Заблокированные"
                                        color: Theme.textPrimary
                                        font.pixelSize: 15
                                        font.weight: Font.Medium
                                    }

                                    Text {
                                        text: I18n.t("settings.blocked.empty", SettingsStore.language) || "Список пуст"
                                        color: Theme.textMuted
                                        font.pixelSize: 14
                                    }
                                }
                            }
                        }
                    }

                    // Language Section (6)
                    Flickable {
                        contentHeight: langContent.implicitHeight
                        clip: true
                        ColumnLayout {
                            id: langContent
                            width: parent.width
                            spacing: Theme.spacingMd

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: langCol.implicitHeight + Theme.spacingLg * 2
                                radius: Theme.radiusMd
                                color: Theme.bgTertiary
                                border.color: Theme.borderSubtle

                                Column {
                                    id: langCol
                                    anchors.fill: parent
                                    anchors.margins: Theme.spacingLg
                                    spacing: Theme.spacingMd

                                    Text {
                                        text: I18n.t("settings.language.title", SettingsStore.language) || "Язык и тема"
                                        color: Theme.textPrimary
                                        font.pixelSize: 15
                                        font.weight: Font.Medium
                                    }

                                    Row {
                                        spacing: Theme.spacingLg
                                        Text { text: I18n.t("settings.language.select", SettingsStore.language) || "Язык"; color: Theme.textSecondary; font.pixelSize: 14; anchors.verticalCenter: parent.verticalCenter }
                                        ComboBox {
                                            width: 180
                                            model: ["🇷🇺 Русский", "🇬🇧 English"]
                                            currentIndex: SettingsViewModel.language === "en" ? 1 : 0
                                            onActivated: SettingsViewModel.language = (currentIndex === 1 ? "en" : "ru")
                                        }
                                    }

                                    Row {
                                        spacing: Theme.spacingLg
                                        Text { text: I18n.t("settings.theme", SettingsStore.language) || "Тема"; color: Theme.textSecondary; font.pixelSize: 14; anchors.verticalCenter: parent.verticalCenter }
                                        ComboBox {
                                            width: 180
                                            model: ["🌙 Тёмная", "☀️ Светлая", "💻 Системная"]
                                            currentIndex: SettingsViewModel.theme === "light" ? 1 : (SettingsViewModel.theme === "system" ? 2 : 0)
                                            onActivated: {
                                                var themes = ["dark", "light", "system"]
                                                SettingsViewModel.theme = themes[currentIndex]
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Premium Section (7)
                    Flickable {
                        contentHeight: premiumContent.implicitHeight
                        clip: true
                        ColumnLayout {
                            id: premiumContent
                            width: parent.width
                            spacing: Theme.spacingMd

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: premiumCol.implicitHeight + Theme.spacingLg * 2
                                radius: Theme.radiusMd
                                color: Theme.bgTertiary
                                border.color: Theme.borderSubtle

                                Column {
                                    id: premiumCol
                                    anchors.fill: parent
                                    anchors.margins: Theme.spacingLg
                                    spacing: Theme.spacingMd

                                    Text {
                                        text: "⭐ Xipher Premium"
                                        color: Theme.textPrimary
                                        font.pixelSize: 18
                                        font.weight: Font.DemiBold
                                    }

                                    Text {
                                        text: I18n.t("settings.premium.description", SettingsStore.language) || "Получите больше возможностей с Premium подпиской"
                                        color: Theme.textSecondary
                                        font.pixelSize: 14
                                        wrapMode: Text.Wrap
                                        width: parent.width
                                    }

                                    XButton {
                                        text: I18n.t("settings.premium.upgrade", SettingsStore.language) || "Улучшить до Premium"
                                    }
                                }
                            }
                        }
                    }

                    // FAQ Section (8)
                    Flickable {
                        contentHeight: faqContent.implicitHeight
                        clip: true
                        ColumnLayout {
                            id: faqContent
                            width: parent.width
                            spacing: Theme.spacingMd

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: faqCol.implicitHeight + Theme.spacingLg * 2
                                radius: Theme.radiusMd
                                color: Theme.bgTertiary
                                border.color: Theme.borderSubtle

                                Column {
                                    id: faqCol
                                    anchors.fill: parent
                                    anchors.margins: Theme.spacingLg
                                    spacing: Theme.spacingMd

                                    Text {
                                        text: "Xipher FAQ"
                                        color: Theme.textPrimary
                                        font.pixelSize: 15
                                        font.weight: Font.Medium
                                    }

                                    Text {
                                        text: I18n.t("settings.faq.description", SettingsStore.language) || "Часто задаваемые вопросы"
                                        color: Theme.textSecondary
                                        font.pixelSize: 14
                                    }

                                    XButton {
                                        text: I18n.t("settings.faq.open", SettingsStore.language) || "Открыть FAQ"
                                        variant: "secondary"
                                        onClicked: Qt.openUrlExternally("https://xipher.pro/faq")
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Toggle Row Component
    component ToggleRow: Row {
        property string label: ""
        property bool checked: false
        signal checkedChanged(bool newValue)
        
        width: parent.width
        spacing: Theme.spacingLg

        Text {
            text: label
            color: Theme.textSecondary
            font.pixelSize: 14
            width: parent.width - 60
            anchors.verticalCenter: parent.verticalCenter
        }

        Switch {
            id: toggleSwitch
            checked: parent.checked
            onCheckedChanged: parent.checkedChanged(checked)
        }
    }
}
