import QtQuick 2.15
import QtQuick.Controls 2.15
import Xipher.Desktop

// Telegram-style action button for info panel
Rectangle {
    id: root

    property string icon: ""
    property string text: ""
    property bool danger: false
    property bool showArrow: false

    signal clicked()

    height: 48
    color: mouseArea.containsMouse ? Theme.bgSidebarHover : "transparent"

    Behavior on color {
        ColorAnimation { duration: Theme.animFast }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    Row {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacingLg
        anchors.rightMargin: Theme.spacingLg
        spacing: Theme.spacingMd

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.icon
            font.pixelSize: 18
            opacity: root.danger ? 1 : 0.7
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            color: root.danger ? Theme.error : Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: 14
        }
    }

    Text {
        anchors.right: parent.right
        anchors.rightMargin: Theme.spacingLg
        anchors.verticalCenter: parent.verticalCenter
        text: "›"
        color: Theme.textMuted
        font.pixelSize: 18
        visible: root.showArrow
    }
}
