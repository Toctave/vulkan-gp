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
  src/render.cpp
  )

target_sources(${PROJECT_NAME} PRIVATE
  src/vulkan/gpu.cpp
  src/vulkan/graphics.cpp
  src/vulkan/internal.cpp
  src/vulkan/compute.cpp
  src/vulkan/render.cpp
  )

target_sources(${PROJECT_NAME} PRIVATE
  src/x11/wm.cpp
  )
  
# GLM
target_include_directories(${PROJECT_NAME} PRIVATE
  extern/include/)

# Shaders
add_custom_target(create-shader-dir ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory shaders)

set_property(TARGET ${PROJECT_NAME}
  PROPERTY ADDITIONAL_CLEAN_FILES shaders)


set(SHADERS
  shaders/base.vert
  shaders/phong.frag
  shaders/flat.frag  
  shaders/wiggle.comp
  )

foreach(SHADER ${SHADERS})
  add_custom_command(OUTPUT ${SHADER}.spv
    COMMAND glslangValidator -V src/${SHADER}.glsl -o ${CMAKE_CURRENT_BINARY_DIR}/${SHADER}.spv
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS create-shader-dir src/${SHADER}.glsl)
  target_sources(${PROJECT_NAME} PRIVATE
    ${SHADER}.spv
    )
endforeach()
