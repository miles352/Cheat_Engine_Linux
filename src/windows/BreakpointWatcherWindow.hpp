#pragma once

#include <map>

#include "AppState.hpp"
#include "Debugger.hpp"
#include "Window.hpp"

class BreakpointWatcherWindow : public Window
{
    AppState& state;
    bool open = true;

    Debugger debugger;

    void draw() override;
    void handle_breakpoint(user_regs_struct regs, pid_t tid);

    Debugger::Breakpoint watched_breakpoint;
    std::string window_id;

    static constexpr size_t DISASM_PREVIEW_LEN = 20;

    struct BreakpointHit
    {
        /** The instruction address that comes before regs.rip
         * paired with a map of address to instruction string
         * This is nullopt while it is loading.
         */
        std::optional<std::pair<uintptr_t, std::map<uintptr_t, std::string>>> disasm_preview;
        /** The saved registers after execution of the instruction.
         * If this address is hit multiple times then this is the most recent state of the registers */
        user_regs_struct regs;
        /** The amount of times this address was hit */
        int count;
    };

    /** The breakpoint handler function gets ran on the debug thread so we need a mutex. */
    std::mutex handler_mutex;
    /** Map of address that triggered breakpoint to breakpoint struct */
    std::unordered_map<uintptr_t, BreakpointHit> breakpoint_hits;
    std::optional<uintptr_t> selected_hit;

    /** Worker threads that read the executable memory to find the previous instructions. */
    std::vector<std::jthread> disasm_preview_threads;

    void get_disasm_preview(pid_t pid, uintptr_t rip);

public:
    explicit BreakpointWatcherWindow(AppState& state, const Debugger::Breakpoint& breakpoint); // process shouldnt be nullopt here
};