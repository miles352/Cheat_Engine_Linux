#pragma once

#include <cstdint>
#include <ctime>
#include <vector>

class Debugger
{
public:

    enum BreakpointMode
    {
        EXECUTION_ONLY,
        WRITE_ONLY,
        IO_READWRITE,
        READWRITE
    };

    /** The distance forwards from the breakpoint address which will trigger the breakpoint */
    enum BreakpointRange
    {
        BYTE,
        WORD, // 2 bytes
        QWORD, // 8 bytes
        DWORD // 4 bytes
    };

    struct Breakpoint
    {
        uintptr_t addr;
        BreakpointMode mode;
        BreakpointRange range;
        bool enabled;
    };

    explicit Debugger(pid_t pid);

    void add_breakpoint(uintptr_t addr, BreakpointMode mode, BreakpointRange range);

    void draw();

private:
    pid_t pid;
    std::vector<pid_t> tids;
    std::vector<Breakpoint> breakpoints;
};
