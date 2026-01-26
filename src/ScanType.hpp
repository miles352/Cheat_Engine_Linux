#pragma once

#include <string>
#include <variant>

using ScanType = std::variant<int8_t, int16_t, int32_t, int64_t, float, double, std::string>;

inline size_t get_scantype_size(const ScanType& scan_type)
{
    return scan_type.visit([]<typename T>(const T&) -> size_t
    {
        if constexpr (std::is_same_v<T, std::string>)
        {
            // TODO: Implement strings
            // fallback value for now
            return 4;
        }
        else
        {
            return sizeof(T);
        }
    });
}