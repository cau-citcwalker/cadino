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

set_target_properties(cadino PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
    OUTPUT_NAME "Cadino"
)
