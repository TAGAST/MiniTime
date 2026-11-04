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

    // 编辑模式
    EnterEditMode = 1501,

    // 热键：显示/隐藏
    ToggleHK_CtrlAltT   = 2001,
    ToggleHK_CtrlShiftT = 2002,
    ToggleHK_AltT       = 2003,
    ToggleHK_F9         = 2004,
    ToggleHK_F10        = 2005,
    ToggleHK_None1      = 2006,

    // 热键：调整位置
    EditHK_CtrlAltE   = 2101,
    EditHK_CtrlShiftE = 2102,
    EditHK_AltE       = 2103,
    EditHK_F11        = 2104,
    EditHK_F12        = 2105,
    EditHK_None2      = 2106,
};

// 九宫格锚点；Custom 表示由自由拖动产生的绝对坐标
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
    Custom,
};