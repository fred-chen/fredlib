/*
 * Copyright (c) 2022 Fred Chen
 *
 * Created on Thu Sep 29 2022
 * Author: Fred Chen
 */

#pragma once

#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>

namespace FRED {

std::mutex g_mu;

/** a switch to turn on/off time printing on stdout */
bool g_printtime = false;
void setprinttime(bool prt = false) { g_printtime = prt; }

template <typename... Args>
std::string string_format(const std::string& format, Args... args) {
    int size_s = std::snprintf(nullptr, 0, format.c_str(), args...) +
                 1;  // Extra space for '\0'
    if (size_s <= 0) {
        throw std::runtime_error("Error during formatting.");
    }
    auto size = static_cast<size_t>(size_s);
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(),
                       buf.get() + size - 1);  // We don't want the '\0' inside
}

/** thread safe printf function  */
template <typename... Args>
void safe_stdout(std::string format, Args... args) {
    std::string t("");
    thread_local static std::chrono::steady_clock::time_point last_print_time =
        std::chrono::steady_clock::now();
    if (g_printtime) {
        t = string_format(
            "(+%.4dms) ",
            (std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::steady_clock::now() - last_print_time))
                .count());
    }
    std::string s =
        t + string_format(std::string("%s") + format, "[x] ", args...);

    std::lock_guard<std::mutex> locker(g_mu);
    std::cout << s << std::endl;
    last_print_time = std::chrono::steady_clock::now();
}

}  // namespace FRED