pragma Singleton
import QtQuick 2.15

QtObject {
    property string mode: "dark"

    FontLoader {
        id: interRegular
        source: "qrc:/qt/qml/Xipher/Desktop/assets/fonts/Inter-Regular.ttf"
    }

    FontLoader {
        id: interSemiBold
        source: "qrc:/qt/qml/Xipher/Desktop/assets/fonts/Inter-SemiBold.ttf"
    }

    property string fontFamily: interRegular.status == FontLoader.Ready ? interRegular.name : "Inter"
    property string fontFamilyDisplay: interSemiBold.status == FontLoader.Ready
        ? interSemiBold.name
        : "Inter"

    // ==========================================
    // TELEGRAM-STYLE DESIGN WITH BLUE BRAND
    // ==========================================

    // Primary blue palette (Xipher brand - синий)
    readonly property color primary: mode == "light" ? "#0088CC" : "#2AABEE"
    readonly property color primaryDark: mode == "light" ? "#006699" : "#229ED9"
    readonly property color primaryLight: mode == "light" ? "#5BC0F2" : "#64B5F6"
    readonly property color primaryHover: mode == "light" ? "#0077B5" : "#3BB5EF"

    // Legacy purple names -> now blue (for compatibility)
    readonly property color purplePrimary: primary
    readonly property color purpleDark: primaryDark
    readonly property color purpleLight: primaryLight
    readonly property color purpleGlow: primaryLight

    // Background colors - Telegram style (solid, no glass)
    readonly property color bgPrimary: mode == "light" ? "#FFFFFF" : "#17212B"
    readonly property color bgSecondary: mode == "light" ? "#F1F1F1" : "#0E1621"
    readonly property color bgTertiary: mode == "light" ? "#E7E7E7" : "#242F3D"
    readonly property color bgChat: mode == "light" ? "#E6EBEE" : "#0E1621"
    readonly property color bgHeader: mode == "light" ? "#FFFFFF" : "#17212B"

    // Sidebar colors
    readonly property color bgSidebar: mode == "light" ? "#FFFFFF" : "#17212B"
    readonly property color bgSidebarHover: mode == "light" ? "#F4F4F5" : "#202B36"
    readonly property color bgSidebarActive: mode == "light" ? "#E3F2FD" : "#2B5278"

    // Glass effect - simplified for Telegram style
    readonly property color glass: mode == "light" ? "#FFFFFF" : "#17212B"
    readonly property color glassBorder: mode == "light" ? "#E0E0E0" : "#293A4C"
    readonly property int glassBlur: 0  // No blur - flat design

    // Accent colors
    readonly property color accentColor: primary
    readonly property color accentStrong: primaryDark
    readonly property color accentSoft: primaryLight
    readonly property color accentBlue: primary  // Same as primary now

    // Text colors - Telegram style
    readonly property color textPrimary: mode == "light" ? "#000000" : "#FFFFFF"
    readonly property color textSecondary: mode == "light" ? "#707579" : "#AAAAAA"
    readonly property color textMuted: mode == "light" ? "#999999" : "#6C7883"
    readonly property color textOnAccent: "#FFFFFF"
    readonly property color textLink: primary

    // Border colors - subtle, Telegram style
    readonly property color borderColor: mode == "light" ? "#E0E0E0" : "#293A4C"
    readonly property color borderHover: mode == "light" ? "#CCCCCC" : "#3D5A73"
    readonly property color borderSubtle: mode == "light" ? "#F0F0F0" : "#1F2936"
    readonly property color strokeStrong: mode == "light" ? "#D0D0D0" : "#3D5A73"

    // Status colors - Telegram style
    readonly property color success: "#5DC452"  // Telegram green
    readonly property color successGlow: Qt.rgba(0.365, 0.769, 0.322, 0.5)
    readonly property color warning: "#E8AB40"
    readonly property color error: "#E53935"
    readonly property color online: "#5DC452"  // Green online indicator

    // Message bubble colors - Telegram style
    readonly property color msgBubbleIn: mode == "light" ? "#FFFFFF" : "#182533"
    readonly property color msgBubbleInBorder: mode == "light" ? "#E0E0E0" : "#1F2F3F"
    readonly property color msgBubbleOut: mode == "light" ? "#EEFFDE" : "#2B5278"
    readonly property color msgBubbleOutStart: msgBubbleOut  // Flat, no gradient
    readonly property color msgBubbleOutEnd: msgBubbleOut    // Flat, no gradient

    // Message status colors
    readonly property color msgTimeSent: mode == "light" ? "#5DC452" : "#6AB7F5"
    readonly property color msgTimeReceived: mode == "light" ? "#999999" : "#6C7883"
    readonly property color msgCheck: mode == "light" ? "#4FAE4E" : "#6AB7F5"  // Read/sent checks

    // Sidebar specific - Telegram widths
    readonly property int sidebarWidth: 400
    readonly property int infoPanelWidth: 420

    // Shadow definitions - minimal for Telegram style
    readonly property color shadowGlow: Qt.rgba(0, 0, 0, 0.15)
    readonly property color shadowGlowStrong: Qt.rgba(0, 0, 0, 0.25)
    readonly property color shadowCard: Qt.rgba(0, 0, 0, 0.1)

    // Radius values - Telegram uses smaller radii
    readonly property int radiusXs: 4
    readonly property int radiusSm: 6
    readonly property int radiusMd: 8
    readonly property int radiusLg: 12
    readonly property int radiusXl: 16
    readonly property int radiusRound: 9999
    readonly property int radiusBubble: 12  // Message bubble radius

    // Spacing values
    readonly property int spacingXxs: 2
    readonly property int spacingXs: 4
    readonly property int spacingSm: 8
    readonly property int spacingMd: 12
    readonly property int spacingLg: 16
    readonly property int spacingXl: 20
    readonly property int spacingXxl: 24

    // Sizes - Telegram style
    readonly property int minHit: 44
    readonly property int avatarSm: 32
    readonly property int avatarMd: 42
    readonly property int avatarLg: 54
    readonly property int avatarXl: 90
    readonly property int iconSm: 16
    readonly property int iconMd: 20
    readonly property int iconLg: 24

    // Chat list item height
    readonly property int chatItemHeight: 72

    // Animation durations - snappier
    readonly property int animFast: 100
    readonly property int animNormal: 150
    readonly property int animSlow: 200

    // Header heights
    readonly property int headerHeight: 56
    readonly property int inputHeight: 48

    function dp(value) {
        return value;
    }

    function radius(value) {
        return value;
    }

    function focusRingColor() {
        return primary;
    }

    // Avatar color generator (Telegram-style palette)
    function avatarColor(index) {
        var colors = [
            "#EE6D6D", // Red
            "#E8AB40", // Orange
            "#A695E7", // Purple
            "#7BC862", // Green
            "#6EC9CB", // Cyan
            "#65AADD", // Blue
            "#EE7AAE", // Pink
            "#2AABEE"  // Primary blue
        ];
        return colors[index % colors.length];
    }

    // Legacy compatibility
    function gradientPurple() {
        return [primary, primaryDark];
    }

    function avatarGradient() {
        return [primary, primaryDark];
    }
}
