cmake_minimum_required(VERSION 3.21)
project(MiniTime CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# 静态链接 CRT：目标机无需安装 VC++ 运行库
if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

set(MINITIME_SOURCES
    src/main.cpp
    src/app.cpp
    src/overlay_window.cpp
    src/clock_presenter.cpp
    src/layered_surface.cpp
    src/gdi_text_painter.cpp
    src/timefmt.cpp
    src/hotkey.cpp
    src/hotkey_key.cpp
    src/tray_icon.cpp
    src/tray_menu.cpp
    src/autostart.cpp
    src/config.cpp
    src/drag_editor.cpp
    src/game_profile.cpp
    src/text_input.cpp
    src/window_picker.cpp
    res/resource.rc
)

add_executable(MiniTime WIN32 ${MINITIME_SOURCES})

target_include_directories(MiniTime PRIVATE src res)

target_compile_definitions(MiniTime PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0A00
    WINVER=0x0A00
)

target_link_libraries(MiniTime PRIVATE
    user32
    gdi32
    shell32
    advapi32
    ole32
    comctl32
    shcore
)

# 版本号由 CI 注入；本地构建时回退默认值
if (NOT DEFINED GAMECLOCK_VERSION)
    set(GAMECLOCK_VERSION "0.0.0-dev")
endif()
target_compile_definitions(MiniTime PRIVATE MINITIME_VERSION="${GAMECLOCK_VERSION}")

if (MSVC)
    target_compile_options(MiniTime PRIVATE /O1 /GL /Gy /Gw /GR- /utf-8 /W3 /permissive-)
    target_link_options(MiniTime PRIVATE
        /LTCG /OPT:REF /OPT:ICF /INCREMENTAL:NO
        /SUBSYSTEM:WINDOWS
    )
else()
    target_compile_options(MiniTime PRIVATE
        -Os -flto -fno-exceptions -fno-rtti
        -ffunction-sections -fdata-sections
        -Wall -Wextra
    )
    target_link_options(MiniTime PRIVATE
        -Wl,--gc-sections -Wl,-s
        -static-libgcc -static-libstdc++
    )
endif()

# 产物统一输出到 build 根目录，便于 CI 校验与打包
set_target_properties(MiniTime PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    OUTPUT_NAME "MiniTime"
)