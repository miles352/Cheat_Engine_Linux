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
    std::optional<Process> process;


    void send_event(AppEvent event)
    {
        events.emplace(std::move(event));
    }

private:
    friend class Application;
    std::queue<AppEvent> events;
};
