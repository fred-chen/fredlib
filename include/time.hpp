/*
 * Copyright (c) 2022 Fred Chen
 *
 * Created on Thu Sep 29 2022
 * Author: Fred Chen
 */

#pragma once

#include <chrono>
#include <thread>

namespace FRED {

using std::chrono::duration;
using std::chrono::time_point;

/**
 * @brief return current time_point from steady clock
 *
 * @return time_point<std::chrono::steady_clock>
 */
time_point<std::chrono::steady_clock> timer_start() {
    return std::chrono::steady_clock::now();
}

/**
 * @brief calculate seconds passed since 'start'
 *
 * @param start the starting time_point
 * @return seconds that has elapsed since starting time_point
 */
size_t secconds_elapsed_since(time_point<std::chrono::steady_clock> start) {
    time_point end = std::chrono::steady_clock::now();
    auto dur = end - start;
    return std::chrono::duration_cast<std::chrono::seconds>(dur).count();
}
size_t ms_elapsed_since(time_point<std::chrono::steady_clock> start) {
    time_point end = std::chrono::steady_clock::now();
    auto dur = end - start;
    return std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
}

/**
 * @brief sleep for sec seconds
 *
 * @param sec seconds
 */
inline void sleep_s(int sec) {
    std::this_thread::sleep_for(std::chrono::seconds(sec));
}
inline void sleep_ms(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
inline void sleep_us(int us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}
inline void sleep_ns(int ns) {
    std::this_thread::sleep_for(std::chrono::nanoseconds(ns));
}

}  // namespace FRED