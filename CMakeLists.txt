cmake_minimum_required(VERSION 4.0.0)
project(Cheat_Engine_Linux)

set(CMAKE_CXX_STANDARD 26)

add_subdirectory(dependencies/)
add_subdirectory(dependencies/capstone)

add_executable(${CMAKE_PROJECT_NAME}
        src/main.cpp
        src/Application.cpp
        src/Application.hpp
        src/MemUtils.hpp
        src/Debugger.cpp
        src/Debugger.hpp
        src/Process.hpp
        src/windows/Window.hpp
        src/windows/ScanWindow.cpp
        src/windows/ScanWindow.hpp
        src/AppState.hpp
        src/events/OpenBPWatcherWindowEvent.hpp
        src/events/SetProcessEvent.hpp
        src/windows/BreakpointWatcherWindow.cpp
        src/windows/BreakpointWatcherWindow.hpp
        src/events/CloseWindowEvent.hpp
        src/windows/MemoryViewerWindow.cpp
        src/windows/MemoryViewerWindow.hpp
        src/ScanType.hpp
        src/events/OpenMemoryViewerWindowEvent.hpp
        src/ProcessCache.hpp
        #        src/test1.cpp
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE src)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE imgui)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE capstone)
