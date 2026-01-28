#pragma once

#include <optional>
#include <queue>

#include "Process.hpp"
#include "ProcessCache.hpp"
#include "events/CloseWindowEvent.hpp"
#include "events/OpenBPWatcherWindowEvent.hpp"
#include "events/OpenMemoryViewerWindowEvent.hpp"
#include "events/SetProcessEvent.hpp"

using AppEvent =   std::variant<OpenBPWatcherWindowEvent,
                                SetProcessEvent,
                                CloseWindowEvent,
                                OpenMemoryViewerWindowEvent>;

struct AppState
{
    std::optional<Process> process;
    std::unique_ptr<ProcessCache> process_cache;

    AppState() : process(std::nullopt), process_cache(nullptr) {};
    AppState(Process process, std::unique_ptr<ProcessCache> process_cache) : process(process), process_cache(std::move(process_cache)) {};

    void send_event(AppEvent event)
    {
        events.emplace(std::move(event));
    }

private:
    friend class Application;
    std::queue<AppEvent> events;
};
