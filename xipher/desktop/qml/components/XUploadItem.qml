import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Xipher.Desktop

Item {
    id: root
    property string tempId: ""
    property string fileName: ""
    property int progress: 0
    property string uploadState: "uploading" // uploading, completed, failed
    property string errorMessage: ""
    signal cancelRequested(string tempId)

    height: 48
    width: Math.max(200, row.implicitWidth + Theme.spacingMd * 2)

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: Theme.radiusMd
        color: Theme.bgTertiary
        border.color: {
            if (uploadState === "failed") return Theme.error
            if (uploadState === "completed") return Theme.success
            return Theme.borderSubtle
        }
        border.width: 1

        Behavior on border.color { ColorAnimation { duration: Theme.animFast } }
    }

    // Progress bar background
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 3
        radius: 1.5
        color: Theme.bgSecondary
        visible: uploadState === "uploading"

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width * (progress / 100)
            radius: 1.5
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Theme.purpleDark }
                GradientStop { position: 1.0; color: Theme.accentBlue }
            }

            Behavior on width { NumberAnimation { duration: 150 } }
        }
    }

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.margins: Theme.spacingSm
        spacing: Theme.spacingSm

        // File icon
        Rectangle {
            width: 32
            height: 32
            radius: Theme.radiusSm
            color: Theme.bgSecondary

            Text {
                anchors.centerIn: parent
                text: {
                    var ext = fileName.split('.').pop().toLowerCase()
                    if (["jpg", "jpeg", "png", "gif", "webp"].indexOf(ext) >= 0) return "🖼️"
                    if (["mp4", "mov", "avi", "webm"].indexOf(ext) >= 0) return "🎬"
                    if (["mp3", "wav", "ogg", "flac"].indexOf(ext) >= 0) return "🎵"
                    if (["pdf"].indexOf(ext) >= 0) return "📕"
                    if (["doc", "docx"].indexOf(ext) >= 0) return "📘"
                    if (["zip", "rar", "7z", "tar"].indexOf(ext) >= 0) return "📦"
                    return "📄"
                }
                font.pixelSize: 16
            }
        }

        // File info
        Column {
            Layout.fillWidth: true
            spacing: 2

            Text {
                text: fileName.length > 0 ? fileName : "file"
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: 12
                font.weight: Font.Medium
                elide: Text.ElideMiddle
                width: parent.width
            }

            Text {
                text: {
                    if (uploadState === "failed") return errorMessage || "Upload failed"
                    if (uploadState === "completed") return "Completed"
                    return progress + "% uploading..."
                }
                color: {
                    if (uploadState === "failed") return Theme.error
                    if (uploadState === "completed") return Theme.success
                    return Theme.textMuted
                }
                font.family: Theme.fontFamily
                font.pixelSize: 10
            }
        }

        // Status/Cancel button
        XIconButton {
            size: 28
            iconText: {
                if (uploadState === "completed") return "✓"
                if (uploadState === "failed") return "↻"
                return "✕"
            }
            variant: "ghost"
            onClicked: {
                if (uploadState === "uploading") {
                    root.cancelRequested(root.tempId)
                } else if (uploadState === "failed") {
                    // Retry logic
                    uploadState = "uploading"
                    progress = 0
                }
            }
        }
    }
}
