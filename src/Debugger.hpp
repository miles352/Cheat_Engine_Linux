#pragma once

#include <condition_variable>
#include <cstdint>
#include <ctime>
#include <functional>
#include <queue>
#include <thread>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>
#include <sys/user.h>

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

    struct SetBreakpointCommand
    {
        Breakpoint breakpoint;
        std::function<void(user_regs_struct)> callback;
        size_t index;
        SetBreakpointCommand(const Breakpoint& breakpoint, std::function<void(user_regs_struct)> callback, size_t index = 0) : breakpoint(breakpoint), callback(std::move(callback)), index(index) {};
    };

    struct RemoveBreakpointCommand
    {
        size_t index = 0;
    };

    struct DisableBreakpointCommand
    {
        size_t index = 0;
    };

    struct EnableBreakpointCommand
    {
        size_t index = 0;
    };

    struct ChangeBreakpointModeCommand
    {
        BreakpointMode new_mode;
        size_t index;
        ChangeBreakpointModeCommand(BreakpointMode new_mode, size_t index = 0) : new_mode(new_mode), index(index) {};
    };

    struct ChangeBreakpointRangeCommand
    {
        BreakpointRange new_range;
        size_t index;
        ChangeBreakpointRangeCommand(BreakpointRange new_range, size_t index = 0) : new_range(new_range), index(index) {};
    };

    using DebugCommand =   std::variant<SetBreakpointCommand,
                                        RemoveBreakpointCommand,
                                        EnableBreakpointCommand,
                                        ChangeBreakpointModeCommand,
                                        ChangeBreakpointRangeCommand>;

    explicit Debugger(pid_t pid);
    ~Debugger();

    std::optional<Breakpoint> get_breakpoint(size_t index = 0);
    void send_command(DebugCommand&& command);

    /** Returns whether the process is still valid */
    bool process_valid() const;
private:
    std::atomic<pid_t> pid;
    /** Only used on debug thread */
    std::unordered_set<pid_t> tids;
    // TODO: This is specific to x86-64 intel cpus, unknown if it works for others
    // Each index corresponds to a debug register: dr0, dr1, dr2, dr3
    std::array<std::optional<Breakpoint>, 4> breakpoints;
    std::array<std::function<void()>, 4> callbacks;

    std::thread debug_thread;
    std::atomic_bool debug_thread_running;

    std::mutex debug_mutex;
    /** Becomes true when the debug thread is done being set up, so that the constructor can end.
     * Mainly so that the signal handler can be added so that if send_command is called shortly after it doesn't
     * cause a signal without a handler which would end the program.
     */
    std::condition_variable debug_init;
    bool debug_init_ready;
    /* A list of commands that must be run on the debug thread because they involve ptrace. */
    std::queue<DebugCommand> commands;

    void debug_thread_fn();
    /** Handles all the commands in the command queue*/
    void handle_commands();
    /** Handles the process state changes using waitpid.
     * @Returns true if the debug thread should end */
    bool handle_events();
};
