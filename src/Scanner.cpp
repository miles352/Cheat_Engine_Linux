#include "Scanner.hpp"


#include <algorithm>
#include <array>
#include <iostream>
#include <numeric>
#include <ostream>
#include <print>
#include <utility>

#include "Application.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "misc/cpp/imgui_stdlib.h"


void Scanner::scan_memory(pid_t pid, const std::vector<std::vector<MemUtils::AddressMapping>>& mappings)
{
    cancelled = false;
    using MemUtils::AddressMapping;

    scan_thread = std::thread{[this, pid, &mappings]
    {
        scan_value.visit([pid, this, &mappings]<typename T>(const T& val)
        {
            scanning = true;
            // Add up all the address distances in the maps
            uint64_t total_address_len = std::accumulate(mappings.begin(), mappings.end(), 0,
            [](uint64_t sum, const std::vector<AddressMapping>& same_name_mappings) { return sum + std::accumulate(same_name_mappings.begin(), same_name_mappings.end(), 0,
            [](uint64_t sum, const AddressMapping& mapping) { return sum + mapping.end - mapping.start; }); });
            uint64_t addresses_scanned{};
            std::vector<uintptr_t> new_addrs;
            std::vector<T> old_values;
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


                    if constexpr (!std::is_same_v<T, std::string>)
                    {
                        switch (scan_comparison)
                        {
                        case EQUAL_TO:
                            {
                                auto [addrs, values] = MemUtils::search_addr_range<T>(pid, map_to_scan.start, map_to_scan.end, val, [](T t1, T t2) { return t1 == t2; });
                                new_addrs.append_range(addrs);
                                old_values.append_range(values);
                                break;
                            }
                        case LESS_THAN:
                            {
                                auto [addrs, values] = MemUtils::search_addr_range<T>(pid, map_to_scan.start, map_to_scan.end, val, [](T t1, T t2) { return t1 < t2; });
                                new_addrs.append_range(addrs);
                                old_values.append_range(values);
                                break;
                            }
                        case GREATER_THAN:
                            {
                                auto [addrs, values] = MemUtils::search_addr_range<T>(pid, map_to_scan.start, map_to_scan.end, val, [](T t1, T t2) { return t1 > t2; });
                                new_addrs.append_range(addrs);
                                old_values.append_range(values);
                                break;
                            }
                        case UNKNOWN:
                            {
                                auto [addrs, values] = MemUtils::search_addr_range<T>(pid, map_to_scan.start, map_to_scan.end, val, [](T t1, T t2) { return true; });
                                new_addrs.append_range(addrs);
                                old_values.append_range(values);
                                break;
                            }
                        case INCREASED:
                        case DECREASED:
                        case UNCHANGED:
                        case CHANGED: std::unreachable(); // These cases are disabled in the UI when scanning for the first time
                        }
                    }
                    else
                    {
                        // TODO: Implement strings
                    }

                    addresses_scanned += map_to_scan.end - map_to_scan.start;
                    scan_percent = static_cast<float>(addresses_scanned) / total_address_len;
                }
            }
            std::println("Took {}ms", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - tp).count());
            std::lock_guard lock{scan_mutex};
            scanned_addrs = std::move(new_addrs);
            if constexpr (!std::is_same_v<T, std::string>)
            {
                scanned_old_values = std::move(old_values);
            }
            scanning = false;
        });
    }};
}

