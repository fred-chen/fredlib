/*
 * Copyright (c) 2022 Fred Chen
 *
 * Created on Thu Sep 29 2022
 * Author: Fred Chen
 */


#pragma once

#include <chrono>

using namespace std;
using chrono::duration;
using chrono::time_point;

/**
 * @brief return current time_point from steady clock
 *
 * @return time_point<std::chrono::steady_clock>
 */
time_point<std::chrono::steady_clock> timer_start() {
    return chrono::steady_clock::now();
}

/**
 * @brief calculate seconds passed since 'start'
 *
 * @param start the starting time_point
 * @return seconds that has elapsed since starting time_point
 */
size_t secconds_elapsed_since(time_point<std::chrono::steady_clock> start) {
    time_point end = chrono::steady_clock::now();
    auto dur = end - start;
    return chrono::duration_cast<chrono::seconds>(dur).count();
}
size_t ms_elapsed_since(time_point<std::chrono::steady_clock> start) {
    time_point end = chrono::steady_clock::now();
    auto dur = end - start;
    return chrono::duration_cast<chrono::milliseconds>(dur).count();
}

/**
 * @brief sleep for sec seconds
 *
 * @param sec seconds
 */
inline void sleep_s(int sec) {
    std::this_thread::sleep_for(chrono::seconds(sec));
}
inline void sleep_ms(int ms) {
    std::this_thread::sleep_for(chrono::milliseconds(ms));
}
inline void sleep_us(int us) {
    std::this_thread::sleep_for(chrono::microseconds(us));
}
inline void sleep_ns(int ns) {
    std::this_thread::sleep_for(chrono::nanoseconds(ns));
}