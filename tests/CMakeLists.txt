add_executable(MeshLoaderCheck MeshLoaderCheck.cpp
    "${PROJECT_SOURCE_DIR}/engine/GraphicsEngine/src/MeshLoader.cpp")
target_include_directories(MeshLoaderCheck PRIVATE "${PROJECT_SOURCE_DIR}/engine/GraphicsEngine/src")
target_link_libraries(MeshLoaderCheck PRIVATE MeshFormat)
set_target_properties(MeshLoaderCheck PROPERTIES FOLDER "Tests")

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME AssetPipeline
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/asset_pipeline_test.py"
            $<TARGET_FILE:GLBConverter> $<TARGET_FILE:MeshLoaderCheck>
            "${PROJECT_BINARY_DIR}/asset-tests")
endif()
