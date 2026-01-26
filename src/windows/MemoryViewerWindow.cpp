#include "MemoryViewerWindow.hpp"

#include <cstring>

#include "imgui.h"
#include "MemUtils.hpp"

size_t MemoryViewerWindow::get_addr_size(uintptr_t addr)
{
    auto it = modified_addrs.find(addr);
    if (it != modified_addrs.end() && it->second.type.has_value())
    {
        return get_scantype_size(*it->second.type);
    }
    return DEFAULT_TYPE_SIZE;
}

void MemoryViewerWindow::draw()
{
    bool open = true;
    ImGui::Begin("Memory Viewer", &open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    if (!open)
    {
        state.send_event(CloseWindowEvent{Window::MEMORY_VIEWER});
        ImGui::End();
        return;
    }

    if (ImGui::Button("Set Base Address"))
    {
        ImGui::OpenPopup("Enter Base Address");
        set_keyboard_focus = true;
    }
    if (base_addr.has_value())
    {
        ImGui::SameLine();
        if (ImGui::Button("Clear Base Address"))
        {
            base_addr = std::nullopt;
        }
    }

    if (ImGui::BeginPopupModal("Enter Base Address", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (set_keyboard_focus)
        {
            ImGui::SetKeyboardFocusHere();
            set_keyboard_focus = false;
        }

        ImGui::InputScalar("Base Address", ImGuiDataType_U64, &new_base_addr, 0, 0, "%lx");

        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Save"))
        {
            this->base_addr = new_base_addr;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (ImGui::BeginTable("MemoryView", 4))
    {
        float address_width = std::max(ImGui::CalcTextSize(std::format("0x{:x}", top_addr).c_str()).x,
                                        ImGui::CalcTextSize("Address").x);
        if (base_addr.has_value()) // add extra width if a base address is also displayed
        {
            address_width += ImGui::CalcTextSize(std::format(" + 0x{:x}", *base_addr).c_str()).x;
        }
        const float name_width = ImGui::CalcTextSize("________________").x; // 16 chars
        const float type_width = ImGui::CalcTextSize("(untyped)______").x;

        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, address_width);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, name_width);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, type_width);
        ImGui::TableSetupColumn("Value");
        ImGui::TableHeadersRow();

        float row_height = ImGui::GetTextLineHeightWithSpacing();
        float visible_height = ImGui::GetContentRegionAvail().y;

        int row_count = static_cast<int>(visible_height / row_height) + 1;

        auto scroll_amount = static_cast<int>(ImGui::GetIO().MouseWheel);

        while (scroll_amount != 0)
        {
            if (scroll_amount > 0) // scrolling up
            {
                scroll_amount--;
                uintptr_t new_top_addr = top_addr;
                auto lbound = modified_addrs.lower_bound(top_addr);
                if (lbound == modified_addrs.begin())
                {
                    // if its the beginning, it must be >= to top_addr, and there cannot be something before it so just decrease by default size
                    new_top_addr -= DEFAULT_TYPE_SIZE;
                }
                else
                {
                    auto prev_val = std::prev(lbound);
                    size_t addr_size = get_addr_size(prev_val->first);
                    if (top_addr - (prev_val->first + addr_size) >= DEFAULT_TYPE_SIZE) // if the gap between the address has enough space for the default size
                    {
                        new_top_addr -= DEFAULT_TYPE_SIZE;
                    }
                    else
                    {
                        new_top_addr = prev_val->first;
                    }
                }
                // If a base address is set dont allow scrolling above it
                if (!base_addr.has_value() || new_top_addr >= *base_addr)
                {
                    top_addr = new_top_addr;
                }
            }
            else // scrolling down
            {
                scroll_amount++;
                top_addr += get_addr_size(top_addr);
            }
        }

        // Needed so that the stack id is calculated outside of all the inner scopes
        ImGuiID rename_address_id = ImGui::GetID("Rename Address");

        uintptr_t last_addr = top_addr;

        for (int i = 0; i < row_count; i++)
        {
            ImGui::PushID(i);

            last_addr = draw_address_row(last_addr, rename_address_id);
            ImGui::PopID();
        }

        if (ImGui::BeginPopupModal("Rename Address", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (set_keyboard_focus)
            {
                ImGui::SetKeyboardFocusHere();
                set_keyboard_focus = false;
            }
            // std::string is cleared before modal is opened and resized to 16 chars
            ImGui::InputText("Address Name", new_addr_name.data(), 16 + 1); // max name of 16 chars
            if (ImGui::Button("Cancel"))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Save"))
            {
                auto it = modified_addrs.find(addr_being_renamed);
                if (it != modified_addrs.end())
                {
                    modified_addrs[addr_being_renamed].name = std::move(new_addr_name);
                }
                else
                {
                    modified_addrs.emplace(addr_being_renamed, ModifiedAddr{std::move(new_addr_name), std::nullopt});
                }

                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::EndTable();
    }

    ImGui::End();
}


uintptr_t MemoryViewerWindow::draw_address_row(uintptr_t row_addr, ImGuiID rename_address_id)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    ImGui::Selectable("##selectable_row", false, ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.0f, ImGui::GetFrameHeight()));

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::SameLine();
    ImGui::PopStyleVar();

    // Context menu when right clicking anywhere in the row
    if (ImGui::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonRight))
    {
        if (ImGui::MenuItem("Rename"))
        {
            addr_being_renamed = row_addr;
            new_addr_name.clear();
            new_addr_name.resize(16);
            set_keyboard_focus = true;
            ImGui::OpenPopup(rename_address_id);
        }
        if (ImGui::BeginMenu("Change Type"))
        {
            std::optional<ScanType> changed_type{};
            if (ImGui::MenuItem("Int8")) changed_type = static_cast<int8_t>(0);
            if (ImGui::MenuItem("Int16")) changed_type = static_cast<int16_t>(0);
            if (ImGui::MenuItem("Int32")) changed_type = static_cast<int32_t>(0);
            if (ImGui::MenuItem("Int64")) changed_type = static_cast<int64_t>(0);
            if (ImGui::MenuItem("Float")) changed_type = static_cast<float>(0);
            if (ImGui::MenuItem("Double")) changed_type = static_cast<double>(0);
            if (ImGui::MenuItem("String")) changed_type = std::string{};

            if (changed_type.has_value()) // if a new type was clicked
            {
                // Update new type
                auto modified_addr = modified_addrs.find(row_addr);
                if (modified_addr != modified_addrs.end())
                {
                    modified_addr->second.type = *changed_type;
                }
                else
                {
                    modified_addrs.emplace(row_addr, ModifiedAddr{std::nullopt, *changed_type});
                }

                // Delete any modified addrs that are within the length of the newly selected type to keep the map clean
                size_t type_length = get_scantype_size(*changed_type);
                for (size_t i = 1; i < type_length; i++) // start 1 after the start address of the row
                {
                    modified_addrs.erase(row_addr + i);
                }
                // TODO: Also need to add padding bytes to make sure 4 byte alignment stays below this address.
                // For example, 4 bytes converted to 1 byte should add 3 bytes of padding as well
            }

            ImGui::EndMenu();
        }

        ImGui::EndPopup();
    }

    if (base_addr.has_value())
    {
        ImGui::Text("0x%lx + 0x%lx", *base_addr, row_addr - *base_addr);
    }
    else
    {
        ImGui::Text("0x%lx", row_addr);
    }


    ImGui::TableNextColumn();


    // Display (unnamed) or the user defined name for the address
    auto modified_addr = modified_addrs.find(row_addr);
    if (modified_addr != modified_addrs.end() && modified_addr->second.name.has_value())
    {
        ImGui::TextUnformatted(modified_addr->second.name->c_str());
    }
    else
    {
        // Display as greyed out if unnamed
        ImGui::BeginDisabled();
        ImGui::TextUnformatted("(unnamed)");
        ImGui::EndDisabled();
    }

    ImGui::TableNextColumn();
    // Display (untyped) or the user defined type for the address
    bool type_defined = modified_addr != modified_addrs.end() && modified_addr->second.type.has_value(); // defined because its used later
    if (type_defined)
    {
        auto type_name = modified_addr->second.type->visit([]<typename T>(const T&) -> const char*
        {
            if constexpr (std::is_same_v<T, int8_t>)
            {
                return "Int8";
            }
            else if constexpr (std::is_same_v<T, int16_t>)
            {
                return "Int16";
            }
            else if constexpr (std::is_same_v<T, int32_t>)
            {
                return "Int32";
            }
            else if constexpr (std::is_same_v<T, int64_t>)
            {
                return "Int64";
            }
            else if constexpr (std::is_same_v<T, float>)
            {
                return "Float";
            }
            else if constexpr (std::is_same_v<T, double>)
            {
                return "Double";
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                return "String";
            }
            return "Unknown";
        });
        ImGui::TextUnformatted(type_name);
    }
    else
    {
        ImGui::BeginDisabled();
        ImGui::TextUnformatted("(untyped)");
        ImGui::EndDisabled();
    }


    ImGui::TableNextColumn();

    // get bytes for biggest type
    std::vector<uint8_t> bytes = MemUtils::read_addrs(state.process->pid, row_addr, 8);
    if (!type_defined)
    {
        int32_t int32_t_display;
        float float_display;
        double double_display;
        // TODO: String display should be default too if the address starts with a character within ascii range

        std::memcpy(&int32_t_display, bytes.data(), sizeof(int32_t));
        std::memcpy(&float_display, bytes.data(), sizeof(float));
        std::memcpy(&double_display, bytes.data(), sizeof(double));

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75, 0.2, 0.2, 1));
        ImGui::Text("%d", int32_t_display);
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2, 0.75, 0.2, 1));
        ImGui::Text("%f", float_display);
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2, 0.2, 0.75, 1));
        ImGui::Text("%lf", double_display);
        ImGui::SameLine();

        ImGui::PopStyleColor(3);
    }
    else // if a type is defined, only show that type
    {
        modified_addr->second.type->visit([&bytes]<typename T>(const T&)
        {
            if constexpr (!std::is_same_v<T, std::string>)
            {
                T num;
                std::memcpy(&num, bytes.data(), sizeof(T));
                ImGui::TextUnformatted(std::format("{}", num).c_str());
            }
            else
            {
                // TODO: implement strings
            }
        });
    }




    return row_addr + get_addr_size(row_addr);
}
