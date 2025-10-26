cmake_minimum_required(VERSION 3.15)
project(InspectionRegionManager)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPQXX REQUIRED libpqxx)
pkg_check_modules(GFLAGS REQUIRED gflags)
pkg_check_modules(NLOHMANN_JSON REQUIRED nlohmann_json)

# Data loader executable
# add_executable(data_loader
#     src/data_loader.cpp
# )

# target_include_directories(data_loader PRIVATE 
#     ${LIBPQXX_INCLUDE_DIRS}
#     ${GFLAGS_INCLUDE_DIRS}
# )

# target_link_libraries(data_loader
#     ${LIBPQXX_LIBRARIES}
#     ${GFLAGS_LIBRARIES}
# )

# Create a static library for the query engine logic
add_library(query_engine_lib STATIC
    src/query_engine.cpp
)

target_include_directories(query_engine_lib PUBLIC
    src # So consumers can find query_engine.h
    ${LIBPQXX_INCLUDE_DIRS}
    ${GFLAGS_INCLUDE_DIRS}
    ${NLOHMANN_JSON_INCLUDE_DIRS}
)

target_link_libraries(query_engine_lib PRIVATE
    ${LIBPQXX_LIBRARIES}
    ${GFLAGS_LIBRARIES}
    ${NLOHMANN_JSON_LIBRARIES}
)

# Query engine executable
add_executable(query_engine src/main.cpp) # main is now here
target_link_libraries(query_engine PRIVATE query_engine_lib)

# Compiler flags
# target_compile_options(data_loader PRIVATE -Wall -Wextra)
target_compile_options(query_engine_lib PRIVATE -Wall -Wextra)

# --- Unit/Integration Testing ---
enable_testing()

# Fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)

# Test executable
add_executable(query_engine_test
    tests/query_engine_test.cpp # This file has its own main() from gtest
)

target_link_libraries(query_engine_test PRIVATE
    GTest::gtest_main
    query_engine_lib # Link against our library
)
