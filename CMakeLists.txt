cmake_minimum_required(VERSION 3.14)
project(FluidSimulator)

set(CMAKE_CXX_STANDARD 17)

# Find the required libraries
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)


# Add the executable
add_executable(FluidSimulator
                src/main.cpp
                src/Fluid.cpp
                external/glad/src/glad.c)
# add_executable(test_opengl test_opengl.cpp)

target_include_directories(FluidSimulator PRIVATE external/glad/include)

# Link libraries
target_link_libraries(FluidSimulator OpenGL::GL glfw)

add_custom_command(TARGET FluidSimulator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_BINARY_DIR}/src
    COMMENT "Copying shader files to build directory"
)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
