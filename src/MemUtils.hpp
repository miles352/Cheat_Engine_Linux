#pragma once

#include <cassert>
#include <cstdint>
#include <fstream>
#include <functional>
#include <sched.h>
#include <string>
#include <type_traits>
#include <unistd.h>
#include <vector>
#include <sys/uio.h>
#include <print>

namespace MemUtils
{
    // struct representing lines in /proc/pid/maps
    struct AddressMapping
    {
        uintptr_t start;
        uintptr_t end;
        uint8_t permissions;
        uintptr_t offset; // offset into the file the memory is mapped to
        // device
        // inode
        std::string pathname;
    };

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    T read_addr(pid_t pid, uintptr_t addr)
    {
        T val;
        iovec from{reinterpret_cast<void*>(addr), sizeof(T)};
        iovec to{&val, sizeof(T)};

        ssize_t status = process_vm_readv(pid, &to, 1, &from, 1, 0);
        if (status < 0)
        {
            // perror("Error: ");
            return 0;
        }
        return val;
    }

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    void write_addr(pid_t pid, uintptr_t addr, T value)
    {
        iovec from{reinterpret_cast<void*>(&value), sizeof(T)};
        iovec to{reinterpret_cast<void*>(addr), sizeof(T)};

        ssize_t status = process_vm_writev(pid, &from, 1, &to, 1, 0);
        // if (status < 0)
        // {
        // perror("Error2: ");
        // }
    }

    /** Returns a pair containing the addresses found, and the value read at that address */
    template <typename T, typename Pred>
    requires std::is_trivially_copyable_v<T> && std::predicate<Pred, T, T>
    std::pair<std::vector<uintptr_t>, std::vector<T>> search_addr_range(pid_t pid, uintptr_t start, uintptr_t end, T value, Pred&& pred)
    {
        size_t page_size = getpagesize();
        assert(end > start);
        assert((end - start) % page_size == 0);

        std::vector<uintptr_t> addrs;
        std::vector<T> values;
        std::vector<T> buff(page_size / sizeof(T));
        for (int i = 0; i < (end - start) / page_size; i++)
        {
            iovec from{reinterpret_cast<void*>(start + page_size * i), page_size};
            iovec to{buff.data(), page_size};

            ssize_t amt_read = process_vm_readv(pid, &to, 1, &from, 1, 0);
            if (amt_read < 0)
            {
                // perror("Error: "); // things like /dev/nvidiactl fail here
                continue;
            }


            for (int j = 0; j < amt_read / sizeof(T); j++)
            {
                if (pred(buff[j], value))
                {
                    addrs.push_back(start + page_size * i + j * sizeof(T));
                    values.push_back(buff[j]);
                }
            }
        }

        return std::pair{std::move(addrs), std::move(values)};
    }

    inline std::vector<std::vector<AddressMapping>> get_mappings(pid_t pid, bool include_libs)
    {
        auto tp = std::chrono::system_clock::now();
        std::filesystem::path exe_path = std::filesystem::read_symlink(std::format("/proc/{}/exe", pid));

        std::vector<std::vector<MemUtils::AddressMapping>> mappings;

        std::ifstream maps{std::format("/proc/{}/maps", pid)};

        while (true)
        {
            MemUtils::AddressMapping mapping{};
            // parse lines in /proc/pid/maps

            maps >> std::hex >> mapping.start;
            if (maps.eof()) break;
            maps.get(); // move passed dash
            maps >> mapping.end;
            maps.get(); // move passed space
            if (maps.get() == 'r') mapping.permissions |= 0x1;
            if (maps.get() == 'w') mapping.permissions |= 0x2;
            if (maps.get() == 'x') mapping.permissions |= 0x4;

            int p_or_s = maps.get();
            if (p_or_s == 'p') mapping.permissions |= 0x8;
            if (p_or_s == 's') mapping.permissions |= 0x10;

            maps >> mapping.offset;
            maps >> std::dec;

            int dev_maj, dev_min; // Unused
            maps >> dev_maj;
            maps.get();
            maps >> dev_min;

            int inode; // Unused
            maps >> inode;

            std::getline(maps, mapping.pathname);
            // trim leading whitespace
            auto it = std::ranges::find_if_not(mapping.pathname, [](char c) { return std::isspace(c); });
            mapping.pathname.erase(mapping.pathname.begin(), it);


            if ((mapping.permissions & 0x1) == 0) continue; // dont include mappings that are unreadable



            if (mapping.pathname == exe_path
                || mapping.pathname.empty() // always include unnamed regions
                || mapping.pathname == "[heap]" || mapping.pathname.contains("[stack") // catch stacks marked with thread ids: [stack:tid]
                || (include_libs && (mapping.pathname.rfind(".so") != std::string::npos || mapping.permissions & 0x10)))
            {
                auto it = std::ranges::find_if(mappings, [&mapping](const std::vector<MemUtils::AddressMapping>& other_mapping) { return other_mapping[0].pathname == mapping.pathname; });
                if (it != mappings.end())
                {
                    it->push_back(mapping);
                }
                else
                {
                    mappings.emplace_back(1, mapping);
                }
            }

            // include if:
            // - pathname same as pid
            // - unnamed mapping
            // - [heap] || [stack]
            // - libraries enabled && pathname includes .so || mapped as shared
        }
        std::println("Took {}ms", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - tp).count());
        return mappings;
    }
}