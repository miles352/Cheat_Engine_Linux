#include "Scanner.hpp"


#include <array>
#include <iostream>
#include <numeric>
#include <ostream>
#include <print>

#include "Application.hpp"
#include "imgui.h"


void Scanner::scan_memory(pid_t pid, const std::vector<std::vector<MemUtils::AddressMapping>>& mappings)
{
    cancelled = false;
    using MemUtils::AddressMapping;

    scan_thread = std::thread{[this, pid, &mappings]
    {
        scanning = true;
        // Add up all the address distances in the maps
        uint64_t total_address_len = std::accumulate(mappings.begin(), mappings.end(), 0,
        [](uint64_t sum, const std::vector<AddressMapping>& same_name_mappings) { return sum + std::accumulate(same_name_mappings.begin(), same_name_mappings.end(), 0,
        [](uint64_t sum, const AddressMapping& mapping) { return sum + mapping.end - mapping.start; }); });

        uint64_t addresses_scanned{};
        std::vector<uintptr_t> new_addrs;
        auto tp = std::chrono::system_clock::now();
        // accessing mappings should be fine without lock because it is reading only and the main thread only can write to it when scanning is false
        for (auto& mapping : mappings)
        {
            if (!selected_mapping.empty() && mapping[0].pathname != selected_mapping) continue;
            for (const AddressMapping& map_to_scan : mapping)
            {
                if (cancelled)
                {
                    scanning = false;
                    return;
                }

                scan_value.visit([pid, &map_to_scan, &new_addrs]<typename T>(const T& val)
                {
                    if constexpr (!std::is_same_v<T, std::string>)
                    {
                        new_addrs.append_range(MemUtils::search_addr_range<T>(pid, map_to_scan.start, map_to_scan.end, [&val](T addr_val) { return addr_val == val; }));
                    }
                    else
                    {
                        // TODO: Implement strings
                    }
                });

                addresses_scanned += map_to_scan.end - map_to_scan.start;
                scan_percent = static_cast<float>(addresses_scanned) / total_address_len;
            }
        }
        std::println("Took {}ms", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - tp).count());
        std::lock_guard lock{scan_mutex};
        scanned_addrs = std::move(new_addrs);
        scanning = false;
    }};
}

void Scanner::rescan_memory(pid_t pid)
{
    cancelled = false;
    scan_thread = std::thread{[this, pid]
    {
        scanning = true;
        std::chrono::time_point<std::chrono::system_clock> tp = std::chrono::system_clock::now();
        std::vector<uintptr_t> still_valid; //TODO: Would be cool if it didnt allocate more memory temporarily, but probably fine

        std::atomic_uint64_t total_new{};
        for (uintptr_t addr : scanned_addrs)
        {
            if (cancelled)
            {
                scanning = false;
                return;
            }

            scan_value.visit([pid, addr, &still_valid]<typename T>(const T& val)
            {
                if constexpr (!std::is_same_v<T, std::string>)
                {
                    if (MemUtils::read_addr<T>(pid, addr) == val) still_valid.emplace_back(addr);
                }
                else
                {
                    // TODO: Implement strings
                }
            });


            scan_percent = static_cast<float>(total_new++) / scanned_addrs.size();
        }
        std::lock_guard lock{scan_mutex};
        scanned_addrs = std::move(still_valid);
        std::println("Took {}ms", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - tp).count());
        std::cout << std::endl;
        scanning = false;
    }};
}

