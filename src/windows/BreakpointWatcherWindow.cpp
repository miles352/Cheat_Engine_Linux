#include "BreakpointWatcherWindow.hpp"

#include "Debugger.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "MemUtils.hpp"
#include "capstone/capstone.h"

BreakpointWatcherWindow::BreakpointWatcherWindow(AppState& state, const Debugger::Breakpoint& breakpoint)
    : state(state), debugger(Debugger{state.process->pid}), watched_breakpoint(breakpoint), selected_hit(std::nullopt)
{
    assert(breakpoint.mode == Debugger::READWRITE || breakpoint.mode == Debugger::WRITE_ONLY && "Breakpoint watcher does not support IO or execution breakpoints");
    window_id = std::format("Find out what {} this address##debug_window", breakpoint.mode == Debugger::BreakpointMode::READWRITE ? "accesses" : "writes to");
    debugger.send_command(Debugger::SetBreakpointCommand{breakpoint, [this](const user_regs_struct& regs, pid_t tid) { handle_breakpoint(regs, tid); }});
}

void BreakpointWatcherWindow::draw()
{
    // ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
    ImGui::Begin(window_id.c_str(), &open);

    if (!open || !debugger.process_valid()) state.send_event(CloseWindowEvent{WindowID::DEBUG});


    // thread creation breakpoint adds

    // A child wrapper so it can be resizable
    if (ImGui::BeginChild("breakpoint_table_wrapper", ImVec2{0, 0}, ImGuiChildFlags_ResizeY))
    {
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
                bool selected = selected_hit.has_value() && *selected_hit == addr;
                if (ImGui::Selectable(std::format("{}", breakpoint.count).c_str(), selected, ImGuiSelectableFlags_SpanAllColumns))
                {
                    selected_hit = addr;
                }

                ImGui::TableNextColumn();
                ImGui::Text("0x%lx", addr);

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(breakpoint.disasm_preview.has_value() ? breakpoint.disasm_preview->second.at(breakpoint.disasm_preview->first).c_str() : "Loading...");

                ImGui::PopID();
            }

            ImGui::EndTable();
        }
    }
    ImGui::EndChild();




    if (selected_hit.has_value())
    {
        if (ImGui::BeginChild("debug_window_details", ImVec2(0, 0), ImGuiChildFlags_Borders))
        {
            const auto& disasm_preview = breakpoint_hits[*selected_hit].disasm_preview;
            if (disasm_preview.has_value())
            {
                for (const auto& [addr, insn] : disasm_preview->second)
                {
                    if (addr == disasm_preview->first)
                    {
                        ImGui::TextColored(ImVec4{1.0, 0, 0, 1.0}, "0x%lx %s", addr, insn.c_str());
                    }
                    else
                    {
                        ImGui::Text("0x%lx %s", addr, insn.c_str());
                    }
                }
            }
        }
        ImGui::EndChild();

        // display more information
        // - More lines of asm
        // - Register states
    }


    ImGui::End();
    // ImGui::PopStyleVar();
}

void BreakpointWatcherWindow::get_disasm_preview(pid_t pid, uintptr_t rip)
{
    std::vector<std::vector<MemUtils::AddressMapping>> mappings = MemUtils::get_mappings(pid, false);
    // find mapping that contains rip and is executable
    for (const auto& mapping : mappings)
    {
        for (const auto& map : mapping)
        {
            if (map.start <= rip && map.end > rip && (map.permissions & 0x4))
            {
                // Read the memory of the mapping
                std::vector<uint8_t> memory = MemUtils::read_addrs(pid, map.start, map.end - map.start);
                // dissassemble the mapping
                csh handle;
                cs_open(CS_ARCH_X86, CS_MODE_64, &handle);
                cs_insn* insn;
                size_t count = cs_disasm(handle, memory.data(), memory.size(), map.start, 0, &insn);

                for (int i = 0; i < count; i++)
                {
                    if (insn[i].address >= rip)
                    {
                        // save the next DISASM_PREVIEW_LEN instructions centered around the instruction that triggered the breakpoint
                        // i is subtracted by 1 in its uses because i-1 is the instruction that actually triggered it
                        std::map<uintptr_t, std::string> saved_insns;
                        std::lock_guard lock{handler_mutex};
                        for (int j = 0; j < DISASM_PREVIEW_LEN / 2; j++)
                        {
                            int index = i - 1 - j;
                            if (index < 0) break;
                            saved_insns.emplace(insn[index].address,
                                std::format("{} {}", insn[index].mnemonic, insn[index].op_str));
                        }
                        for (int j = 1; j < DISASM_PREVIEW_LEN / 2 + 1; j++)
                        {
                            int index = i - 1 + j;
                            if (index >= count) break;
                            saved_insns.emplace(insn[index].address,
                                std::format("{} {}", insn[index].mnemonic, insn[index].op_str));
                        }
                        breakpoint_hits[rip].disasm_preview = {insn[i-1].address, std::move(saved_insns)};
                        cs_free(insn, count);
                        cs_close(&handle);
                        return;
                    }
                }
            }
        }
    }
    throw std::runtime_error("This shouldn't happen");
}

// #define TIME_START auto tp = std::chrono::system_clock::now();
// #define TIME_END(msg) std::cout << msg << " took " << std::chrono::duration_cast<std::chrono::milliseconds>((std::chrono::system_clock::now() - tp)).count() << "ms" << std::endl;

void BreakpointWatcherWindow::handle_breakpoint(user_regs_struct regs, pid_t tid)
{
    std::unique_lock lock{handler_mutex};

    auto it = breakpoint_hits.find(regs.rip);
    if (it != breakpoint_hits.end())
    {
        it->second.count++;
        it->second.regs = regs;
    }
    else
    {
        breakpoint_hits.emplace(regs.rip, BreakpointHit{std::nullopt, regs, 1});
        lock.unlock();
        disasm_preview_threads.emplace_back(&BreakpointWatcherWindow::get_disasm_preview, this, tid, regs.rip);
    }
}
