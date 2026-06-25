cmake_minimum_required (VERSION 3.16)

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("OpenGL")


# 第三方库 — GLAD
add_library(glad STATIC "external/glad/src/glad.c")
target_include_directories(glad PUBLIC "external/glad/include")

# 第三方库 — GLM (纯头文件)
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE "external")

# 第三方库 — GLFW (从源码编译)
option(GLFW_BUILD_DOCS OFF)
option(GLFW_BUILD_EXAMPLES OFF)
option(GLFW_BUILD_TESTS OFF)
add_subdirectory("external/glfw")

# Assimp — 3D 模型加载库
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT ON CACHE BOOL "" FORCE)
add_subdirectory("external/assimp")

# stb_image — 单头文件图像加载库
add_subdirectory("external/stb")

add_subdirectory("OpenGL")
