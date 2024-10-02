#pragma once

#include <cmath>

struct Timer
{
    Timer();
    Timer(float tmax);
    ~Timer() = default;

    bool paused;
    float t;
    float tmax;

    void reset();
    void pause_or_resume();
    void update(float dt);
};