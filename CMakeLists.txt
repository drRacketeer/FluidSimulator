cmake_minimum_required(VERSION 3.14)
project(FluidSimulator)

set(CMAKE_CXX_STANDARD 17)

# Find the required libraries
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)


# Add the executable
add_executable(FluidSimulator src/main.cpp external/glad/src/glad.c)
# add_executable(test_opengl test_opengl.cpp)

target_include_directories(FluidSimulator PRIVATE external/glad/include)

# Link libraries
target_link_libraries(FluidSimulator OpenGL::GL glfw)
