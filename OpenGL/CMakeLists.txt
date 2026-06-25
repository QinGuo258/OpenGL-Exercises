add_executable (OpenGL
    "../src/main.cpp"
    "../src/Camera.cpp"
    "../src/FPSCamera.cpp"
    "../src/Shader.cpp"
    "../src/Texture.cpp"
    "../src/Mesh.cpp"
    "../src/Model.cpp"
    "../src/Player.cpp"
    "../src/ThirdPersonCamera.cpp"
    "../src/Animation.cpp"
    "../src/CollisionWorld.cpp"
    "../src/Enemy.cpp"
    "../src/Audio.cpp"
    "../src/Arrow.cpp"
    "../src/FontRenderer.cpp"
    "../src/font_baker.c"
)

set_source_files_properties("../src/font_baker.c" PROPERTIES LANGUAGE C)

target_link_libraries(OpenGL PRIVATE
    glfw
    glad
    glm
    assimp
    stb_image
)

set_property(TARGET OpenGL PROPERTY CXX_STANDARD 20)
target_compile_options(OpenGL PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

# 传入源码 shader 目录的绝对路径，供热重载时从源码目录读取
target_compile_definitions(OpenGL PRIVATE SHADER_SRC_DIR="${CMAKE_SOURCE_DIR}/shaders/")

# 将 shader 和 model 文件复制到可执行文件所在目录
add_custom_command(TARGET OpenGL POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/../shaders"
    "$<TARGET_FILE_DIR:OpenGL>/shaders"
)
add_custom_command(TARGET OpenGL POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/../models"
    "$<TARGET_FILE_DIR:OpenGL>/models"
)
add_custom_command(TARGET OpenGL POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/../textures"
    "$<TARGET_FILE_DIR:OpenGL>/textures"
)
add_custom_command(TARGET OpenGL POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/../audio"
    "$<TARGET_FILE_DIR:OpenGL>/audio"
)
add_custom_command(TARGET OpenGL POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/../fonts"
    "$<TARGET_FILE_DIR:OpenGL>/fonts"
)
