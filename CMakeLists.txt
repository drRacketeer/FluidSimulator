cmake_minimum_required(VERSION 3.14) # A good baseline version
project(MyCppProject VERSION 1.0.0) # You can change the project name here

# --- This is the most important line for LazyVim ---
# It tells CMake to generate the compile_commands.json file
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# --------------------------------------------------

# Use the current directory as the source root
set(SOURCE_FILES src/main.cpp)

# Create the executable target
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

# Set C++ standard to at least C++17
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