void Scanner::draw(pid_t pid)
{
    bool currently_scanning = scanning;
    scan_mutex.lock();

    ImGui::Begin("Scanner");

    if (ImGui::BeginPopup("invalid_id"))
    {
        ImGui::Text("The selected memory region no longer exists, select a valid region.");
        ImGui::Separator();

        if (ImGui::Button("OK", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    if (currently_scanning)
    {
        if (ImGui::Button("Cancel"))
        {
            cancelled = true;
        }
        ImGui::SameLine();
        ImGui::ProgressBar(scan_percent, ImVec2(0.0f, 0.0f));
        ImGui::BeginDisabled(); // Everything after this is disabled if scanning
    }
    else
    {
        if (scan_thread.joinable())
        {
            scan_thread.join();
        }

        if (scanned_addrs.empty() && ImGui::Button("Scan"))
        {
            mappings = Application::get_mappings(pid, show_library_mappings);
            auto it = std::ranges::find_if(mappings, [this](auto& same_name_mappings){ return same_name_mappings[0].pathname == selected_mapping; });
            if (it == mappings.end())
            {
                selected_mapping.clear();
                ImGui::OpenPopup("invalid_id");
            }
            else
            {
                scan_memory(pid, mappings);
            }

        }
        else if (!scanned_addrs.empty())
        {
            if (ImGui::Button("Rescan"))
            {
                rescan_memory(pid);
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear"))
            {
                scanned_addrs.clear();
            }
        }
    }

    scan_value.visit([this]<typename T>(T& val)
    {
        if constexpr (!std::is_same_v<T, std::string>)
        {
            ImGuiDataType type;
            if constexpr (std::is_same_v<T, int8_t>)
            {
                type = ImGuiDataType_S8;
            }
            else if constexpr (std::is_same_v<T, int16_t>)
            {
                type = ImGuiDataType_S16;
            }
            else if constexpr (std::is_same_v<T, int32_t>)
            {
                type = ImGuiDataType_S32;
            }
            else if constexpr (std::is_same_v<T, int64_t>)
            {
                type = ImGuiDataType_S64;
            }
            else if constexpr (std::is_same_v<T, float>)
            {
                type = ImGuiDataType_Float;
            }
            else if constexpr (std::is_same_v<T, double>)
            {
                type = ImGuiDataType_Double;
            }
            ImGui::InputScalar("Scan Value", type, &val);
        }
        else
        {
            // TODO: Implement strings
        }
    });

    // The options after this shouldn't change in the middle of a sequence of scans
    if (!scanned_addrs.empty()) ImGui::BeginDisabled();

    if (ImGui::BeginCombo("Scan Type", SCAN_TYPE_LABELS[scan_value.index()]))
    {
        if (ImGui::Selectable(SCAN_TYPE_LABELS[0])) scan_value = static_cast<int8_t>(0);
        if (ImGui::Selectable(SCAN_TYPE_LABELS[1])) scan_value = static_cast<int16_t>(0);
        if (ImGui::Selectable(SCAN_TYPE_LABELS[2])) scan_value = static_cast<int32_t>(0);
        if (ImGui::Selectable(SCAN_TYPE_LABELS[3])) scan_value = static_cast<int64_t>(0);
        if (ImGui::Selectable(SCAN_TYPE_LABELS[4])) scan_value = static_cast<float>(0);
        if (ImGui::Selectable(SCAN_TYPE_LABELS[5])) scan_value = static_cast<double>(0);
        if (ImGui::Selectable(SCAN_TYPE_LABELS[6])) scan_value = std::string{};

        ImGui::EndCombo();
    }

    const char* mem_region_preview = selected_mapping.empty() ? "All" : selected_mapping.c_str();
    if (ImGui::BeginCombo("Memory Region", mem_region_preview))
    {
        if (!mapping_select_menu_open)
        {
            // Only get the mappings once when the dropdown is opened
            mappings = Application::get_mappings(pid, show_library_mappings);
            mapping_select_menu_open = true;
        }
        if (ImGui::Selectable("All")) selected_mapping.clear();
        for (auto& mapping : mappings)
        {
            const std::string& pathname = mapping[0].pathname;
            if (pathname.empty()) continue;
            if (ImGui::Selectable(pathname.c_str())) selected_mapping = pathname;
        }

        ImGui::EndCombo();
    }
    else
    {
        mapping_select_menu_open = false;
    }
    ImGui::Checkbox("Show Libraries", &show_library_mappings);

    if (!scanned_addrs.empty()) ImGui::EndDisabled();
    if (currently_scanning) ImGui::EndDisabled();

    ImGui::End();

    // ----- end main window

    draw_scan_results(pid);

    draw_addr_table(pid);


    scan_mutex.unlock();
}

void Scanner::draw_scan_results(pid_t pid) const
{
    ImGui::Begin("Scan Results");
    ImGui::Text("Found: %d", scanned_addrs.size());
    ImGui::BeginTable("address_results", 2, ImGuiTableFlags_ScrollY);

    ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
    ImGui::TableSetupColumn("Address");
    ImGui::TableSetupColumn("Value");
    // ImGui::TableSetupColumn("Old Value");
    ImGui::TableHeadersRow();


    ImGuiListClipper clipper;

    clipper.Begin(scanned_addrs.size());

    while (clipper.Step())
    {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
        {
            ImGui::TableNextRow();
            // Address column
            ImGui::TableNextColumn();
            ImGui::Text("0x%lx", scanned_addrs[i]);

            // Value column
            ImGui::TableNextColumn();

            scan_value.visit([pid, addr = scanned_addrs[i]]<typename T>(const T& val)
            {
                if constexpr (!std::is_same_v<T, std::string>)
                {
                    ImGui::TextUnformatted(std::format("{}", MemUtils::read_addr<T>(pid, addr)).c_str());
                }
                else
                {
                    // TODO: Implement strings
                }
            });

            // Old Value column
            // ImGui::TableNextColumn();

            // scanned_old_vals[i].visit([]<typename T>(const T& val)
            // {
            //     if constexpr (!std::is_same_v<T, std::string>)
            //     {
            //         ImGui::TextUnformatted(std::format("{}", val).c_str());
            //     }
            //     else
            //     {
            //         // TODO: Implement strings
            //     }
            // });
        }
    }

    ImGui::EndTable();

    ImGui::End();

}



void Scanner::draw_addr_table(pid_t pid)
{
    ImGui::Begin("Address Table");

    ImGui::BeginTable("addr_table", 5);

    ImGui::TableSetupColumn("Frozen");
    ImGui::TableSetupColumn("Description");
    ImGui::TableSetupColumn("Address");
    ImGui::TableSetupColumn("Type");
    ImGui::TableSetupColumn("Value");
    ImGui::TableHeadersRow();

    // TODO: Doesnt make sense to store ScanType here because we just want the type, the value is calculated every frame

    for (auto& entry : entries)
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Checkbox("", &entry.frozen);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(entry.description.c_str());
        ImGui::TableNextColumn();
        ImGui::Text("0x%lx", entry.addr);
        ImGui::TableNextColumn();
        ImGui::Text("Float");
        ImGui::TableNextColumn();
        ImGui::Text("%f", std::get<float>(entry.value));

    }

    // frozen, description, addr, type, value

    // bool, std::string, uintptr_t, ScanType




    ImGui::EndTable();

    ImGui::End();
}
