#pragma once
#include <array>
#include <atomic>
#include <cassert>
#include <functional>
#include <thread>
#include <vector>
#include <sys/uio.h>
#include <SDL3/SDL_video.h>
#include <sys/types.h>
#include <print>
#include <queue>

#include "AppState.hpp"
#include "MemUtils.hpp"
#include "Process.hpp"
#include "windows/ScanWindow.hpp"
#include "windows/Window.hpp"


class Application
{

    // Window stuff
    SDL_GLContext gl_context;
    SDL_Window* sdl_window;

    std::string process_name;

    AppState state;

    std::array<std::unique_ptr<Window>, static_cast<int>(AppState::WINDOW_LENGTH)> windows = { std::make_unique<ScanWindow>(state) };

    // map of window ids to window pts
    // open window function which checks if window exists, then forwards arguments to window constructor


public:
    bool done = false;

    Application();
    ~Application();

    void handle_events();

    void handle_event(OpenDebugWindowEvent data);
    void handle_event(SetProcess data);

    void draw_frame();
};


