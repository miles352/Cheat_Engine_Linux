#pragma once

#include "AppState.hpp"
#include "Debugger.hpp"
#include "Window.hpp"

class DebugWindow : public Window
{
    void draw() override;
    AppState& state;
    bool open = true;

    Debugger debugger;
public:
    explicit DebugWindow(AppState& state, Debugger::Breakpoint breakpoint); // process shouldnt be nullopt here
};