#pragma once

#include <vector>
#include <mutex>
#include <unordered_map>

#include "MemUtils.hpp"
#include "capstone/capstone.h"


class ProcessCache
{
    std::unordered_map<MemUtils::AddressMapping, std::vector<uint64_t>, MemUtils::AddressMapping::Hash> insn_caches;
    std::mutex mutex;
    pid_t pid;

public:
    explicit ProcessCache(pid_t pid) : pid(pid) {};

    /** Finds the lengths of each instruction in a mapping of memory.
     * NOTE: This method is a BLOCKING call, and if there is no cached value then it will calculate it which takes some time.
     * @param mapping The mapping, this is not checked to be executable memory, or that it exists at all.
     * @returns A vector of packed instruction lengths where each length is 4 bits. The sizes are packed from right to left.
     */
    const std::vector<uint64_t>& get_insn_lens(const MemUtils::AddressMapping& mapping)
    {
        // The mutex here is designed so that multiple threads may call this function at once, and they will not be able to
        // calculate new values at the same time. This choice was made because reading an entire processes memory with
        // capstone can use a lot of RAM, and doing it concurrently on many threads can easily cause OOM.



        std::lock_guard lock{mutex};
        // If already cached, return immediately
        auto it = insn_caches.find(mapping);
        if (it != insn_caches.end())
        {
            return it->second;
        }

        std::vector<uint8_t> memory = MemUtils::read_addrs(pid, mapping.start, mapping.end - mapping.start);

        csh csh;
        cs_open(CS_ARCH_X86, CS_MODE_64, &csh);
        // Ignoring possible errors for capstone
        cs_insn* insns;
        size_t count  = cs_disasm(csh, memory.data(), memory.size(), mapping.start, 0, &insns);

        std::vector<uint64_t> insn_sizes;
        insn_sizes.reserve((count * 4) / 64 + 1); // 4 bits per size value because intel x64 instructions are 0-15 bytes long

        uint64_t current{};
        for (size_t i{}; i < count; i++)
        {
            assert(insns[i].size < 16);
            current |= static_cast<uint64_t>(insns[i].size) << ((i % 16) * 4); // 16 sizes fit into 1 uint64_t, and each space is 4 bits wide

            if ((i + 1) % 16 == 0)
            {
                insn_sizes.push_back(current);
                current = 0;
            }
        }
        if (count % 16 != 0) // add extras
        {
            insn_sizes.push_back(current);
        }

        // Capstone cleanup
        cs_free(insns, count);
        cs_close(&csh);

        auto emplaced = insn_caches.emplace(mapping, std::move(insn_sizes));
        return emplaced.first->second; // Return the newly emplaced size vector
    }
};
