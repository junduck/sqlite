add_library(bench_utils INTERFACE)
target_include_directories(bench_utils INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Remove me
add_executable(bench_dummy dummy.cpp)
target_link_libraries(bench_dummy PRIVATE benchmark::benchmark bench_utils)
