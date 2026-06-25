add_library(cadino_app STATIC
    MainWindow.cpp
    MainWindow.hpp
)

target_include_directories(cadino_app
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(cadino_app
    PUBLIC
        cadino_core
        cadino_ui
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::OpenGL
        Qt6::OpenGLWidgets
    PRIVATE
        spdlog::spdlog
)

if(CADINO_ENABLE_OPENNURBS)
    target_link_libraries(cadino_app PRIVATE cadino_io)
    target_compile_definitions(cadino_app PRIVATE CADINO_HAS_OPENNURBS=1)
endif()

set_target_properties(cadino_app PROPERTIES
    AUTOMOC ON
    POSITION_INDEPENDENT_CODE ON
)

qt_add_executable(cadino
    main.cpp
)

target_link_libraries(cadino
    PRIVATE
        cadino_app
        spdlog::spdlog
)

set_target_properties(cadino PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
    OUTPUT_NAME "Cadino"
)
