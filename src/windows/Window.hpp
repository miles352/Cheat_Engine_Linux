#pragma once



class Window
{
public:
    enum WindowID
    {
        SCANNER,
        DEBUG,
        MEMORY_VIEWER,
        WINDOW_LENGTH
    };

    virtual ~Window() = default;
    virtual void draw() = 0;
};