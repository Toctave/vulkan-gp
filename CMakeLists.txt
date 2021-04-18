cmake_minimum_required(VERSION 3.10)
project(vulkan-gp)

add_executable(${PROJECT_NAME})

find_package(Vulkan REQUIRED)
find_package(X11 REQUIRED)

target_link_libraries(${PROJECT_NAME} ${X11_LIBRARIES})
target_compile_definitions(${PROJECT_NAME} PRIVATE -DVK_USE_PLATFORM_XLIB_KHR)

target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)

target_sources(vulkan-gp PRIVATE
  src/main.cpp)
  
