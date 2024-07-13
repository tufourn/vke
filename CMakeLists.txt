cmake_minimum_required(VERSION 3.28)
project(vke)

set(CMAKE_CXX_STANDARD 20)

# Volk
add_subdirectory(external/volk)

# vk-bootstrap
add_subdirectory(external/vk-bootstrap)

# VMA
add_subdirectory(external/VulkanMemoryAllocator)

# GLFW
option(GLFW_BUILD_DOCS OFF)
option(GLFW_BUILD_EXAMPLES OFF)
option(GLFW_BUILD_TESTS OFF)
add_subdirectory(external/glfw)
add_definitions(-DGLFW_INCLUDE_NONE)

# Dear ImGui
file(GLOB IMGUI_SOURCES
        external/imgui/*.cpp
        external/imgui/backends/imgui_impl_glfw.cpp)
add_library(imgui STATIC ${IMGUI_SOURCES})

# Includes
include_directories(include
        external/imgui/
        external/glfw/include
        external/glm/
        external/cgltf/
)

# Sources
file(GLOB_RECURSE SOURCES src/*.cpp)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE
        volk_headers
        vk-bootstrap::vk-bootstrap
        GPUOpen::VulkanMemoryAllocator
        imgui
        glfw)

# Copy assets
file(COPY assets DESTINATION ${CMAKE_BINARY_DIR})

# Compile shaders
file(GLOB_RECURSE GLSL_SOURCES
        "shaders/*.frag"
        "shaders/*.vert"
)

foreach(GLSL ${GLSL_SOURCES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
    add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/"
            COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} -V ${GLSL} -o ${SPIRV}
            DEPENDS ${GLSL}
    )
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
        shaders
        DEPENDS ${SPIRV_BINARY_FILES}
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_BINARY_DIR}/shaders"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"
)

add_dependencies(${PROJECT_NAME} shaders)
