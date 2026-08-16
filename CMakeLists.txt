cmake_minimum_required(VERSION 3.14)
project(FluidSimulator)

set(CMAKE_CXX_STANDARD 17)

# Find the required libraries
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

# Imgui sources
set(IMGUI_SOURCES
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/backends/imgui_impl_glfw.cpp
    external/imgui/backends/imgui_impl_opengl3.cpp
)


# Add the executable
add_executable(FluidSimulator
                src/main.cpp
                src/Fluid.cpp
                external/glad/src/glad.c
                ${IMGUI_SOURCES})

target_include_directories(FluidSimulator PRIVATE
                            external/glad/include
                            external/imgui            # for imgui.h
                            external/imgui/backends   # for imgui_impl_*.h
)

# Link libraries
target_link_libraries(FluidSimulator OpenGL::GL glfw)

add_custom_command(TARGET FluidSimulator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_BINARY_DIR}/src
    COMMENT "Copying shader files to build directory"
)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
