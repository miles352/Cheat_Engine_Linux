#pragma once
#include <array>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include "MemUtils.hpp"

class Scanner
{

public:
    using ScanType = std::variant<int8_t, int16_t, int32_t, int64_t, float, double, std::string>;

    struct AddrTableEntry
    {
        bool frozen;
        std::string description;
        uintptr_t addr;
        ScanType value;
    };

    /** Mutex used on the scanning thread. */
    std::mutex scan_mutex;
    std::thread scan_thread;
    /** The addresses found when scanning. */
    std::vector<uintptr_t> scanned_addrs;
    /** A flag used for if the scanner is currently scanning the memory. */
    std::atomic_bool scanning = false;
    std::atomic_bool cancelled = false;
    /** The value to look for when scanning. Cast to other smaller types if not scanning for 8 byte value. */
    ScanType scan_value{int32_t{0}};
    std::atomic<float> scan_percent{};
    /** Mappings of the virtual address spaces the program has control of. */
    std::vector<std::vector<MemUtils::AddressMapping>> mappings;
    bool show_library_mappings = false;
    bool mapping_select_menu_open = false;
    /** The region of memory to scan as an index in mappings. If empty then all regions will be scanned. */
    std::string selected_mapping; // has to be a string because indexes can be invalidated when the mappings are recomputed

    // Address table
    std::vector<AddrTableEntry> entries = {AddrTableEntry{false, "Health", 0x7ff18af91cd1, 100.0f}};

    static constexpr std::array<const char*, 7> SCAN_TYPE_LABELS {"1 Byte", "2 Bytes", "4 Bytes", "8 Bytes", "Float", "Double", "String"};

    /** Scan the memory for the first time */
    void scan_memory(pid_t pid, const std::vector<std::vector<MemUtils::AddressMapping>>& mappings);
    /** Scan the memory when scanned_addrs already contains found addresses */
    void rescan_memory(pid_t pid);

    void draw(pid_t pid);
    void draw_scan_results(pid_t pid) const;
    void draw_addr_table(pid_t pid);
};
