add_subdirectory(external)

add_custom_target(CopyShadersRelease ALL COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders ${CMAKE_CURRENT_BINARY_DIR}/Release/resources/shaders)
add_custom_target(CopyShadersDebug ALL COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders ${CMAKE_CURRENT_BINARY_DIR}/Debug/resources/shaders)

add_executable(01_Initialization "01_Initialization.cpp")

target_link_libraries(01_Initialization PRIVATE 
WilloRHI ${Vulkan_LIBRARIES} vk-bootstrap::vk-bootstrap glfw)
