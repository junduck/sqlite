add_library(test_utils INTERFACE)
target_include_directories(test_utils INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB TEST_SRC test_*.cpp)
add_executable(test_main main.cpp
  ${TEST_SRC})
target_link_libraries(test_main GTest::gtest_main test_utils ju::sqlite)

# Remove me
file(GLOB DUMMY_TEST dummy_*.cpp)
add_executable(test_dummy ${DUMMY_TEST})
target_link_libraries(test_dummy GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(test_main)

# Copy test data to the build directory
file(COPY "${CMAKE_SOURCE_DIR}/data" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/data")
