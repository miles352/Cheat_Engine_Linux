#pragma once

#include <cassert>
#include <optional>
#include <queue>

#include "Process.hpp"
#include "events/OpenDebugWindowEvent.hpp"
#include "events/SetProcess.hpp"

using AppEvent = std::variant<OpenDebugWindowEvent, SetProcess>;

struct AppState
{
    enum WindowID
    {
        SCANNER,
        DEBUG,
        WINDOW_LENGTH
    };

    std::optional<Process> process;



    void send_event(AppEvent event)
    {
        events.emplace(std::move(event));
    }

    // void set_process(std::optional<Process> process)
    // {
    //     this->process = std::move(process);
    //     process_updated = true;
    // }

    // TODO: std::vector<Event> which gets handled in Application.cpp
    // SetProcessEvent
    // All events handled in Application
    // events just define data

    // template <typename T, typename... Args>
    // void open_window(WindowID id, Args&&... args)
    // {
    //     assert(id >= 0 && id < WindowID::WINDOW_LENGTH);
    //     windows[id] = std::make_unique<T>(std::forward<Args>(args)...);
    // }

private:
    friend class Application;
    bool process_updated;
    std::queue<AppEvent> events;
    //
};
