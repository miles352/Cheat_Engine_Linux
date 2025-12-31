#include "BreakpointWatcherWindow.hpp"

#include "Debugger.hpp"
#include "imgui.h"
#include "MemUtils.hpp"
#include "capstone/capstone.h"

BreakpointWatcherWindow::BreakpointWatcherWindow(AppState& state, const Debugger::Breakpoint& breakpoint): state(state), debugger(Debugger{state.process->pid})
{
    debugger.send_command(Debugger::SetBreakpointCommand{breakpoint, [this](const user_regs_struct& regs, pid_t tid) { handle_breakpoint(regs, tid); }});
}

void BreakpointWatcherWindow::draw()
{
    ImGui::Begin("Debug Window", &open);

    if (!open) state.send_event(CloseWindowEvent{WindowID::DEBUG});

    if (ImGui::BeginTable("breakpoint_table", 3))
    {
        ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
        ImGui::TableSetupColumn("Count");
        ImGui::TableSetupColumn("Address");
        ImGui::TableSetupColumn("Instruction");
        ImGui::TableHeadersRow();

        std::lock_guard lock{handler_mutex};
        for (const auto& [addr, breakpoint] : breakpoint_hits)
        {
            ImGui::PushID(static_cast<int>(addr));
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            if (ImGui::Selectable(std::format("{}", breakpoint.count).c_str(), false, ImGuiSelectableFlags_SpanAllColumns))
            {
                // display more information
                // - More lines of asm
                // - Register states
            }

            ImGui::TableNextColumn();
            ImGui::Text("0x%lx", addr);

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(breakpoint.asm_preview.c_str());

            ImGui::PopID();
        }

        ImGui::EndTable();
    }


    ImGui::End();
}

void BreakpointWatcherWindow::handle_breakpoint(user_regs_struct regs, pid_t tid)
{
    std::lock_guard lock{handler_mutex};
    auto it = breakpoint_hits.find(regs.rip);
    if (it != breakpoint_hits.end())
    {
        it->second.count++;
        it->second.regs = regs;
    }
    else
    {
        // RIP contains the instruction after the one that triggered the breakpoint
        // to get the actual instruction, we check backwards from 1 to 15 bytes, until a valid instruction is decoded
        // only instructions that end at RIP and are equal to the amount left, checking from right to left (meaning no partial instruction thats smaller)
        // if multiple results are left then the instruction that is longest is chosen
        // TODO: This can obviously fail, it would be better to scan forwards instead of backwards and save possibly the sizes of each instruction

        csh handle;
        cs_open(CS_ARCH_X86, CS_MODE_64, &handle);
        // X86 instructions can be up to 15 bytes
        uint8_t bytes[15];
        cs_insn* best_match{};
        for (int i = 1; i < 16; i++)
        {
            bytes[15 - i] = MemUtils::read_addr<uint8_t>(tid, regs.rip - i);

            cs_insn* insn;
            size_t ret = cs_disasm(handle, bytes + (15 - i), i, regs.rip - i, 1, &insn);
            if (ret == 1 && insn->size == i)
            {
                if (!best_match || insn->size > best_match->size)
                {
                    if (best_match) cs_free(best_match, 1);
                    best_match = insn;
                }
            }
            else if (ret > 0)
            {
                cs_free(insn, ret);
            }
        }

        std::string instruction_str = "Failed to Decode";
        if (best_match)
        {
            instruction_str = best_match->mnemonic;
            instruction_str += " ";
            instruction_str += best_match->op_str;
            cs_free(best_match, 1);
        }

        breakpoint_hits.emplace(regs.rip, BreakpointHit{std::move(instruction_str), regs, 1});

        cs_close(&handle);
    }
}

// on breakpoint want:
// - read registers, instruction pointer, etc
// - read memory nearby
