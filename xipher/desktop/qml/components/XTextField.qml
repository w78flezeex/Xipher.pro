import QtQuick 2.15
import QtQuick.Controls 2.15
import Xipher.Desktop

// Telegram-style text field
TextField {
    id: root
    property bool showFocusRing: true
    property string leftIcon: ""
    property string rightIcon: ""
    property bool error: false

    signal rightIconClicked()

    implicitHeight: 44
    font.family: Theme.fontFamily
    font.pixelSize: 14
    color: Theme.textPrimary
    placeholderTextColor: Theme.textMuted
    leftPadding: leftIcon ? 40 : Theme.spacingMd
    rightPadding: rightIcon ? 40 : Theme.spacingMd
    topPadding: Theme.spacingSm
    bottomPadding: Theme.spacingSm
    selectByMouse: true
    selectionColor: Theme.primary
    selectedTextColor: "#FFFFFF"

    background: Rectangle {
        radius: Theme.radiusSm
        color: Theme.bgTertiary
        border.color: {
            if (root.error) return Theme.error
            if (root.activeFocus) return Theme.primary
            return Theme.borderColor
        }
        border.width: root.activeFocus ? 2 : 1

        Behavior on border.color { ColorAnimation { duration: Theme.animFast } }

        // Left icon
        Text {
            visible: root.leftIcon.length > 0
            text: root.leftIcon
            font.pixelSize: 16
            anchors.left: parent.left
            anchors.leftMargin: Theme.spacingMd
            anchors.verticalCenter: parent.verticalCenter
            opacity: 0.5
        }

        // Right icon button
        Text {
            visible: root.rightIcon.length > 0
            text: root.rightIcon
            font.pixelSize: 16
            anchors.right: parent.right
            anchors.rightMargin: Theme.spacingMd
            anchors.verticalCenter: parent.verticalCenter
            opacity: rightIconMa.containsMouse ? 1 : 0.5

            MouseArea {
                id: rightIconMa
                anchors.fill: parent
                anchors.margins: -8
                cursorShape: Qt.PointingHandCursor
                hoverEnabled: true
                onClicked: root.rightIconClicked()
            }
        }
    }
}
