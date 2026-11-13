cmake_minimum_required(VERSION 3.25)
project(shelf LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Test)
qt_standard_project_setup()

add_compile_options(-Wall -Wextra -Wpedantic)

add_library(shelfcore STATIC
    src/paint/bevel.cpp
    src/paint/sheet.cpp
    src/panel/layout.cpp
    src/palette/oklab.cpp
    src/palette/palette.cpp
    src/widgets/shelfwidget.h)
target_include_directories(shelfcore PUBLIC src)
target_link_libraries(shelfcore PUBLIC Qt6::Widgets)

add_executable(shelf src/main.cpp)
target_link_libraries(shelf PRIVATE shelfcore)

enable_testing()
add_subdirectory(tests)
