cmake_minimum_required(VERSION 3.25)
project(shelf LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Test)
find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB REQUIRED IMPORTED_TARGET xcb xcb-composite xcb-damage)
qt_standard_project_setup()

add_compile_options(-Wall -Wextra -Wpedantic)

add_library(shelfcore STATIC
    src/paint/bevel.cpp
    src/paint/sheet.cpp
    src/panel/config.cpp
    src/panel/dock.cpp
    src/panel/layout.cpp
    src/panel/panel.cpp
    src/panel/render.cpp
    src/palette/oklab.cpp
    src/palette/palette.cpp
    src/widgets/catalog.cpp
    src/widgets/clock.cpp
    src/widgets/foreignslot.cpp
    src/widgets/shelfwidget.h
    src/widgets/tasklayout.cpp
    src/widgets/tasks.cpp
    src/x11/atoms.cpp
    src/x11/client.cpp
    src/x11/connection.cpp
    src/x11/reply.cpp
    src/x11/thumbnails.cpp
    src/x11/trace.cpp
    src/x11/windowmodel.cpp)
target_include_directories(shelfcore PUBLIC src)
target_link_libraries(shelfcore PUBLIC Qt6::Widgets PkgConfig::XCB)

add_executable(shelf src/main.cpp)
target_link_libraries(shelf PRIVATE shelfcore)

enable_testing()
add_subdirectory(tests)
