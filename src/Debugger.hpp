#pragma once

#include <cstdint>
#include <ctime>
#include <queue>
#include <thread>
#include <unordered_set>
#include <variant>
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

    struct BreakpointCommand
    {
        Breakpoint breakpoint;
    };

    using DebugCommand = std::variant<BreakpointCommand>;

    // TODO:
    // Only keep 4 breakpoints
    // create methods to modify them, enable/disable, change mode, change range, etc
    // all methods use indexes 0-3, default as 0

    explicit Debugger(pid_t pid);
    ~Debugger();

    void add_breakpoint(uintptr_t addr, BreakpointMode mode, BreakpointRange range);
    /** Returns whether the process is still valid */
    bool process_valid() const;
private:
    std::atomic<pid_t> pid;
    /** Only used on debug thread */
    std::unordered_set<pid_t> tids;
    std::vector<Breakpoint> breakpoints;

    std::thread debug_thread;
    std::atomic_bool debug_thread_running;

    std::mutex debug_mutex;
    /* A list of commands that must be run on the debug thread because they involve ptrace. */
    std::queue<DebugCommand> commands;


    void debug_thread_fn();
    /** Handles all the commands in the command queue*/
    void handle_commands();
    /** Handles the process state changes using waitpid.
     * @Returns true if the debug thread should end */
    bool handle_events();
};
