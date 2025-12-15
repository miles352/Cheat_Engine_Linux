#include "Application.hpp"

int main()
{
    Application app;

    while (!app.done)
    {
        app.draw_frame();
    }

    return 0;
}
