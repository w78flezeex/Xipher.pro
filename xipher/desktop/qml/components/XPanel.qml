import QtQuick 2.15
import QtQuick.Controls 2.15
import Xipher.Desktop

Item {
    id: root
    property int padding: Theme.spacingLg
    property color backgroundColor: Theme.glass
    property color borderColor: Theme.glassBorder
    property bool elevated: true
    property bool glowEffect: false
    property int radius: Theme.radiusLg

    implicitWidth: contentItem.childrenRect.width + padding * 2
    implicitHeight: contentItem.childrenRect.height + padding * 2

    default property alias content: contentItem.data

    // Glow effect
    Rectangle {
        id: glow
        anchors.fill: panel
        anchors.margins: -8
        radius: panel.radius + 8
        visible: glowEffect
        opacity: 0.15
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Theme.purpleGlow }
            GradientStop { position: 1.0; color: Theme.accentBlue }
        }
        z: -2
    }

    // Shadow
    Rectangle {
        id: shadow
        anchors.fill: panel
        anchors.topMargin: elevated ? 4 : 0
        radius: panel.radius
        color: Theme.shadowGlow
        opacity: elevated ? 0.25 : 0.0
        z: -1

        Behavior on opacity { NumberAnimation { duration: Theme.animFast } }
    }

    Rectangle {
        id: panel
        anchors.fill: parent
        radius: root.radius
        color: backgroundColor
        border.color: borderColor
        border.width: 1

        Behavior on color { ColorAnimation { duration: Theme.animFast } }
        Behavior on border.color { ColorAnimation { duration: Theme.animFast } }
    }

    Item {
        id: contentItem
        anchors.fill: panel
        anchors.margins: padding
    }
}
