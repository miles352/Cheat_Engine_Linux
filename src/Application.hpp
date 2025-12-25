#pragma once

#include <array>
#include <SDL3/SDL_video.h>

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

    AppState state;

    std::array<std::unique_ptr<Window>, Window::WINDOW_LENGTH> windows = { std::make_unique<ScanWindow>(state) };

    void handle_events();

    void handle_event(OpenDebugWindowEvent data);
    void handle_event(SetProcess data);

public:
    Application();
    ~Application();
    bool done = false;
    void draw_frame();
};


