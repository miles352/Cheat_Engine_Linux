#pragma once

#include "AppState.hpp"
#include "Debugger.hpp"
#include "Window.hpp"

class BreakpointWatcherWindow : public Window
{
    AppState& state;
    bool open = true;

    Debugger debugger;

    void draw() override;
    void handle_breakpoint(user_regs_struct regs);

    struct BreakpointHit
    {
        /** The instruction where the breakpoint was triggered */
        std::string asm_preview;
        /** The saved registers after execution of the instruction.
         * If this address is hit multiple times then this is the most recent state of the registers */
        user_regs_struct regs;
        /** The amount of times this address was hit */
        int count;
    };

    /** Map of address that triggered breakpoint to breakpoint struct */
    std::unordered_map<uintptr_t, BreakpointHit> breakpoint_hits;

public:
    explicit BreakpointWatcherWindow(AppState& state, const Debugger::Breakpoint& breakpoint); // process shouldnt be nullopt here
};