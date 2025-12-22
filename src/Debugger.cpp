#include "Debugger.hpp"

#include <filesystem>

Debugger::Debugger(pid_t pid) : pid(pid)
{
    for (auto& dir : std::filesystem::directory_iterator{std::format("/proc/{}/task", pid)})
    {
        // Store thread ids for later
        pid_t tid = std::stoi(dir.path().filename());
        this->tids.emplace_back(tid);

        // Seize all the threads

    }
}

void Debugger::add_breakpoint(uintptr_t addr, BreakpointMode mode, BreakpointRange range)
{
    this->breakpoints.emplace_back(addr, mode, range, true);
}

void Debugger::draw()
{
    // if (!breakpoints.empty())
    {

    }
}
