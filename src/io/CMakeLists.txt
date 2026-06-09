add_library(cadino_io STATIC
    RhinoIO.cpp
)

target_include_directories(cadino_io
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(cadino_io
    PUBLIC
        cadino_core
    PRIVATE
        opennurbsStatic
        spdlog::spdlog
)

# openNURBS uses non-permissive C++ patterns that fail under our strict flags.
if(MSVC)
    target_compile_options(cadino_io PRIVATE /wd4267 /wd4244 /wd4100)
endif()

set_target_properties(cadino_io PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
