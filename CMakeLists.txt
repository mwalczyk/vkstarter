cmake_minimum_required (VERSION 2.6)
project (VkStarter)

# enable C++11
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-std=c++14)
set_directory_properties(PROPERTIES COMPILE_DEFINITIONS_DEBUG "_DEBUG")

# include source files
include_directories("${PROJECT_SOURCE_DIR}/vkstarter")
file(GLOB SOURCES "vkstarter/*.cpp")

# setup GLFW
add_subdirectory("${PROJECT_SOURCE_DIR}/third_party/glfw")
include_directories("${PROJECT_SOURCE_DIR}/third_party/glfw/include/GLFW")

# create the executable
add_executable(VkStarter ${SOURCES})

# add libraries (note that as of version 3.7, CMake supports Vulkan out-of-the-box)
find_package(Vulkan REQUIRED)
target_link_libraries(VkStarter Vulkan::Vulkan)
target_link_libraries(VkStarter glfw ${GLFW_LIBRARIES})
