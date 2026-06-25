add_library(cadino_ui STATIC
    PlanView.cpp
    SelectTool.cpp
    WallTool.cpp
    BoxTool.cpp
    CylinderTool.cpp
    DoorTool.cpp
    SlabTool.cpp
    NurbsCurveTool.cpp
    NurbsSurfaceTool.cpp
    DimensionTool.cpp
    Alignment.cpp
    Array.cpp
    Clipboard.cpp
    Mirror.cpp
    MirrorTool.cpp
    OffsetTool.cpp
    PolarArrayTool.cpp
    IfcExporter.cpp
    Viewport3D.cpp
    PropertiesPanel.cpp
    LayerPanel.cpp
    Snap.cpp
    DocumentIO.cpp
    DxfExporter.cpp
    BooleanOps.cpp
    OcctShapes.cpp
    MeshExport.cpp
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
        TKernel TKMath TKBRep TKG3d TKG2d TKTopAlgo TKGeomAlgo TKGeomBase
        TKPrim TKBO TKMesh TKShHealing TKHLR
)

set_target_properties(cadino_ui PROPERTIES
    AUTOMOC ON
    POSITION_INDEPENDENT_CODE ON
)
