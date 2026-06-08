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
