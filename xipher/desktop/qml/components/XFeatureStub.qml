import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Xipher.Desktop

Popup {
    id: root
    property string titleText: "Feature"
    property string bodyText: "This feature is not enabled."
    property string icon: "🚧"

    modal: true
    focus: true
    width: 400
    height: 280
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.animNormal; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.95; to: 1; duration: Theme.animNormal; easing.type: Easing.OutCubic }
        }
    }

    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.animFast }
    }

    background: Rectangle {
        radius: Theme.radiusXl
        color: Theme.bgSecondary
        border.color: Theme.glassBorder
        border.width: 1
    }

    Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, 0.5)
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingXl
        spacing: Theme.spacingLg

        // Icon
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: 64
            height: 64
            radius: 32
            color: Theme.bgTertiary
            border.color: Theme.borderSubtle

            Text {
                anchors.centerIn: parent
                text: root.icon
                font.pixelSize: 28
            }
        }

        // Title
        Text {
            text: root.titleText
            font.family: Theme.fontFamilyDisplay
            font.pixelSize: 20
            font.weight: Font.DemiBold
            color: Theme.textPrimary
            Layout.alignment: Qt.AlignHCenter
        }

        // Body
        Text {
            text: root.bodyText
            font.family: Theme.fontFamily
            font.pixelSize: 14
            color: Theme.textMuted
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
            Layout.maximumWidth: 320
            Layout.alignment: Qt.AlignHCenter
        }

        Item { Layout.fillHeight: true }

        XButton {
            text: "Got it"
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 140
            onClicked: root.close()
        }
    }
}
