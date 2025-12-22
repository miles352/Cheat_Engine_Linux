cmake_minimum_required(VERSION 4.0.0)
project(Cheat_Engine_Linux)

set(CMAKE_CXX_STANDARD 26)

add_subdirectory(dependencies/)
add_subdirectory(dependencies/capstone)

add_executable(${CMAKE_PROJECT_NAME}
        src/main.cpp
        src/Application.cpp
        src/Application.hpp
        src/Scanner.cpp
        src/Scanner.hpp
        src/MemUtils.hpp
        src/Debugger.cpp
        src/Debugger.hpp
        src/ProcessManager.hpp
        src/ProcessManager.cpp
        src/Process.hpp
        #        src/test1.cpp
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE imgui)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE capstone)
