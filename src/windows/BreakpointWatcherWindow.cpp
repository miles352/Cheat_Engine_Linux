#include "BreakpointWatcherWindow.hpp"

#include "Debugger.hpp"
#include "imgui.h"

BreakpointWatcherWindow::BreakpointWatcherWindow(AppState& state, const Debugger::Breakpoint& breakpoint): state(state), debugger(Debugger{state.process->pid})
{
    debugger.send_command(Debugger::SetBreakpointCommand{breakpoint, [this](const user_regs_struct& regs) { handle_breakpoint(regs); }});
}

void BreakpointWatcherWindow::draw()
{
    ImGui::Begin("Debug Window", &open);

    if (!open) state.send_event(CloseWindowEvent{WindowID::DEBUG});

    if (ImGui::BeginTable("breakpoint_table", 2))
    {
        ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
        ImGui::TableSetupColumn("Address");
        ImGui::TableSetupColumn("Instruction");
        ImGui::TableHeadersRow();

        for (const auto& [addr, breakpoint] : breakpoint_hits)
        {
            ImGui::PushID(static_cast<int>(addr));
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            if (ImGui::Selectable(std::format("0x{:x}", addr).c_str(), false, ImGuiSelectableFlags_SpanAllColumns))
            {
                // display more information
                // - More lines of asm
                // - Register states
            }

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(breakpoint.asm_preview.c_str());

            ImGui::PopID();
        }

        ImGui::EndTable();
    }


    ImGui::End();
}

void BreakpointWatcherWindow::handle_breakpoint(user_regs_struct regs)
{
    breakpoint_hits.emplace(regs.rip, BreakpointHit{"test", regs, 1});
}

// on breakpoint want:
// - read registers, instruction pointer, etc
// - read memory nearby
