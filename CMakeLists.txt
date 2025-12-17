cmake_minimum_required(VERSION 4.0.0)
project(Cheat_Engine_Linux)

set(CMAKE_CXX_STANDARD 26)

add_subdirectory(dependencies/imgui)

add_executable(${CMAKE_PROJECT_NAME}
        src/main.cpp
        src/Application.cpp
        src/Application.hpp
        src/Scanner.cpp
        src/Scanner.hpp
        src/MemUtils.hpp
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE imgui)
