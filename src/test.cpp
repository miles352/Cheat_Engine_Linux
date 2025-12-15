

#include <iostream>
#include <bits/this_thread_sleep.h>
#include <print>
#include <unistd.h>

int main()
{
    printf("PID: %d\n", getpid());
    long a = 0x91236d9782FFF;
    float x = 5;
    long b = 0x91236d9782FFF;
    while (true)
    {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1ms);
        std::cin >> x;
        std::printf("Number %f at %p", x, &x);
        if (x == 1) break;
    }

}
