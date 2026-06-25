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

catch_discover_tests(cadino_smoke_tests)

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

catch_discover_tests(cadino_gui_tests)
