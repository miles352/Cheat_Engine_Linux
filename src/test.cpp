

#include <iostream>
#include <bits/this_thread_sleep.h>
#include <print>
#include <thread>
#include <unistd.h>
#include <vector>

int main()
{
    printf("PID: %d\n", getpid());
    long a = 0x91236d9782FFF;
    int x = 5;
    long b = 0x91236d9782FFF;
    std::vector<std::thread> threads;
    while (true)
    {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1ms);
        int y;
        std::cin >> y;
        if (y < 0)
        {
            threads.emplace_back([](){});
            std::println("Created thread!");
        }
        else if (x == 1) break;
        else
        {
            x = y;
            std::printf("Number %d at %p", x, &x);
        }
    }

}
