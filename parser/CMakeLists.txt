cmake_minimum_required(VERSION 3.10)
project(c-parser)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-Wall -Werror -Wshadow")

# Download GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(BUILD_GMOCK OFF CACHE BOOL "Builds the googlemock subproject" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "Enable installation of googletest" FORCE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)
enable_testing()

# Build tests
add_executable(c-parser-tests
    parser.c
    simple_tests.cpp
#    statement_tests.cpp
#    operator_tests.cpp
#    type_tests.cpp
#    struct_tests.cpp
#    declaration_tests.cpp
#   negative_tests.cpp
    ../scanner/scanner.c
    ast_print.c
    ast_free.c
)
target_include_directories(c-parser-tests BEFORE PUBLIC
    ../scanner
)
target_link_libraries(c-parser-tests GTest::gtest_main)
gtest_discover_tests(c-parser-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)
