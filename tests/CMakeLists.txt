add_executable(palette_test palette_test.cpp)
target_link_libraries(palette_test PRIVATE shelfcore Qt6::Test)
add_test(NAME palette COMMAND palette_test)

add_executable(sheet_test sheet_test.cpp)
target_link_libraries(sheet_test PRIVATE shelfcore Qt6::Test)
add_test(NAME sheet COMMAND sheet_test)
set_tests_properties(sheet PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
