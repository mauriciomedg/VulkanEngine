add_library(SingularityEngineRuntime SHARED
    include/Singularity/Export.h
    include/Singularity/SingularityEngineRuntime.h
    src/SingularityEngineRuntime.cpp
)

target_compile_definitions(SingularityEngineRuntime
    PRIVATE SINGULARITY_ENGINE_RUNTIME_EXPORTS
)

target_include_directories(SingularityEngineRuntime
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(SingularityEngineRuntime
    PRIVATE GraphicsEngine
)

set_target_properties(SingularityEngineRuntime PROPERTIES
    FOLDER "Engine"
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
)
