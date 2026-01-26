#pragma once
#include "Window.hpp"
#include <array>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <variant>
#include <vector>

#include "AppState.hpp"
#include "MemUtils.hpp"
#include "Process.hpp"
#include "ScanType.hpp"


class ScanWindow : public Window
{
    void draw() override;

    using ScanResultsValues = std::variant<std::vector<int8_t>, std::vector<int16_t>, std::vector<int32_t>, std::vector<int64_t>, std::vector<float>, std::vector<double>>;

    struct AddrTableEntry
    {
        bool frozen;
        std::string description;
        uintptr_t addr;
        ScanType value;
        bool selected;
    };

    enum ScanComparisons
    {
        EQUAL_TO,     // scan value == user typed value
        LESS_THAN,    // scan value < user typed value
        GREATER_THAN, // scan value > user typed value
        UNKNOWN,      // always true
        INCREASED,    // new value > old value
        DECREASED,    // new value < old value
        UNCHANGED,    // new value == old value
        CHANGED,      // new value != old value
    };


    AppState& state;
    /** Mutex used on the scanning thread. */
    std::mutex scan_mutex;
    std::thread scan_thread;
    /** The addresses found when scanning. */
    std::vector<uintptr_t> scanned_addrs;
    /** Stores the old values to be displayed and compared against. */
    ScanResultsValues scanned_old_values;
    /** A flag used for if the scanner is currently scanning the memory. */
    std::atomic_bool scanning = false;
    std::atomic_bool cancelled = false;
    /** The value to look for when scanning. Cast to other smaller types if not scanning for 8 byte value. */
    ScanType scan_value{int32_t{0}};
    /** The type of comparison to perform when scanning for values. */
    ScanComparisons scan_comparison {EQUAL_TO};
    std::atomic<float> scan_percent{};
    /** Mappings of the virtual address spaces the program has control of. */
    std::vector<std::vector<MemUtils::AddressMapping>> mappings;
    bool show_library_mappings = false;
    bool mapping_select_menu_open = false;
    /** The region of memory to scan as an index in mappings. If empty then all regions will be scanned. */
    std::string selected_mapping; // has to be a string because indexes can be invalidated when the mappings are recomputed

    // Address result window
    std::unordered_set<uintptr_t> selected_result_addrs;

    // Address table
    std::vector<AddrTableEntry> addr_table_entries;

    // TODO: Copy all uses of PROCESS when using on other threads

    // Process manager stuff (top bar and process select menu)
    void draw_process_manager();
    std::vector<Process> processes;
    static constexpr auto PROCESS_POPUP_TITLE = "Attach to Process";
    std::string process_search_str;
    // Flag to only set the keyboard focus to the search bar the first time the select menu appears
    bool set_keyboard_focus{};



    static constexpr std::array SCAN_TYPE_LABELS {"1 Byte", "2 Bytes", "4 Bytes", "8 Bytes", "Float", "Double", "String"};
    static constexpr std::array SCAN_COMPARISON_LABELS {"Equal to", "Less than", "Greater than", "Unknown (Add all)", "Increased", "Decreased", "Unchanged", "Changed"};


    /** Scan the memory for the first time */
    void scan_memory(const std::vector<std::vector<MemUtils::AddressMapping>>& mappings);
    /** Scan the memory when scanned_addrs already contains found addresses */
    void rescan_memory();

    void draw_scan_results();
    void draw_addr_table();

    static void draw_scantype_input(ScanType& value, const char* label);
public:
    explicit ScanWindow(AppState& state) : state(state) {};
};
