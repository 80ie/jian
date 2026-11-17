foreach(name palette export extract library scheme paletteeditor sheet layout order config dock clock client tasklayout thumbnails tray worldview panel player pulse coverart medialayout soundlayout)
    add_executable(${name}_test ${name}_test.cpp)
    target_link_libraries(${name}_test PRIVATE jiancore Qt6::Test)
    add_test(NAME ${name} COMMAND ${name}_test)
endforeach()
set_tests_properties(sheet clock worldview panel PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)

add_library(fakeplayer STATIC fakeplayer.cpp)
target_link_libraries(fakeplayer PUBLIC Qt6::DBus)
add_executable(fakeplayer_main fakeplayer_main.cpp)
set_target_properties(fakeplayer_main PROPERTIES OUTPUT_NAME fakeplayer)
target_link_libraries(fakeplayer_main PRIVATE fakeplayer Qt6::Gui)

add_executable(mpris_test mpris_test.cpp)
target_link_libraries(mpris_test PRIVATE jiancore fakeplayer Qt6::Test)
add_test(NAME mpris COMMAND mpris_test)
# The test starts its own bus; an address that leads nowhere catches anything that reaches for
# the session's.
set_tests_properties(mpris PROPERTIES ENVIRONMENT "DBUS_SESSION_BUS_ADDRESS=unix:path=/dev/null;DISPLAY=")
