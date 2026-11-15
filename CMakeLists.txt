cmake_minimum_required(VERSION 3.25)
project(shelf LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets DBus Network Test)
find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB REQUIRED IMPORTED_TARGET xcb xcb-composite xcb-damage)
qt_standard_project_setup()

add_compile_options(-Wall -Wextra -Wpedantic)

add_library(shelfcore STATIC
    src/media/coverart.cpp
    src/media/mpris.cpp
    src/media/player.cpp
    src/media/pulse.cpp
    src/paint/bevel.cpp
    src/paint/sheet.cpp
    src/panel/config.cpp
    src/panel/dock.cpp
    src/panel/editoverlay.cpp
    src/panel/layout.cpp
    src/panel/order.cpp
    src/panel/panel.cpp
    src/panel/popout.cpp
    src/panel/render.cpp
    src/panel/windowclass.cpp
    src/settings/historypage.cpp
    src/settings/imagepage.cpp
    src/settings/layoutpage.cpp
    src/settings/paletteeditor.cpp
    src/settings/palettepage.cpp
    src/settings/presetspage.cpp
    src/settings/rampstrip.cpp
    src/settings/settingswindow.cpp
    src/palette/export.cpp
    src/palette/extract.cpp
    src/palette/library.cpp
    src/palette/oklab.cpp
    src/palette/palette.cpp
    src/palette/scheme.cpp
    src/widgets/catalog.cpp
    src/widgets/clock.cpp
    src/widgets/foreignslot.cpp
    src/widgets/media.cpp
    src/widgets/medialayout.cpp
    src/widgets/mediapaint.cpp
    src/widgets/shelfwidget.h
    src/widgets/tasklayout.cpp
    src/widgets/tasks.cpp
    src/widgets/tray.cpp
    src/widgets/worldview.cpp
    src/x11/atoms.cpp
    src/x11/client.cpp
    src/x11/connection.cpp
    src/x11/reply.cpp
    src/x11/thumbnails.cpp
    src/x11/trace.cpp
    src/x11/windowmodel.cpp)
target_include_directories(shelfcore PUBLIC src)
qt_add_resources(shelfcore exports PREFIX / FILES
    exports/fvwm/target.ini exports/fvwm/template
    exports/gtk3/target.ini exports/gtk3/template
    exports/gtk4/target.ini exports/gtk4/template
    exports/kde/target.ini exports/kde/template
    exports/qt5ct/target.ini exports/qt5ct/template
    exports/qt6ct/target.ini exports/qt6ct/template)
target_link_libraries(shelfcore PUBLIC Qt6::Widgets Qt6::DBus Qt6::Network PkgConfig::XCB)

add_executable(shelf src/main.cpp)
target_link_libraries(shelf PRIVATE shelfcore)

enable_testing()
add_subdirectory(tests)
