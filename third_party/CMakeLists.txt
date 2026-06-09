# Isolated third-party builds. We strip the project's strict compile options
# (notably /W4 /permissive-) for these subdirectories because openNURBS is
# legacy code and would not compile under our policies.

get_directory_property(_cadino_saved_opts COMPILE_OPTIONS)
set_property(DIRECTORY PROPERTY COMPILE_OPTIONS "")

if(MSVC)
    add_compile_options(/W0 /wd4996 /wd4267 /wd4244 /wd4189 /wd4100 /wd4101)
    # opennurbs_brep.cpp needs /bigobj
    add_compile_options(/bigobj)
endif()

# openNURBS pulls in its own bundled zlib (with Z_PREFIX so symbols don't clash
# with vcpkg's). The shared OpenNURBS DLL is unused — only opennurbsStatic
# survives because EXCLUDE_FROM_ALL keeps the SHARED target out of the default
# build, but we still avoid linking it.
add_subdirectory(opennurbs EXCLUDE_FROM_ALL)

# Tell consumers where the headers live.
target_include_directories(opennurbsStatic SYSTEM INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/opennurbs"
)

set_property(DIRECTORY PROPERTY COMPILE_OPTIONS "${_cadino_saved_opts}")
