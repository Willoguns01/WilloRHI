set(CMAKE_CXX_STANDARD 20)

#TODO: variation to make sure we use the shell scrip on linux
add_custom_target(CompileShaders COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/resources/compile_shaders.bat WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/resources)

add_custom_target(CopyShadersRelease ALL COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders ${CMAKE_CURRENT_BINARY_DIR}/Release/resources/shaders)
add_dependencies(CopyShadersRelease CompileShaders)

add_custom_target(CopyShadersDebug ALL COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders ${CMAKE_CURRENT_BINARY_DIR}/Debug/resources/shaders)
add_dependencies(CopyShadersDebug CompileShaders)

add_subdirectory(external)

add_library(ExampleApplication "ExampleApplication.cpp" "ExampleApplication.hpp")
target_link_libraries(ExampleApplication PRIVATE WilloRHI ${Vulkan_LIBRARIES} vk-bootstrap::vk-bootstrap glfw)
add_dependencies(ExampleApplication CopyShadersRelease CopyShadersDebug)

add_executable(01_HelloTriangle "01_HelloTriangle.cpp")
target_link_libraries(01_HelloTriangle PRIVATE ExampleApplication WilloRHI glfw glm::glm)
