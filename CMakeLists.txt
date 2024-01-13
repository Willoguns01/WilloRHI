cmake_minimum_required(VERSION 3.9)

project(WilloRHI VERSION 0.1)
set(CMAKE_CXX_STANDARD 20)

file(GLOB_RECURSE RHI_SOURCE src/*.cpp)
file(GLOB_RECURSE RHI_INCLUDE include/include/WilloRHI/*.hpp)

add_library(WilloRHI ${RHI_SOURCE} ${RHI_INCLUDE})

target_include_directories(WilloRHI PUBLIC include)

find_package(Vulkan REQUIRED)
target_include_directories(WilloRHI PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(WilloRHI PRIVATE ${Vulkan_LIBRARIES})
