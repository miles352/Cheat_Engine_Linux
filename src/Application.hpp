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

#include "MemUtils.hpp"
#include "Scanner.hpp"


class Application
{

    // Window stuff
    SDL_GLContext gl_context;
    SDL_Window* window;

    pid_t pid = 21175; // TODO: Move pid and other process stuff into class


    Scanner scanner;





public:
    static std::vector<std::vector<MemUtils::AddressMapping>> get_mappings(pid_t pid, bool include_libs = true); // TODO: Move into process information class

    bool done = false;

    Application();
    ~Application();

    void draw_frame();
};