void Scanner::rescan_memory(pid_t pid)
{
    cancelled = false;
    scan_thread = std::thread{[this, pid]
    {
        scan_value.visit([pid, this]<typename T>(const T& val)
        {
            scanning = true;
            std::chrono::time_point<std::chrono::system_clock> tp = std::chrono::system_clock::now();
            std::vector<uintptr_t> still_valid; //TODO: Would be cool if it didnt allocate more memory temporarily, but probably fine
            std::vector<T> new_old_values;

            uint64_t total_new{};

            const size_t pagesize = getpagesize();
            std::vector<T> page_buffer(pagesize / sizeof(T));
            uintptr_t current_buffer_start = 0;
            for (int i = 0; i < scanned_addrs.size(); i++)
            {
                uintptr_t addr = scanned_addrs[i];
                if (cancelled)
                {
                    scanning = false;
                    return;
                }

                if (addr < current_buffer_start || addr > current_buffer_start + pagesize)
                {
                    current_buffer_start = (addr / pagesize) * pagesize;
                    iovec from{reinterpret_cast<void*>(current_buffer_start), pagesize};
                    iovec to{page_buffer.data(), pagesize};

                    ssize_t status = process_vm_readv(pid, &to, 1, &from, 1, 0);
                    if (status < 0)
                    {
                        // perror("Error: ");
                        continue;
                    }
                }

                if constexpr (!std::is_same_v<T, std::string>)
                {
                    T new_val = page_buffer[(addr % pagesize) / sizeof(T)];
                    switch (scan_comparison)
                    {
                    case EQUAL_TO:
                        if (new_val == val) still_valid.emplace_back(addr);
                        break;
                    case LESS_THAN:
                        if (new_val < val) still_valid.emplace_back(addr);
                        break;
                    case GREATER_THAN:
                        if (new_val > val) still_valid.emplace_back(addr);
                        break;
                    case UNKNOWN:
                        still_valid.emplace_back(addr);
                        break;
                    case INCREASED:
                        if (new_val > std::get<std::vector<T>>(scanned_old_values)[i]) still_valid.emplace_back(addr);
                        break;
                    case DECREASED:
                        if (new_val < std::get<std::vector<T>>(scanned_old_values)[i]) still_valid.emplace_back(addr);
                        break;
                    case UNCHANGED:
                        if (new_val == std::get<std::vector<T>>(scanned_old_values)[i]) still_valid.emplace_back(addr);
                        break;
                    case CHANGED:
                        if (new_val != std::get<std::vector<T>>(scanned_old_values)[i]) still_valid.emplace_back(addr);
                        break;
                    }
                    new_old_values.emplace_back(new_val);
                }
                else
                {
                    // TODO: Implement strings
                }


                scan_percent = static_cast<float>(total_new++) / scanned_addrs.size();
            }
            std::lock_guard lock{scan_mutex};
            scanned_addrs = std::move(still_valid);
            if constexpr (!std::is_same_v<T, std::string>)
            {
                scanned_old_values = std::move(new_old_values);
            }
            std::println("Took {}ms", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - tp).count());
            std::cout << std::endl;
            scanning = false;
        });
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
                scanned_old_values.visit([](auto& vec) { vec.clear(); });
                if (scan_comparison >= ScanComparisons::UNKNOWN) scan_comparison = ScanComparisons::EQUAL_TO;
            }
        }
    }

    draw_scantype_input(scan_value, "Scan Value");


    if (ImGui::BeginCombo("Scan Comparison", SCAN_COMPARISON_LABELS[scan_comparison]))
    {
        for (int i = 0; i < SCAN_COMPARISON_LABELS.size(); i++)
        {
            if (scanned_addrs.empty() && i > ScanComparisons::UNKNOWN) break;
            if (ImGui::Selectable(SCAN_COMPARISON_LABELS[i])) scan_comparison = static_cast<ScanComparisons>(i);
        }
        ImGui::EndCombo();
    }

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

void Scanner::draw_scan_results(pid_t pid)
{
    ImGui::Begin("Scan Results");
    ImGui::Text("Found: %lu", scanned_addrs.size());
    if (ImGui::BeginTable("address_results", 3, ImGuiTableFlags_ScrollY))
    {
        ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
        ImGui::TableSetupColumn("Address");
        ImGui::TableSetupColumn("Value");
        ImGui::TableSetupColumn("Old Value");
        ImGui::TableHeadersRow();


        ImGuiListClipper clipper;

        clipper.Begin(scanned_addrs.size());

        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
            {
                bool selected = selected_result_addrs.contains(scanned_addrs[i]);

                // set hover color to invisible so we dont get hover effects
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));
                if (!selected)
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0,0,0,0));
                else
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_Header));


                ImGui::TableNextRow();
                // Address column
                ImGui::TableNextColumn();

                ImGuiSelectableFlags flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_AllowDoubleClick;
                if (ImGui::Selectable(std::format("{:x}", scanned_addrs[i]).c_str(), selected, flags)) // if clicked or double clicked
                {
                    selected_result_addrs.clear();
                    selected_result_addrs.emplace(scanned_addrs[i]);
                    // TODO: Ctrl click / Shift click to select multiple

                    if (ImGui::IsMouseDoubleClicked(0))
                    {
                        addr_table_entries.emplace_back(false, "Enter description", scanned_addrs[i], scan_value);
                    }
                }

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
                ImGui::TableNextColumn();

                scanned_old_values.visit([i]<typename T>(const T& val)
                {
                    if constexpr (!std::is_same_v<T, std::string>)
                    {
                        ImGui::TextUnformatted(std::format("{}", val[i]).c_str());
                    }
                    else
                    {
                        // TODO: Implement strings
                    }
                });
                ImGui::PopStyleColor(2);
            }
        }

        ImGui::EndTable();
    }

    ImGui::End();

}

