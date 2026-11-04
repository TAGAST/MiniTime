#pragma once

// 菜单命令 ID：菜单层只生产这些值，由 App 统一分发。
// 放在独立头文件中，避免 config 与 tray_menu 相互依赖。
enum class MenuCommand : int {
    None = 0,

    // 主菜单
    ToggleShow   = 1001,
    Autostart    = 1002,
    Exit         = 1003,

    // 大小（整体缩放）
    Scale75      = 1101,
    Scale100     = 1102,
    Scale125     = 1103,
    Scale150     = 1104,
    Scale175     = 1105,

    // 字重
    WeightRegular = 1201,
    WeightMedium  = 1202,
    WeightBold    = 1203,

    // 颜色
    ColorWhite   = 1301,
    ColorCream   = 1302,
    ColorYellow  = 1303,
    ColorCyan    = 1304,
    ColorGreen   = 1305,

    // 位置（九宫格）
    AnchorTopLeft      = 1401,
    AnchorTopCenter    = 1402,
    AnchorTopRight     = 1403,
    AnchorMiddleLeft   = 1404,
    AnchorCenter       = 1405,
    AnchorMiddleRight  = 1406,
    AnchorBottomLeft   = 1407,
    AnchorBottomCenter = 1408,
    AnchorBottomRight  = 1409,
};

// 九宫格锚点
enum class Anchor {
    TopLeft,
    TopCenter,
    TopRight,
    MiddleLeft,
    Center,
    MiddleRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
};
