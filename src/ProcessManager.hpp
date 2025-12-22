#pragma once
#include <filesystem>
#include <vector>

#include "Process.hpp"

class ProcessManager
{
public:

    ProcessManager();

    void draw();
    void draw_select_modal();

    std::vector<Process> processes;
    std::optional<Process> current;
    /** A flag to be checked to see if the process updated */
    bool updated;
private:
    static constexpr auto PROCESS_POPUP_TITLE = "Attach to Process";
};
