import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Xipher.Desktop
import "../components"
import "../i18n/Translations.js" as I18n

Popup {
    id: root
    property string callState: "idle"
    property string contactName: "Unknown"
    property string contactAvatar: ""
    property bool isVideo: false

    modal: true
    focus: true
    width: 420
    height: 500
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    closePolicy: Popup.CloseOnEscape

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.animNormal; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.9; to: 1; duration: Theme.animNormal; easing.type: Easing.OutBack }
        }
    }

    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.animFast }
    }

    background: Rectangle {
        radius: Theme.radiusXl
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.bgSecondary }
            GradientStop { position: 1.0; color: Theme.bgPrimary }
        }
        border.color: Theme.glassBorder
        border.width: 1
    }

    Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, 0.7)
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingXl
        spacing: Theme.spacingLg

        Item { Layout.fillHeight: true; Layout.preferredHeight: 1 }

        // Avatar with animation
        Item {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 120
            Layout.preferredHeight: 120

            Rectangle {
                id: pulseRing
                anchors.centerIn: parent
                width: 140
                height: 140
                radius: 70
                color: "transparent"
                border.color: Theme.purpleGlow
                border.width: 3
                opacity: root.callState === "connecting" || root.callState === "ringing" ? 0.6 : 0

                SequentialAnimation on scale {
                    loops: Animation.Infinite
                    running: pulseRing.opacity > 0
                    NumberAnimation { from: 1; to: 1.3; duration: 1000; easing.type: Easing.OutQuad }
                    NumberAnimation { from: 1.3; to: 1; duration: 1000; easing.type: Easing.InQuad }
                }

                SequentialAnimation on opacity {
                    loops: Animation.Infinite
                    running: pulseRing.opacity > 0
                    NumberAnimation { from: 0.6; to: 0; duration: 2000 }
                }
            }

            XAvatar {
                id: avatar
                anchors.centerIn: parent
                size: 120
                name: root.contactName
                imageUrl: root.contactAvatar
                online: root.callState === "in_call"
            }
        }

        // Contact name
        Text {
            text: root.contactName
            font.family: Theme.fontFamilyDisplay
            font.pixelSize: 24
            font.weight: Font.DemiBold
            color: Theme.textPrimary
            Layout.alignment: Qt.AlignHCenter
        }

        // Status
        Text {
            text: {
                switch(root.callState) {
                    case "idle": return I18n.t("call.ready", SettingsStore.language) || "Ready to call"
                    case "connecting": return I18n.t("call.connecting", SettingsStore.language) || "Connecting..."
                    case "ringing": return I18n.t("call.ringing", SettingsStore.language) || "Ringing..."
                    case "in_call": return callTimer.text
                    default: return ""
                }
            }
            font.family: Theme.fontFamily
            font.pixelSize: 14
            color: Theme.textMuted
            Layout.alignment: Qt.AlignHCenter

            Timer {
                id: callTimer
                property int seconds: 0
                property string text: "00:00"
                interval: 1000
                repeat: true
                running: root.callState === "in_call"
                onTriggered: {
                    seconds++
                    var m = Math.floor(seconds / 60)
                    var s = seconds % 60
                    text = (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s
                }
            }
        }

        Item { Layout.fillHeight: true; Layout.preferredHeight: 2 }

        // Call controls
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Theme.spacingXl

            // Mute button
            XIconButton {
                size: 56
                iconText: "🎤"
                variant: "ghost"
                enabled: root.callState === "in_call"
                opacity: enabled ? 1 : 0.4
            }

            // Main action button (Start/End)
            Rectangle {
                width: 72
                height: 72
                radius: 36
                color: root.callState === "idle" ? Theme.success : Theme.error

                Text {
                    anchors.centerIn: parent
                    text: root.callState === "idle" ? "📞" : "📵"
                    font.pixelSize: 28
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (root.callState === "idle") {
                            root.callState = "connecting"
                        } else {
                            root.callState = "idle"
                            callTimer.seconds = 0
                            callTimer.text = "00:00"
                        }
                    }
                }

                Behavior on color { ColorAnimation { duration: Theme.animFast } }
            }

            // Video toggle
            XIconButton {
                size: 56
                iconText: root.isVideo ? "📹" : "📷"
                variant: "ghost"
                enabled: root.callState === "in_call"
                opacity: enabled ? 1 : 0.4
                onClicked: root.isVideo = !root.isVideo
            }
        }

        Item { Layout.fillHeight: true; Layout.preferredHeight: 1 }

        // Close button
        XButton {
            text: I18n.t("call.close", SettingsStore.language) || "Close"
            variant: "ghost"
            Layout.alignment: Qt.AlignHCenter
            onClicked: {
                root.callState = "idle"
                root.close()
            }
        }
    }

    Timer {
        interval: 1500
        repeat: false
        running: root.callState === "connecting"
        onTriggered: root.callState = "ringing"
    }

    Timer {
        interval: 2000
        repeat: false
        running: root.callState === "ringing"
        onTriggered: root.callState = "in_call"
    }
}
