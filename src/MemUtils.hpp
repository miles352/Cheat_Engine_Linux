#pragma once

#include <cassert>
#include <cstdint>
#include <functional>
#include <sched.h>
#include <string>
#include <type_traits>
#include <unistd.h>
#include <vector>
#include <sys/uio.h>

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
}