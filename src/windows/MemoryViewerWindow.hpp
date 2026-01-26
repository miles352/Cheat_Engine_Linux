#pragma once

#include <map>
#include <optional>
#include <string>

#include "AppState.hpp"
#include "imgui.h"
#include "ScanType.hpp"
#include "Window.hpp"


class MemoryViewerWindow : public Window
{
    struct ModifiedAddr
    {
        std::optional<std::string> name;
        std::optional<ScanType> type; // variant only used for type information, kinda dumb but variants have a nice visit api
    };

    std::optional<uintptr_t> base_addr; // used to display addresses as an offset instead of absolute address
    AppState& state;
    uintptr_t top_addr; // Top address is always assumed to be correct, meaning it's at the start of a row
    std::map<uintptr_t, ModifiedAddr> modified_addrs;

    // State variables for popup modals
    uintptr_t addr_being_renamed;
    std::string new_addr_name{};
    bool set_keyboard_focus;
    uintptr_t new_base_addr{};



    static constexpr int DEFAULT_TYPE_SIZE = 0x4; // If an address is not given a type, it will take up this amount of bytes

    size_t get_addr_size(uintptr_t addr);

    uintptr_t draw_address_row(uintptr_t row_addr, ImGuiID rename_address_id);
public:
    MemoryViewerWindow(AppState& state, uintptr_t top_addr) : state(state), top_addr(top_addr) {};

    void draw() override;

};
