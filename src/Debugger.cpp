#include "Debugger.hpp"

#include <cassert>
#include <filesystem>
#include <mutex>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <print>

void handler(int)
{
}

Debugger::Debugger(pid_t pid) : pid(pid), debug_thread_running(true), debug_init_ready(false)
{

    debug_thread = std::thread{&Debugger::debug_thread_fn, this};
    std::unique_lock lock{debug_mutex};
    debug_init.wait(lock, [this] { return debug_init_ready; });
    // Wait until signal handler is added until returning from constructor
}

Debugger::~Debugger()
{
    debug_thread_running = false;
    pthread_kill(debug_thread.native_handle(), SIGUSR1);
    std::println("Thread killed");
    debug_thread.join();
    std::println("Thread joined");
}

std::optional<Debugger::Breakpoint> Debugger::get_breakpoint(size_t index)
{
    std::unique_lock lock{debug_mutex};
    return breakpoints.at(index);
}

bool Debugger::process_valid() const
{
    return pid != -1;
}

void Debugger::send_command(DebugCommand&& command)
{
    command.visit([](auto x)
    {
        assert(x.index <= 3 && "Must be valid breakpoint index 0-3");
    });
    std::unique_lock lock{debug_mutex};
    commands.emplace(std::move(command));
    lock.unlock();
    // Wake the debug thread up if it is blocked on the waitpid call
    pthread_kill(debug_thread.native_handle(), SIGUSR1);
}

void Debugger::debug_thread_fn()
{
    {
        std::lock_guard lock{debug_mutex};
        for (auto& dir : std::filesystem::directory_iterator{std::format("/proc/{}/task", this->pid.load())})
        {
            // Store thread ids for later
            pid_t tid = std::stoi(dir.path().filename());
            this->tids.emplace(tid);

            // Seize all the threads
            if (ptrace(PTRACE_SEIZE, tid, 0, PTRACE_O_TRACECLONE) == -1)
            {
                perror("SEIZING ERROR");
            }
        }
        std::println("Seized!\n");


        struct sigaction action{};
        action.sa_handler = &handler;
        sigemptyset(&action.sa_mask); // unblock all signals
        action.sa_flags = SA_INTERRUPT; // interrupt syscalls instead of restarting them
        sigaction(SIGUSR1, &action, nullptr);

        debug_init_ready = true;
    }
    debug_init.notify_one();

    while (debug_thread_running)
    {
        handle_commands();

        if (handle_events()) break;
    }
}

