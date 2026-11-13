add_executable(palette_test palette_test.cpp)
target_link_libraries(palette_test PRIVATE shelfcore Qt6::Test)
add_test(NAME palette COMMAND palette_test)