void Scanner::draw_addr_table(pid_t pid)
{
    ImGui::Begin("Address Table");

    if (ImGui::BeginTable("addr_table", 5, ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::PushStyleVarX(ImGuiStyleVar_CellPadding, 0.0f);
        ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
        ImGui::TableSetupColumn("Frozen", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch, 3.0f);
        ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableHeadersRow();
        ImGui::PopStyleVar();

        for (int i = 0; i < addr_table_entries.size(); i++)
        {
            AddrTableEntry& entry = addr_table_entries[i];

            // set hover color to invisible so we dont get hover effects
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));
            if (!entry.selected)
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0,0,0,0));
            else
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_Header));

            ImGui::PushID(&entry);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();

            if (ImGui::Selectable("##selectable_row", entry.selected, ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.0f, ImGui::GetFrameHeight())))
            {
                std::ranges::for_each(addr_table_entries, [](auto& entry) { entry.selected = false; });
                entry.selected = true;
                // TODO: Ctrl click / Shift click to select multiple
            }
            ImGui::SameLine(); // Needed or the selectable eats a column

            ScanType new_val = entry.value.visit([pid, &entry]<typename T>(const T& val)
            {
                if constexpr (!std::is_same_v<T, std::string>)
                {
                    auto new_val = ScanType{MemUtils::read_addr<T>(pid, entry.addr)};
                    if (entry.frozen && new_val != entry.value)
                    {
                        MemUtils::write_addr(pid, entry.addr, val);
                        return ScanType{val};
                    }
                    return new_val;
                }
                else
                {
                    // TODO: Implement strings
                    return ScanType{5};
                }
            });


            ImGui::Checkbox("##checkbox_frozen", &entry.frozen);

            ImGui::TableNextColumn();

            ImGui::SetNextItemWidth(-FLT_MIN); // make the text box take up the full column width
            ImGui::InputText("##text_description", &entry.description);

            ImGui::TableNextColumn();

            ImGui::Text("0x%lx", entry.addr);
            ImGui::TableNextColumn();

            ImGui::PopStyleColor(2);

            size_t new_type = new_val.index();
            if (ImGui::BeginCombo("##combo_typeselect", SCAN_TYPE_LABELS[new_val.index()]))
            {

                for (int j = 0; j < SCAN_TYPE_LABELS.size(); j++)
                {
                    if (ImGui::Selectable(SCAN_TYPE_LABELS[j], new_type == j))
                    {
                        new_type = j;
                    }
                }

                ImGui::EndCombo();
            }

            if (new_type != new_val.index())
            {
                new_val.visit([&new_val, new_type]<typename T>(const T& value)
                {
                    if constexpr (!std::is_same_v<T, std::string>)
                    {
                        if (new_type == 0) new_val = static_cast<int8_t>(value);
                        if (new_type == 1) new_val = static_cast<int16_t>(value);
                        if (new_type == 2) new_val = static_cast<int32_t>(value);
                        if (new_type == 3) new_val = static_cast<int64_t>(value);
                        if (new_type == 4) new_val = static_cast<float>(value);
                        if (new_type == 5) new_val = static_cast<double>(value);
                        if (new_type == 6) new_val = "";
                    }
                    else
                    {
                        if (new_type != 6) new_val = "";
                    }
                });
            }



            ImGui::TableNextColumn();

            ImGui::SetNextItemWidth(-FLT_MIN);
            draw_scantype_input(new_val, "##value");

            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                new_val.visit([pid, &entry]<typename T>(const T& val)
                {
                    if constexpr (!std::is_same_v<T, std::string>)
                    {
                        MemUtils::write_addr(pid, entry.addr, val);
                    }
                    else
                    {
                        // TODO: implement strings
                    }
                });
            }


            entry.value = new_val;


            ImGui::PopID();
        }


        ImGui::EndTable();
    }

    ImGui::End();
}

void Scanner::draw_scantype_input(ScanType& value, const char* label)
{
    value.visit([label]<typename T>(T& val)
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
            ImGui::InputScalar(label, type, &val);
        }
        else
        {
            // TODO: Implement strings
        }
    });
}
