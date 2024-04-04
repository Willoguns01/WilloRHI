cmake_minimum_required(VERSION 3.9)

project(WilloRHI)
set(CMAKE_CXX_STANDARD 20)

file(GLOB_RECURSE RHI_SOURCE src/*.cpp)
file(GLOB_RECURSE RHI_INCLUDE include/WilloRHI/*.hpp)

add_library(WilloRHI ${RHI_SOURCE} ${RHI_INCLUDE})

target_include_directories(WilloRHI PUBLIC include)

find_package(Vulkan REQUIRED)
target_include_directories(WilloRHI PUBLIC ${Vulkan_INCLUDE_DIRS} vendor/)

include(FetchContent)
add_subdirectory(external)

target_link_libraries(WilloRHI PRIVATE 
    ${Vulkan_LIBRARIES} 
    vk-bootstrap::vk-bootstrap 
    concurrentqueue
    )

if(UNIX AND NOT APPLE)
    find_package(X11)
    if(X11_FOUND)
        target_compile_definitions(WilloRHI PUBLIC WilloRHI_BUILD_X11)
        target_link_libraries(WilloRHI PRIVATE ${X11_LIBRARIES})
    endif()
    
    find_package(Wayland)
    if(Wayland_FOUND)
        target_compile_definitions(WilloRHI PUBLIC WilloRHI_BUILD_WAYLAND)
        target_link_libraries(WilloRHI PRIVATE ${Wayland_LIBRARIES})
    endif()
endif()

option(WilloRHI_BUILD_EXAMPLES "Build examples" TRUE)
if (${WilloRHI_BUILD_EXAMPLES})
    add_subdirectory(examples)
endif()
