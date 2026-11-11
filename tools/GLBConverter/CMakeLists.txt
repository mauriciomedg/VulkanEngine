add_executable(GLBConverter main.cpp MeshWriter.cpp MeshWriter.h)
target_link_libraries(GLBConverter PRIVATE MeshFormat)
set_target_properties(GLBConverter PROPERTIES FOLDER "Tools")
