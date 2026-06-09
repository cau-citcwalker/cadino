qt_add_executable(cadino
    main.cpp
    MainWindow.cpp
    MainWindow.hpp
)

target_link_libraries(cadino
    PRIVATE
        cadino_core
        cadino_ui
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::OpenGL
        Qt6::OpenGLWidgets
        spdlog::spdlog
)

if(CADINO_ENABLE_OPENNURBS)
    target_link_libraries(cadino PRIVATE cadino_io)
    target_compile_definitions(cadino PRIVATE CADINO_HAS_OPENNURBS=1)
endif()

set_target_properties(cadino PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
    OUTPUT_NAME "Cadino"
)
