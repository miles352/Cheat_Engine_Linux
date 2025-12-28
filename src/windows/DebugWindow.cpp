#include "DebugWindow.hpp"

#include "imgui.h"

DebugWindow::DebugWindow(AppState& state, Debugger::Breakpoint breakpoint): state(state), debugger(Debugger{state.process->pid})
{
    debugger.add_breakpoint(breakpoint);
}

void DebugWindow::draw()
{
    ImGui::Begin("Debug Window", &open);

    if (!open) state.send_event(CloseWindowEvent{WindowID::DEBUG});



    ImGui::End();
}
