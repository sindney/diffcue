# DiffcueDeps.cmake — helpers for the vendored-only dependency policy.
#
# diffcue links *only* vendored static libs (ImGui, ImGuiColorTextEdit,
# GLFW3). There is no find_package(glfw3) / find_package(git2) here: git is
# a runtime-only prerequisite (probed on PATH at startup), not a build
# dependency. No folder-picker library is linked — when diffcue is launched
# with no <folder> argument, it uses the current working directory.

# diffcue_apply_static_runtime(TARGET <name>)
#
# Force a static C/C++ runtime so the produced binary is self-contained:
#   - MSVC:  /MT (and /MTd in Debug) instead of the /MD default
#   - GCC/Clang (Linux): -static-libstdc++ -static-libgcc
#   - macOS:  no-op (static libstdc++ is not a thing on Darwin; the system
#             libc++ is the only option and is always present)
function(diffcue_apply_static_runtime)
    cmake_parse_arguments(ARG "" "TARGET" "" ${ARGN})
    if(NOT ARG_TARGET)
        message(FATAL_ERROR "diffcue_apply_static_runtime requires TARGET")
    endif()

    if(MSVC)
        # Replace /MD[x] with /MT[x] in the per-config runtime flags.
        foreach(lang C CXX)
            foreach(cfg "" DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
                set(flag_var "CMAKE_${lang}_FLAGS${cfg}")
                if(DEFINED ${flag_var})
                    string(REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
                    set(${flag_var} "${${flag_var}}" PARENT_SCOPE)
                endif()
            endforeach()
        endforeach()
    elseif(UNIX AND NOT APPLE)
        target_link_options(${ARG_TARGET} PRIVATE
            -static-libstdc++ -static-libgcc)
    endif()
endfunction()

# diffcue_apply_sanitizer(TARGET <name>)
#
# Wire ASan + UBSan (GCC/Clang) or ASan (MSVC) when DIFFCUE_SANITIZE is ON.
# No-op otherwise. Intended for the diffcue executable and the test binaries.
function(diffcue_apply_sanitizer)
    cmake_parse_arguments(ARG "" "TARGET" "" ${ARGN})
    if(NOT ARG_TARGET)
        message(FATAL_ERROR "diffcue_apply_sanitizer requires TARGET")
    endif()
    if(NOT DIFFCUE_SANITIZE)
        return()
    endif()

    if(MSVC)
        target_compile_options(${ARG_TARGET} PRIVATE /fsanitize=address)
    else()
        target_compile_options(${ARG_TARGET} PRIVATE
            -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${ARG_TARGET} PRIVATE
            -fsanitize=address,undefined)
    endif()
endfunction()

# diffcue_warnings_as_errors(TARGET <name>)
#
# Turn compiler warnings into errors on RelWithDebInfo/Release for the
# vendored targets and diffcue itself (task 2.8).
function(diffcue_warnings_as_errors)
    cmake_parse_arguments(ARG "" "TARGET" "" ${ARGN})
    if(NOT ARG_TARGET)
        message(FATAL_ERROR "diffcue_warnings_as_errors requires TARGET")
    endif()
    if(MSVC)
        target_compile_options(${ARG_TARGET} PRIVATE /WX)
    else()
        target_compile_options(${ARG_TARGET} PRIVATE -Werror)
    endif()
endfunction()
