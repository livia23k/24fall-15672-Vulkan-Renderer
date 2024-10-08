#include "Source/Tools/Timer.hpp"

Timer::Timer()
{
    t = 0.f;
    tmax = __FLT_MAX__;
    paused = false;
}

Timer::Timer(float tmax)
{
    t = 0.f;
    this->tmax = tmax;
    paused = false;
}

void Timer::reset()
{
    t = 0.f;
    paused = true;
}

void Timer::pause_or_resume()
{
    paused = !paused;
}

void Timer::update(float dt)
{
    if (!paused)
        t += dt;
    
    if (t < 0)
    {
        t = 0;
    }
    if (t >= tmax)
    {
        t = std::fmod(t, tmax);
    }
}