void Debugger::handle_commands()
{
    std::unique_lock lock{debug_mutex};
    while (!commands.empty())
    {
        auto& command = commands.front();
        for (pid_t tid : tids)
        {
            ptrace(PTRACE_INTERRUPT, tid, 0, 0);
            int status;
            waitpid(tid, &status, 0);
            assert(WSTOPSIG(status) == SIGTRAP); // temporary
            command.visit([tid, this]<typename T>(const T& c)
            {
                if constexpr (std::is_same_v<T, SetBreakpointCommand>)
                {
                    long dr7 = ptrace(PTRACE_PEEKUSER, tid, offsetof(user, u_debugreg[7]), 0);

                    const unsigned int enabled_shift = 1u << c.index * 2;
                    if (c.breakpoint.enabled) dr7 |= enabled_shift;    // set the local enable bit
                    else dr7 &= ~enabled_shift;                        // clear the local enable bit

                    const unsigned int mode_shift = 16u + c.index * 4;
                    dr7 &= ~(0b11 << mode_shift);               // clear the mode bits
                    dr7 |= (c.breakpoint.mode << mode_shift);   // set the mode bits to the enum value

                    const unsigned int len_shift = 18u + c.index * 4;
                    dr7 &= ~(0b11 << len_shift);                // clear the len bits
                    dr7 |= (c.breakpoint.range << len_shift);   // set the len bits to the enum value

                    ptrace(PTRACE_POKEUSER, tid, offsetof(user, u_debugreg[7]), dr7);
                    ptrace(PTRACE_POKEUSER, tid, offsetof(user, u_debugreg[c.index]), c.breakpoint.addr);

                    breakpoints[c.index] = c.breakpoint;
                    callbacks[c.index] = std::move(c.callback);
                }
                else if constexpr (std::is_same_v<T, RemoveBreakpointCommand>)
                {
                    long dr7 = ptrace(PTRACE_PEEKUSER, tid, offsetof(user, u_debugreg[7]), 0);
                    const unsigned int enabled_shift = 1u << c.index * 2;
                    dr7 &= ~enabled_shift;                      // disable the breakpoint
                    // other fields (address, mode, length) are left as they are because there is no unset value
                    ptrace(PTRACE_POKEUSER, tid, offsetof(user, u_debugreg[7]), dr7);

                    breakpoints[c.index] = std::nullopt;
                    callbacks[c.index] = std::nullopt;
                }
                else if constexpr (std::is_same_v<T, DisableBreakpointCommand>)
                {
                    long dr7 = ptrace(PTRACE_PEEKUSER, tid, offsetof(user, u_debugreg[7]), 0);
                    const unsigned int enabled_shift = 1u << c.index * 2;
                    dr7 &= ~enabled_shift;                      // disable the breakpoint
                    ptrace(PTRACE_POKEUSER, tid, offsetof(user, u_debugreg[7]), dr7);

                    if (breakpoints[c.index].has_value()) breakpoints[c.index]->enabled = false;
                }
                else if constexpr (std::is_same_v<T, EnableBreakpointCommand>)
                {
                    long dr7 = ptrace(PTRACE_PEEKUSER, tid, offsetof(user, u_debugreg[7]), 0);
                    const unsigned int enabled_shift = 1u << c.index * 2;
                    dr7 |= enabled_shift;                      // enable the breakpoint
                    ptrace(PTRACE_POKEUSER, tid, offsetof(user, u_debugreg[7]), dr7);

                    if (breakpoints[c.index].has_value()) breakpoints[c.index]->enabled = true;
                }
                else if constexpr (std::is_same_v<T, ChangeBreakpointModeCommand>)
                {
                    long dr7 = ptrace(PTRACE_PEEKUSER, tid, offsetof(user, u_debugreg[7]), 0);
                    const unsigned int mode_shift = 16u + c.index * 4;
                    dr7 &= ~(0b11 << mode_shift);               // clear the mode bits
                    dr7 |= (c.new_mode << mode_shift);          // set the mode bits to the enum value
                    ptrace(PTRACE_POKEUSER, tid, offsetof(user, u_debugreg[7]), dr7);

                    if (breakpoints[c.index].has_value()) breakpoints[c.index]->mode = c.new_mode;
                }
                else if constexpr (std::is_same_v<T, ChangeBreakpointRangeCommand>)
                {
                    long dr7 = ptrace(PTRACE_PEEKUSER, tid, offsetof(user, u_debugreg[7]), 0);
                    const unsigned int len_shift = 18u + c.index * 4;
                    dr7 &= ~(0b11 << len_shift);                // clear the len bits
                    dr7 |= (c.new_range << len_shift);   // set the len bits to the enum value
                    ptrace(PTRACE_POKEUSER, tid, offsetof(user, u_debugreg[7]), dr7);

                    if (breakpoints[c.index].has_value()) breakpoints[c.index]->range = c.new_range;
                }
            });
            ptrace(PTRACE_CONT, tid, 0, 0);
        }
        commands.pop();
    }
}

bool Debugger::handle_events()
{
    int status{};
    pid_t pid = waitpid(-1, &status, __WALL);
    if (pid == -1)
    {
        if (errno == EINTR)
        {
            return false;
        }
        else
        {
            perror("Waitpid");
            return true;
        }
    }
    if (WIFSIGNALED(status) || WIFEXITED(status))
    {
        std::println("process ended");
        if (pid == this->pid) // if the main pid is signaled
        {
            // TODO: Stop debugging
            pid = -1;
            return true;
        }
        this->tids.erase(pid);
    }
    if (WIFCONTINUED(status)) std::println("Continued");
    if (WIFSTOPPED(status))
    {
        if (status>>8 == (SIGTRAP | (PTRACE_EVENT_CLONE<<8))) // clone() called
        {
            pid_t child_tid; // pid returned by waitpid is the parents pid
            if (ptrace(PTRACE_GETEVENTMSG, pid, 0, &child_tid) == -1) perror("PTRACE_GETEVENTMSG Error");
            // TODO: Add breakpoints
            if (ptrace(PTRACE_CONT, child_tid, 0, 0) == -1) perror("PTRACE_CONT Error");
            if (ptrace(PTRACE_CONT, pid, 0, 0) == -1) perror("PTRACE_CONT Error");
            std::println("New Thread created: {}", child_tid);
        }
        else if (WSTOPSIG(status) == SIGTRAP)
        {
            if (ptrace(PTRACE_POKEUSER, pid, offsetof(user, u_debugreg[6]), 0) == -1) perror("Pokeuser");

            user_regs_struct regs;
            long stat = ptrace(PTRACE_GETREGS, pid, 0, &regs);
            std::println("Breakpoint hit! RIP: 0x{:x} RDX: 0x{:x} RCX: 0x{:x} RBX: 0x{:x} RAX: 0x{:x} RSI: 0x{:x} RSP: 0x{:x}", regs.rip, regs.rdx, regs.rcx, regs.rbx, regs.rax, regs.rsi, regs.rsp);
            if (ptrace(PTRACE_CONT, pid, 0, 0) == -1) perror("PTRACE_CONT Error");
        }
        else
        {
            std::println("Stopped: {}", WSTOPSIG(status));
            if (ptrace(PTRACE_CONT, pid, 0, WSTOPSIG(status)) == -1) perror("PTRACE_CONT Error");
        }
    }
    return false;
}
