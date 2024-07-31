#include "Timer.h"

float Timer::totalTime() const {
    if (m_stopped) {
        return static_cast<float>(
                std::chrono::duration_cast<std::chrono::microseconds>(m_stopTime - m_baseTime).count() / 1e6f -
                m_pausedTime
        );
    } else {
        return static_cast<float>(
                std::chrono::duration_cast<std::chrono::microseconds>(m_currTime - m_baseTime).count() / 1e6f -
                m_pausedTime
        );
    }
}

void Timer::reset() {
    auto now = std::chrono::steady_clock::now();

    m_baseTime = now;
    m_prevTime = now;
    m_pausedTime = 0.0;
    m_stopped = false;
}

void Timer::start() {
    auto now = std::chrono::steady_clock::now();
    if (m_stopped) {
        m_pausedTime += std::chrono::duration_cast<std::chrono::microseconds>(now - m_stopTime).count() / 1e6f;

        m_prevTime = now;
        m_stopped = false;
    }
}

void Timer::stop() {
    if (!m_stopped) {
        m_stopTime = std::chrono::steady_clock::now();
        m_stopped = true;
    }
}

void Timer::tick() {
    if (m_stopped) {
        m_deltaTime = 0.0;
        return;
    }

    m_currTime = std::chrono::steady_clock::now();

    auto elapsed = m_currTime - m_prevTime;

    m_deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count() / 1e6f;

    m_prevTime = m_currTime;

    if (m_deltaTime < 0.0) {
        m_deltaTime = 0.0;
    }
}

float Timer::deltaTime() const {
    return static_cast<float>(m_deltaTime);
}
