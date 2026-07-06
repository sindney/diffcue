# tests/CMakeLists.txt — Catch2 harness (task 10.1, 10.6).
#
# Uses the Catch2 v3 amalgamated distribution from the vendored full repo.
# The amalgamated .cpp is compiled once into catch2_static; each test binary
# links it.

include(${CMAKE_SOURCE_DIR}/cmake/DiffcueDeps.cmake)

# --- Catch2 amalgamated static lib ---
add_library(catch2_static STATIC
    ${CMAKE_SOURCE_DIR}/thirdparty/Catch2/extras/catch_amalgamated.cpp)
target_include_directories(catch2_static PUBLIC
    ${CMAKE_SOURCE_DIR}/thirdparty/Catch2/extras)
set_target_properties(catch2_static PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON)

# --- Test sources ---
file(GLOB DIFFCUE_TEST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp)

# One test binary covering all unit tests.
add_executable(diffcue_tests
    ${DIFFCUE_TEST_SOURCES})
target_link_libraries(diffcue_tests PRIVATE
    catch2_static
    diffcue_lib)
target_include_directories(diffcue_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated)
target_compile_definitions(diffcue_tests PRIVATE
    DIFFCUE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
diffcue_apply_sanitizer(TARGET diffcue_tests)

# Register with CTest (Catch2's registered tests are auto-discovered).
include(${CMAKE_SOURCE_DIR}/thirdparty/Catch2/extras/Catch.cmake)
catch_discover_tests(diffcue_tests)
