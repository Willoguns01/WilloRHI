add_subdirectory(external)

add_executable(01_Initialization "01_Initialization.cpp")

target_link_libraries(01_Initialization PRIVATE 
WilloRHI ${Vulkan_LIBRARIES} vk-bootstrap::vk-bootstrap glfw)
