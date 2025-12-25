#pragma once

class Window
{
public:
    virtual ~Window() = default;
    virtual void draw() = 0;
};