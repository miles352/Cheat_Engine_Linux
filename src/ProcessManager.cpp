#include "ProcessManager.hpp"

#include <fstream>

#include "imgui.h"

ProcessManager::ProcessManager() : updated(false)
{

}

void ProcessManager::draw()
{
    // Process information window
    ImGui::Begin("Process Information");


    if (ImGui::Button("Attach"))
    {

        ImGui::OpenPopup(PROCESS_POPUP_TITLE);
    }

    this->draw_select_modal();

    if (current.has_value())
    {
        ImGui::Text("Current process: %s", current->name.c_str());
    }




    ImGui::End();
}

void ProcessManager::draw_select_modal()
{
    if (ImGui::BeginPopupModal(PROCESS_POPUP_TITLE))
    {
        if (ImGui::Button("Refresh"))
        {
            processes.clear();
        }
        if (ImGui::BeginTable("process_list", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("PID");
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Path");
            ImGui::TableHeadersRow();

            if (processes.empty())
            {
                for (auto& dir : std::filesystem::directory_iterator{"/proc/"})
                {
                    std::ifstream status{dir.path() / "status"};
                    pid_t pid;
                    std::filesystem::path exe_path;
                    try
                    {
                        pid = std::stoi(dir.path().filename());
                        exe_path = std::filesystem::read_symlink(std::format("/proc/{}/exe", pid));
                    }
                    catch (...)
                    {
                        continue;
                    }


                    std::string line_str;
                    std::string key;
                    std::string name;
                    while (std::getline(status, line_str))
                    {
                        std::istringstream line{std::move(line_str)};
                        line >> key;
                        if (key == "Name:")
                        {
                            line >> name;
                            break;
                        }
                    }

                    processes.emplace_back(pid, std::move(name), std::move(exe_path));
                }
            }
            else
            {
                for (const Process& process : processes)
                {
                    ImGui::TableNextRow();

                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(std::format("{}", process.pid).c_str(), false, ImGuiSelectableFlags_SpanAllColumns))
                    {
                        current = process;
                        updated = true;
                        ImGui::CloseCurrentPopup();
                    }


                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(process.name.c_str());

                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(process.path.c_str());
                }
            }


            ImGui::EndTable();
        }

        ImGui::EndPopup();
    }
}
