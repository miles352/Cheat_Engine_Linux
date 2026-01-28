#include "BreakpointWatcherWindow.hpp"

#include <iostream>

#include "Debugger.hpp"
#include "imgui.h"
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
    ImGui::Begin(window_id.c_str(), &open);

    if (!open || !debugger.process_valid())
    {
        state.send_event(CloseWindowEvent{WindowID::DEBUG});
        ImGui::End();
        return;
    }

    std::lock_guard lock{handler_mutex};

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

            for (const auto& [addr, breakpoint] : breakpoint_hits)
            {
                ImGui::PushID(static_cast<int>(addr));
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                bool selected = selected_hit.has_value() && *selected_hit == addr;
                if (ImGui::Selectable("", selected, ImGuiSelectableFlags_SpanAllColumns)) // This label needs to be blank because if it changes frequently imgui click detection does not work
                {
                    selected_hit = addr;
                }

                ImGui::SameLine();
                ImGui::Text("%d", breakpoint.count);

                if (breakpoint.disasm_preview.has_value())
                {
                    ImGui::TableNextColumn();
                    ImGui::Text("0x%lx", breakpoint.disasm_preview->first);

                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(breakpoint.disasm_preview->second.at(breakpoint.disasm_preview->first).c_str());
                }
                else
                {
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Loading...");

                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Loading...");
                }


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
                ImGui::Separator();
            }
            else
            {
                ImGui::TextUnformatted("Loading dissassembly...");
            }

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0, 0}); // make align with text height
            ImGui::PushStyleColor(ImGuiCol_FrameBg, {0.f, 0.f, 0.f, 0.f}); // remove text input box

            const user_regs_struct& regs = breakpoint_hits[*selected_hit].regs;
            // TODO: Add xmm registers
            std::string register_display = std::format("Registers (Values after highlighted instruction was executed):\n"
                            "RDI 0x{:x}\n"
                            "RSI 0x{:x}\n"
                            "RDX 0x{:x}\n"
                            "RCX 0x{:x}\n"
                            "RAX 0x{:x}\n"
                            "RIP 0x{:x}\n"
                            "RBX 0x{:x}\n"
                            "RBP 0x{:x}\n"
                            "RSP 0x{:x}\n"
                            "R8 0x{:x}\n"
                            "R9 0x{:x}\n"
                            "R10 0x{:x}\n"
                            "R11 0x{:x}\n"
                            "R12 0x{:x}\n"
                            "R13 0x{:x}\n"
                            "R14 0x{:x}\n"
                            "R15 0x{:x}\n"
                            "CS 0x{:x}\n"
                            "SS 0x{:x}\n"
                            "DS 0x{:x}\n"
                            "ES 0x{:x}\n"
                            "FS 0x{:x}\n"
                            "GS 0x{:x}",
                            regs.rdi,
                            regs.rsi,
                            regs.rdx,
                            regs.rcx,
                            regs.rax,
                            regs.rip,
                            regs.rbx,
                            regs.rbp,
                            regs.rsp,
                            regs.r8,
                            regs.r9,
                            regs.r10,
                            regs.r11,
                            regs.r12,
                            regs.r13,
                            regs.r14,
                            regs.r15,
                            regs.cs,
                            regs.ss,
                            regs.ds,
                            regs.es,
                            regs.fs,
                            regs.gs);

            // https://github.com/ocornut/imgui/issues/950#issuecomment-1605762156
            ImVec2 text_size = ImGui::CalcTextSize(register_display.c_str(), register_display.c_str() + register_display.size());
            text_size.x = -FLT_MIN; // fill width (suppresses label)
            text_size.y += ImGui::GetStyle().FramePadding.y; // single pad

            ImGui::InputTextMultiline("##multilineInput", register_display.data(), register_display.size() + 1, text_size, ImGuiInputTextFlags_ReadOnly);
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();


        }
        ImGui::EndChild();
    }


    ImGui::End();
}

#define TIME_START auto tp = std::chrono::system_clock::now();
#define TIME_END(msg) std::cout << msg << " took " << std::chrono::duration_cast<std::chrono::milliseconds>((std::chrono::system_clock::now() - tp)).count() << "ms" << std::endl;

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
                const std::vector<uint64_t>& insn_lens = state.process_cache->get_insn_lens(map);


                uintptr_t addr = map.start; // the address before rip
                std::array<uintptr_t, DISASM_PREVIEW_LEN / 2> history{};
                size_t history_counter{};
                for (uint64_t packed : insn_lens)
                {
                    for (int j = 0; j < 16; j++)
                    {
                        uintptr_t next = addr + (packed & 0xF);
                        packed >>= 4;

                        // save last DISASM_PREVIEW_LEN addresses so we can get a valid starting address to give to capstone
                        history[history_counter % history.size()] = addr;
                        history_counter++;

                        if (next == rip)
                        {
                            uintptr_t preview_addr = history[(history_counter - history.size()) % history.size()];
                            std::vector<uint8_t> bytes = MemUtils::read_addrs(pid, preview_addr, DISASM_PREVIEW_LEN * 15); // Read 15 bytes for each instruction just incase they are that big

                            csh csh;
                            cs_open(CS_ARCH_X86, CS_MODE_64, &csh);
                            cs_insn* insns;
                            size_t count = cs_disasm(csh, bytes.data(), bytes.size(), preview_addr, DISASM_PREVIEW_LEN, &insns);
                            std::map<uintptr_t, std::string> saved_insns;
                            for (size_t i{}; i < count; i++)
                            {
                                saved_insns.emplace(insns[i].address, std::format("{} {}", insns[i].mnemonic, insns[i].op_str));
                            }
                            std::lock_guard lock{handler_mutex};
                            breakpoint_hits[rip].disasm_preview = {addr, std::move(saved_insns)};
                            cs_free(insns, count);
                            cs_close(&csh);
                            return;
                        }

                        addr = next;
                    }
                }
            }
        }
    }
    std::cerr << "Failed to find mapping that contains RIP" << std::endl;
}

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
