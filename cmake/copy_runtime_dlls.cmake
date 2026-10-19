# Copies the non-Windows DLLs an executable needs (libcurl, its TLS/compression libraries,
# libstdc++, ...) next to it, so the program also runs from a normal shell without the MSYS2
# bin folder on PATH.
# Usage: cmake -DEXE=<exe> -DDEST=<dir> -DSEARCH=<dll dir> -DOBJDUMP=<objdump> -P copy_runtime_dlls.cmake
if(POLICY CMP0207)
  cmake_policy(SET CMP0207 NEW)  # match exclude regexes against normalized (forward-slash) paths
endif()
set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM "windows+pe")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL "objdump")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND "${OBJDUMP}")

file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${EXE}"
    RESOLVED_DEPENDENCIES_VAR deps
    UNRESOLVED_DEPENDENCIES_VAR missing
    DIRECTORIES "${SEARCH}"
    PRE_EXCLUDE_REGEXES "^api-ms-.*" "^ext-ms-.*"
    POST_EXCLUDE_REGEXES ".*[/\\\\][Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\].*")

foreach(dll IN LISTS deps)
    file(COPY "${dll}" DESTINATION "${DEST}")
endforeach()
