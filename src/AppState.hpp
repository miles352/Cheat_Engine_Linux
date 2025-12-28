#pragma once

#include <cassert>
#include <optional>
#include <queue>

#include "Process.hpp"
#include "events/CloseWindowEvent.hpp"
#include "events/OpenDebugWindowEvent.hpp"
#include "events/SetProcessEvent.hpp"

using AppEvent = std::variant<OpenDebugWindowEvent, SetProcessEvent, CloseWindowEvent>;

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
