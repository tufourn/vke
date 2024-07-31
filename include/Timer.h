#pragma once

#include <chrono>

class Timer {
public:
    float totalTime() const;

    float deltaTime() const;;

    void reset();

    void start();

    void stop();

    void tick();

private:
    double m_deltaTime;
    double m_pausedTime;

    std::chrono::steady_clock::time_point m_baseTime;
    std::chrono::steady_clock::time_point m_stopTime;
    std::chrono::steady_clock::time_point m_prevTime;
    std::chrono::steady_clock::time_point m_currTime;

    bool m_stopped;
};