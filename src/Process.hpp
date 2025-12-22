#pragma once
#include <filesystem>

struct Process
{
    pid_t pid;
    std::string name;
    std::filesystem::path path;
};
