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
// #include "imgui_internal.h"
#include "imgui_internal.h"
#include "windows/BreakpointWatcherWindow.hpp"
#include "windows/ScanWindow.hpp"
#include "windows/Window.hpp"

Application::Application()
{
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
    this->sdl_window = SDL_CreateWindow("Cheat_Engine_Linux", static_cast<int>(1280 * main_scale), static_cast<int>(800 * main_scale), window_flags);
    if (sdl_window == nullptr)
    {
        throw std::runtime_error(std::format("Error: SDL_CreateWindow(): %s\n", SDL_GetError()));
    }
    this->gl_context = SDL_GL_CreateContext(sdl_window);
    if (gl_context == nullptr)
    {
        throw std::runtime_error(std::format("Error: SDL_GL_CreateContext(): %s\n", SDL_GetError()));
    }

    SDL_GL_MakeCurrent(sdl_window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync
    SDL_SetWindowPosition(sdl_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(sdl_window);

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
    ImGui_ImplSDL3_InitForOpenGL(sdl_window, gl_context);
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
    SDL_DestroyWindow(this->sdl_window);
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
        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(sdl_window))
            done = true;
    }

    // [If using SDL_MAIN_USE_CALLBACKS: all code below would likely be your SDL_AppIterate() function]
    if (SDL_GetWindowFlags(sdl_window) & SDL_WINDOW_MINIMIZED)
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

    ImGui::SetNextWindowSize(ImVec2(1000, 1200), ImGuiCond_FirstUseEver);
    ImGui::Begin("Cheat_Engine_Linux", nullptr);


    ImGuiID dockspace_id = ImGui::GetID("Cheat_Engine_Linux");

    ImGui::DockSpace(dockspace_id);
    ImGui::End();

    static bool setup = true;
    if (setup)
    {
        setup = false;
        if (const char* path = ImGui::GetIO().IniFilename)
        {
            if (!std::filesystem::exists(path)) // If there is no saved data
            {
                ImGui::DockBuilderRemoveNode(dockspace_id);
                ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
                ImVec2 size = ImGui::GetMainViewport()->Size;
                ImGui::DockBuilderSetNodeSize(dockspace_id, size);

                ImGuiID dock_main_id = dockspace_id;
                ImGuiID dock_top = // set dock_top to the top portion, dock_main_id becomes the rest of the space
                    ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Up, 0.05f, nullptr, &dock_main_id);

                ImGuiID dock_bottom = // set dock_bottom to the bottom portion, again dock_main_id becomes the rest of the space
                    ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.25f, nullptr, &dock_main_id);

                ImGuiID dock_left =
                    ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.45f, nullptr, &dock_main_id);

                ImGui::DockBuilderDockWindow("Process Information", dock_top);
                ImGui::DockBuilderDockWindow("Scan Results", dock_bottom);
                ImGui::DockBuilderDockWindow("Address Table", dock_left);
                ImGui::DockBuilderDockWindow("Scanner", dock_main_id);
                ImGui::DockBuilderFinish(dockspace_id);
            }
        }
    }



    this->handle_events();

    for (const auto& window : windows)
    {
        if (window) window->draw();
    }




    // Rendering
    ImGui::Render();
    glViewport(0, 0, static_cast<int>(io.DisplaySize.x), static_cast<int>(io.DisplaySize.y));
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(sdl_window);
}




void Application::handle_events()
{
    if (state.events.empty()) return;
    do
    {
        AppEvent event = std::move(state.events.front());
        event.visit([this](auto&& event_data)
        {
            this->handle_event(std::move(event_data));
        });

        state.events.pop();
    }
    while (!state.events.empty());
}

void Application::handle_event(OpenBPWatcherWindowEvent data)
{
    windows[Window::DEBUG] = std::make_unique<BreakpointWatcherWindow>(state, data.breakpoint);
}

void Application::handle_event(SetProcessEvent data)
{
    for (auto& window : windows)
    {
        window = nullptr;
    }
    state.process = std::move(data.new_process);
    windows[0] = std::make_unique<ScanWindow>(state);
}

void Application::handle_event(CloseWindowEvent data)
{
    windows[data.id] = nullptr;
}

