add_executable(cadino_core_tests
    test_wall.cpp
    test_document.cpp
    test_command.cpp
    test_box.cpp
)

target_link_libraries(cadino_core_tests
    PRIVATE
        cadino_core
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cadino_core_tests)

# Headless end-to-end smoke test exercising the full document/IO pipeline.
# Lives in a separate target because it pulls in cadino_ui (and therefore
# Qt + OCCT) — the lightweight core tests stay quick to run.
add_executable(cadino_smoke_tests
    test_smoke.cpp
)

target_link_libraries(cadino_smoke_tests
    PRIVATE
        cadino_core
        cadino_ui
        Qt6::Core
        Catch2::Catch2WithMain
)

# Make Qt and OCCT DLLs reachable when catch_discover_tests probes the
# binary at build time and when ctest later runs it.
get_target_property(_qt_core_loc Qt6::Core LOCATION)
get_filename_component(_qt_bin_dir "${_qt_core_loc}" DIRECTORY)
get_filename_component(_qt_prefix "${_qt_bin_dir}" DIRECTORY)
set(_qt_platforms_dir "${_qt_prefix}/plugins/platforms")
set(_dl_paths "${_qt_bin_dir}")
if(WIN32 AND EXISTS "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin")
    list(APPEND _dl_paths "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin")
endif()
if(WIN32 AND EXISTS "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/bin")
    list(APPEND _dl_paths "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/bin")
endif()

# Discover at ctest runtime (after env is set) so Qt can find its
# platform plugins and we don't pop a missing-plugin dialog at build time.
catch_discover_tests(cadino_smoke_tests
    DISCOVERY_MODE PRE_TEST
    DL_PATHS ${_dl_paths}
)

# Full GUI end-to-end: synthesizes mouse events on real widgets under the
# offscreen QPA plugin. Provides its own QApplication-bearing main().
add_executable(cadino_gui_tests
    test_gui.cpp
)

target_link_libraries(cadino_gui_tests
    PRIVATE
        cadino_app
        cadino_core
        cadino_ui
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Test
        Catch2::Catch2
)

set_target_properties(cadino_gui_tests PROPERTIES AUTOMOC ON)

# Bake the Qt platform-plugin directory into the test binary so that
# QApplication can spin up the offscreen plugin even when the test is
# launched without a deployment step (catch_discover_tests probes it
# during build).
target_compile_definitions(cadino_gui_tests PRIVATE
    "CADINO_QT_PLATFORMS_DIR=\"${_qt_platforms_dir}\""
)

catch_discover_tests(cadino_gui_tests
    DISCOVERY_MODE PRE_TEST
    DL_PATHS ${_dl_paths}
)
