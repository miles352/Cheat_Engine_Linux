#pragma once



class Window
{
public:
    enum WindowID
    {
        SCANNER,
        DEBUG,
        WINDOW_LENGTH
    };

    virtual ~Window() = default;
    virtual void draw() = 0;
};