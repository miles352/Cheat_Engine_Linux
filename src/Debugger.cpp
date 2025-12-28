#include "Debugger.hpp"

#include <filesystem>
#include <mutex>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <print>

void handler(int) {};

Debugger::Debugger(pid_t pid) : pid(pid), debug_thread(std::thread{&Debugger::debug_thread_fn, this}), debug_thread_running(true)
{

}

Debugger::~Debugger()
{
    debug_thread_running = false;
    pthread_kill(debug_thread.native_handle(), SIGUSR1);
    std::println("Thread killed");
    debug_thread.join();
    std::println("Thread joined");
}

void Debugger::add_breakpoint(uintptr_t addr, BreakpointMode mode, BreakpointRange range)
{
    std::unique_lock lock{debug_mutex};
    this->commands.emplace(BreakpointCommand{Breakpoint{addr, mode, range, true}});
    lock.unlock();

    // Wake the debug thread up if it is blocked on the waitpid call
    pthread_kill(debug_thread.native_handle(), SIGUSR1);
}

void Debugger::debug_thread_fn()
{
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

    while (debug_thread_running)
    {
        handle_commands();

        if (handle_events()) break;
    }
}

bool Debugger::process_valid() const
{
    return pid != -1;
}

void Debugger::handle_commands()
{
    std::unique_lock lock{debug_mutex};
    while (!commands.empty())
    {
        auto& command = commands.front();
        command.visit([]<typename T>(T& t)
        {
            if constexpr (std::is_same_v<T, BreakpointCommand>)
            {

            }
        });
        commands.pop();
    }
}

bool Debugger::handle_events()
{
    int status{};
    pid_t pid = waitpid(-1, &status, __WALL);
    if (pid == -1)
    {
        std::println("Waitpid error");
        return false;
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
        else
        {
            std::println("Stopped: {}", WSTOPSIG(status));
            if (ptrace(PTRACE_CONT, pid, 0, WSTOPSIG(status)) == -1) perror("PTRACE_CONT Error");
        }
    }
    return false;
}
