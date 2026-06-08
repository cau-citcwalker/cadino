add_library(cadino_ui STATIC
    PlanView.cpp
    SelectTool.cpp
    WallTool.cpp
    BoxTool.cpp
    Viewport3D.cpp
    PropertiesPanel.cpp
)

target_include_directories(cadino_ui
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(cadino_ui
    PUBLIC
        cadino_core
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::OpenGL
        Qt6::OpenGLWidgets
    PRIVATE
        spdlog::spdlog
)

set_target_properties(cadino_ui PROPERTIES
    AUTOMOC ON
    POSITION_INDEPENDENT_CODE ON
)
