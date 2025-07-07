cmake_minimum_required(VERSION 3.22)
project(sqlite VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wshadow -Wconversion")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# External dependencies
include(cmake/ju_friends.cmake)

# Local dependencies
find_package(SQLite3 REQUIRED)

# Main library
add_library(sqlite INTERFACE)
add_library(ju::sqlite ALIAS sqlite)

target_include_directories(sqlite INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(sqlite INTERFACE SQLite::SQLite3 ju::tag_invoke)
target_compile_features(sqlite INTERFACE cxx_std_23)

include(GNUInstallDirs)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/ju/"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ju)
install(TARGETS sqlite EXPORT ${PROJECT_NAME}Targets)
install(EXPORT ${PROJECT_NAME}Targets
  FILE ${PROJECT_NAME}Targets.cmake
  NAMESPACE ju::
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cmake/ju)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/sqliteConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)
install(FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sqliteConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/sqliteConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cmake/ju
)

if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  include(cmake/GoogleBenchAndTest.cmake)

  # Dummy TU to make clangd happy
  add_executable(_clangd_tu src/_clangd_tu.cpp)
  target_link_libraries(_clangd_tu PRIVATE ju::sqlite)

  enable_testing()
  add_subdirectory(test)

  add_subdirectory(bench)

  add_library(sqlite3_exp STATIC ${CMAKE_SOURCE_DIR}/third_party/sqlite3/sqlite3.c)
  target_include_directories(sqlite3_exp PUBLIC ${CMAKE_SOURCE_DIR}/third_party/sqlite3)

  add_executable(update_hook_experiment src/update_hook_experiment.cpp)
  target_link_libraries(update_hook_experiment PRIVATE sqlite3_exp)
  target_include_directories(update_hook_experiment PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
endif()
