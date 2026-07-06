# thirdparty/CMakeLists.txt — all four project deps as static libraries.
#
# Policy (design D7): every third-party dep is vendored and built static.
# No find_package(glfw3) / find_package(git2). git is runtime-only.
#
# Targets produced:
#   imgui_static            — ImGui core + GLFW/OpenGL3 backends
#   imgui_cte_static        — ImGuiColorTextEdit (TextEditor + TextDiff)
#   glfw_static             — GLFW3 from the vendored submodule (tag 3.4)
#   tinyfiledialogs_static  — single .c file
#
# All four are STATIC libs with position-independent code so they can link
# into the diffcue executable.

include(${CMAKE_SOURCE_DIR}/cmake/DiffcueDeps.cmake)

# ---------------------------------------------------------------------------
# imgui_static — core + backends we actually use (GLFW + OpenGL3).
# ---------------------------------------------------------------------------
add_library(imgui_static STATIC
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui_static PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends)
# Note: do NOT define IMGUI_DISABLE_OBSOLETE_FUNCTIONS — theme.txt uses the
# old ImGuiCol_TabActive / ImGuiCol_NavHighlight aliases, which are only
# available when obsolete functions are enabled (the default).
set_target_properties(imgui_static PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON)
diffcue_warnings_as_errors(TARGET imgui_static)

# ---------------------------------------------------------------------------
# imgui_cte_static — TextEditor + TextDiff (depends on imgui_static).
# ---------------------------------------------------------------------------
add_library(imgui_cte_static STATIC
    ImGuiColorTextEdit/TextEditor.cpp
    ImGuiColorTextEdit/TextDiff.cpp)
target_include_directories(imgui_cte_static PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/ImGuiColorTextEdit)
target_link_libraries(imgui_cte_static PUBLIC imgui_static)
set_target_properties(imgui_cte_static PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON)
diffcue_warnings_as_errors(TARGET imgui_cte_static)

# ---------------------------------------------------------------------------
# glfw_static — vendored GLFW 3.4 source snapshot, built as a static lib.
# ---------------------------------------------------------------------------
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(glfw)
# GLFW's own CMakeLists defines the `glfw` target; alias it for clarity.
if(TARGET glfw AND NOT TARGET glfw_static)
    add_library(glfw_static ALIAS glfw)
endif()
# ImGui's GLFW backend needs GLFW's headers — wire that via link deps.
target_link_libraries(imgui_static PUBLIC glfw)

# ---------------------------------------------------------------------------
# nfd (nativefiledialog-extended) — native folder picker.
# ---------------------------------------------------------------------------
set(NFD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NFD_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(nativefiledialog-extended)
