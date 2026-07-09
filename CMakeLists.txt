cmake_minimum_required(VERSION 3.20)

project(diffcue
    VERSION 0.1.0
    DESCRIPTION "Diff reviewer + cue annotator for AI coding CLI review loops"
    LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Global settings
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to RelWithDebInfo if the caller did not pick a config.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

# Prefer static archives over shared libs when searching.
set(CMAKE_FIND_LIBRARY_SUFFIXES
    "${CMAKE_STATIC_LIBRARY_SUFFIX};${CMAKE_SHARED_LIBRARY_SUFFIX}")

# Export compile_commands.json for clangd / IDEs.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# MSVC: treat source files as UTF-8 so em-dashes / non-ASCII in comments
# don't trigger C4819 code-page warnings or mis-interpret bytes.
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Static C/C++ runtime (/MT instead of /MD) so the produced binary is
# self-contained — no MSVCP140.dll / VCRUNTIME140.dll dependency. This is
# the modern CMake 3.15+ way and works with the Visual Studio generator
# (unlike string-replacing CMAKE_CXX_FLAGS). Applies to all targets.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Threading standard library (std::thread / std::mutex / std::condition_variable).
find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(DIFFCUE_SANITIZE "Enable AddressSanitizer + UBSan on GCC/Clang (and ASan on MSVC)" OFF)
option(DIFFCUE_BUILD_TESTS "Build the diffcue unit/integration tests" ON)

# ---------------------------------------------------------------------------
# Version header (task 11.2): derive from git tag, fall back to 0.1.0-dev.
# ---------------------------------------------------------------------------
find_program(GIT_EXECUTABLE git)
if(GIT_EXECUTABLE)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --exact-match
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE DIFFCUE_GIT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE DIFFCUE_GIT_TAG_RC)
    if(NOT DIFFCUE_GIT_TAG_RC EQUAL 0)
        set(DIFFCUE_GIT_TAG "")
    endif()
endif()
if(DIFFCUE_GIT_TAG STREQUAL "")
    set(DIFFCUE_VERSION "${PROJECT_VERSION}-dev")
else()
    set(DIFFCUE_VERSION "${DIFFCUE_GIT_TAG}")
endif()
configure_file(
    ${CMAKE_SOURCE_DIR}/src/version.h.in
    ${CMAKE_BINARY_DIR}/generated/diffcue/version.h
    @ONLY)

# ---------------------------------------------------------------------------
# Vendored third-party static targets (section 2).
# ---------------------------------------------------------------------------
add_subdirectory(thirdparty)

# ---------------------------------------------------------------------------
# Helper module for diffcue-specific dependency wiring.
# ---------------------------------------------------------------------------
include(${CMAKE_SOURCE_DIR}/cmake/DiffcueDeps.cmake)

# ---------------------------------------------------------------------------
# diffcue_lib — all app source except main.cpp (shared by exe + tests).
# ---------------------------------------------------------------------------
file(GLOB_RECURSE DIFFCUE_LIB_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/*.h)
list(REMOVE_ITEM DIFFCUE_LIB_SOURCES "${CMAKE_SOURCE_DIR}/src/main.cpp")

add_library(diffcue_lib STATIC ${DIFFCUE_LIB_SOURCES})
diffcue_apply_static_runtime(TARGET diffcue_lib)
diffcue_apply_sanitizer(TARGET diffcue_lib)
target_include_directories(diffcue_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/thirdparty
    ${CMAKE_SOURCE_DIR}/thirdparty/imgui
    ${CMAKE_SOURCE_DIR}/thirdparty/imgui/backends
    ${CMAKE_SOURCE_DIR}/thirdparty/ImGuiColorTextEdit
    ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(diffcue_lib PUBLIC
    imgui_static
    imgui_cte_static
    glfw_static
    nfd
    Threads::Threads)

# OS OpenGL / windowing system libs (only remaining dynamic deps, per D7).
if(WIN32)
    target_link_libraries(diffcue_lib PUBLIC opengl32 gdi32 user32 shell32)
elseif(APPLE)
    find_library(OPENGL_FRAMEWORK OpenGL REQUIRED)
    target_link_libraries(diffcue_lib PUBLIC ${OPENGL_FRAMEWORK})
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(diffcue_lib PUBLIC OpenGL::GL)
endif()

# ---------------------------------------------------------------------------
# diffcue executable
# ---------------------------------------------------------------------------
add_executable(diffcue src/main.cpp)
diffcue_apply_static_runtime(TARGET diffcue)
diffcue_apply_sanitizer(TARGET diffcue)
target_link_libraries(diffcue PRIVATE diffcue_lib)

# ---------------------------------------------------------------------------
# Testing (section 10)
# ---------------------------------------------------------------------------
if(DIFFCUE_BUILD_TESTS AND EXISTS ${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt)
    enable_testing()
    add_subdirectory(tests)
endif()
