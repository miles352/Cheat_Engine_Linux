#include "BreakpointWatcherWindow.hpp"

#include "Debugger.hpp"
#include "imgui.h"
#include "MemUtils.hpp"
#include "capstone/capstone.h"

BreakpointWatcherWindow::BreakpointWatcherWindow(AppState& state, const Debugger::Breakpoint& breakpoint): state(state), debugger(Debugger{state.process->pid})
{
    assert(breakpoint.mode == Debugger::READWRITE || breakpoint.mode == Debugger::WRITE_ONLY && "Breakpoint watcher does not support IO or execution breakpoints");
    watched_breakpoint = breakpoint;
    window_id = std::format("Find out what {} this address##debug_window", breakpoint.mode == Debugger::BreakpointMode::READWRITE ? "accesses" : "writes to");
    debugger.send_command(Debugger::SetBreakpointCommand{breakpoint, [this](const user_regs_struct& regs, pid_t tid) { handle_breakpoint(regs, tid); }});
}

void BreakpointWatcherWindow::draw()
{
    ImGui::Begin(window_id.c_str(), &open);

    if (!open || !debugger.process_valid()) state.send_event(CloseWindowEvent{WindowID::DEBUG});


    // thread creation breakpoint adds

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
            ImGui::TextUnformatted(breakpoint.prev_insn.has_value() ? breakpoint.prev_insn->second.c_str() : "Loading...");

            ImGui::PopID();
        }

        ImGui::EndTable();
    }


    ImGui::End();
}

std::pair<uintptr_t, std::string> get_previous_insn(pid_t pid, uintptr_t rip)
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

                for (size_t i = 0; i < count; i++)
                {
                    if (insn[i].address >= rip)
                    {
                        std::string disasm = std::format("{} {}", insn[i-1].mnemonic, insn[i-1].op_str);
                        std::pair<uintptr_t, std::string> prev_insn{insn[i-1].address, std::move(disasm)};
                        cs_free(insn, count);
                        cs_close(&handle);
                        return prev_insn;
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
        prev_insn_threads.emplace_back([this, tid, regs]()
        {
            auto prev_insn = get_previous_insn(tid, regs.rip);
            std::lock_guard lock{handler_mutex};
            breakpoint_hits[regs.rip].prev_insn = prev_insn;
        });
    }
}
