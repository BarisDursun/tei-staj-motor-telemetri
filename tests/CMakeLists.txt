add_executable(test_enginemodel test_enginemodel.cpp)
target_link_libraries(test_enginemodel PRIVATE enginecore Qt5::Test)
add_test(NAME test_enginemodel COMMAND test_enginemodel)
