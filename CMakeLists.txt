cmake_minimum_required(VERSION 3.10)
project(vulkan-gp)

add_executable(${PROJECT_NAME})

find_package(Vulkan REQUIRED)
find_package(X11 REQUIRED)

target_link_libraries(${PROJECT_NAME} ${X11_LIBRARIES})
target_compile_definitions(${PROJECT_NAME} PRIVATE -DVK_USE_PLATFORM_XLIB_KHR)

target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)

target_sources(${PROJECT_NAME} PRIVATE
  src/main.cpp
  shaders/triangle.vert.spv
  shaders/triangle.frag.spv
  )
  
# GLM
target_include_directories(${PROJECT_NAME} PRIVATE
  extern/include/)

# Shaders
add_custom_target(create-shader-dir ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory shaders)
add_custom_command(OUTPUT shaders/triangle.vert.spv
  COMMAND glslangValidator -V src/shaders/triangle.vert.glsl -o ${CMAKE_CURRENT_BINARY_DIR}/shaders/triangle.vert.spv
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  DEPENDS create-shader-dir src/shaders/triangle.vert.glsl)
add_custom_command(OUTPUT shaders/triangle.frag.spv
  COMMAND glslangValidator -V src/shaders/triangle.frag.glsl -o ${CMAKE_CURRENT_BINARY_DIR}/shaders/triangle.frag.spv
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  DEPENDS create-shader-dir src/shaders/triangle.frag.glsl)

