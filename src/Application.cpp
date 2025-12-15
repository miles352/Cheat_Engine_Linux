#include "Application.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <numeric>
#include <print>
#include <stdexcept>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"

Application::Application()
{
    // std::print("Enter PID: ");
    // std::cin >> this->pid;
    // this->map_to_scan = *std::ranges::find_if(mappings, [](const auto& mapping) { return strcmp(mapping.pathname, "[stack]") == 0;});

    // Setup SDL
    // [If using SDL_MAIN_USE_CALLBACKS: all code below until the main loop starts would likely be your SDL_AppInit() function]
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        throw std::runtime_error(std::format("Error: SDL_Init(): %s\n", SDL_GetError()));
    }

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    this->window = SDL_CreateWindow("Cheat_Engine_Linux", static_cast<int>(1280 * main_scale), static_cast<int>(800 * main_scale), window_flags);
    if (window == nullptr)
    {
        throw std::runtime_error(std::format("Error: SDL_CreateWindow(): %s\n", SDL_GetError()));
    }
    this->gl_context = SDL_GL_CreateContext(window);
    if (gl_context == nullptr)
    {
        throw std::runtime_error(std::format("Error: SDL_GL_CreateContext(): %s\n", SDL_GetError()));
    }

    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale;        // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init();
}

Application::~Application()
{
    // Cleanup
    // [If using SDL_MAIN_USE_CALLBACKS: all code below would likely be your SDL_AppQuit() function]
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DestroyContext(this->gl_context);
    SDL_DestroyWindow(this->window);
    SDL_Quit();
}

void Application::draw_frame()
{
    static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
    // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
    // [If using SDL_MAIN_USE_CALLBACKS: call ImGui_ImplSDL3_ProcessEvent() from your SDL_AppEvent() function]
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_QUIT)
            done = true;
        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
            done = true;
    }

    // [If using SDL_MAIN_USE_CALLBACKS: all code below would likely be your SDL_AppIterate() function]
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
    {
        SDL_Delay(10);
        return;
    }

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
    bool b_true = true;
    ImGui::ShowDemoWindow(&b_true);

    ImGuiIO& io = ImGui::GetIO();

    // TODO: Shared state struct:
    // - pid
    // -


    scanner.draw(pid);



    // Rendering
    ImGui::Render();
    glViewport(0, 0, static_cast<int>(io.DisplaySize.x), static_cast<int>(io.DisplaySize.y));
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);
}

std::vector<std::vector<MemUtils::AddressMapping>> Application::get_mappings(pid_t pid, bool include_libs)
{
    auto tp = std::chrono::system_clock::now();
    std::filesystem::path exe_path = std::filesystem::read_symlink(std::format("/proc/{}/exe", pid));

    std::vector<std::vector<MemUtils::AddressMapping>> mappings;

    std::ifstream maps{std::format("/proc/{}/maps", pid)};

    while (true)
    {
        MemUtils::AddressMapping mapping{};
        // parse lines in /proc/pid/maps

        maps >> std::hex >> mapping.start;
        if (maps.eof()) break;
        maps.get(); // move passed dash
        maps >> mapping.end;
        maps.get(); // move passed space
        if (maps.get() == 'r') mapping.permissions |= 0x1;
        if (maps.get() == 'w') mapping.permissions |= 0x2;
        if (maps.get() == 'x') mapping.permissions |= 0x4;

        int p_or_s = maps.get();
        if (p_or_s == 'p') mapping.permissions |= 0x8;
        if (p_or_s == 's') mapping.permissions |= 0x10;

        maps >> mapping.offset;
        maps >> std::dec;

        int dev_maj, dev_min; // Unused
        maps >> dev_maj;
        maps.get();
        maps >> dev_min;

        int inode; // Unused
        maps >> inode;

        std::getline(maps, mapping.pathname);
        // trim leading whitespace
        auto it = std::ranges::find_if_not(mapping.pathname, [](char c) { return std::isspace(c); });
        mapping.pathname.erase(mapping.pathname.begin(), it);

        // Make duplicate pathnames unnamed entries so they dont get displayed in the gui
        // if (std::ranges::find_if(mappings, [&mapping](const AddressMapping& other_mapping) { return other_mapping.pathname == mapping.pathname; }) != mappings.end())
        // {
        //     mapping.pathname.clear();
        // }


        if ((mapping.permissions & 0x1) == 0) continue; // dont include mappings that are unreadable



        if (mapping.pathname == exe_path
            || mapping.pathname.empty() // always include unnamed regions
            || mapping.pathname == "[heap]" || mapping.pathname.contains("[stack") // catch stacks marked with thread ids: [stack:tid]
            || (include_libs && (mapping.pathname.rfind(".so") != std::string::npos || mapping.permissions & 0x10)))
        {
            auto it = std::ranges::find_if(mappings, [&mapping](const std::vector<MemUtils::AddressMapping>& other_mapping) { return other_mapping[0].pathname == mapping.pathname; });
            if (it != mappings.end())
            {
                it->push_back(mapping);
            }
            else
            {
                mappings.emplace_back(1, mapping);
            }
        }

        // include if:
        // - pathname same as pid
        // - unnamed mapping
        // - [heap] || [stack]
        // - libraries enabled && pathname includes .so || mapped as shared
    }
    std::println("Took {}ms", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - tp).count());
    return mappings;
}



