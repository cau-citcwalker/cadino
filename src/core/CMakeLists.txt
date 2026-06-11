add_library(cadino_core STATIC
    entity/Wall.cpp
    entity/NurbsCurve.cpp
    document/Document.cpp
    command/CommandStack.cpp
    command/WallCommands.cpp
    command/BoxCommands.cpp
    command/CylinderCommands.cpp
    command/DoorWindowSlabCommands.cpp
    command/NurbsCurveCommands.cpp
    command/BlockCommands.cpp
)

target_include_directories(cadino_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(cadino_core
    PUBLIC
        Eigen3::Eigen
    PRIVATE
        fmt::fmt
        spdlog::spdlog
)

set_target_properties(cadino_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
