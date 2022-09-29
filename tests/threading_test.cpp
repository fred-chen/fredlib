#include "threading.hpp"

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "catch.hpp"
#include "safe_stdout.hpp"

using namespace FRED;

/**
 * put threads in sync
 *
 * the first thread must wait until all threads reach the sync point
 */
TEST_CASE("SyncPoint", "[threading]") {
    const int nthreads = 15;
    atomic_uint max_duration = 1000;
    std::vector<std::thread*> v;
    SyncPoint sp;

    sp.initialize(nthreads);

    for (int i = 1; i <= nthreads; i++) {
        std::thread* t = new std::thread(
            [i, &max_duration](SyncPoint& sp) {
                sleep_ms(i);
                auto dur = sp.sync();
                if (dur > max_duration) max_duration = dur;
            },
            std::ref(sp));
        v.push_back(t);
    }
    for (int i = 0; i < nthreads; i++) {
        v[i]->join();
        delete v[i];
    }
    REQUIRE(max_duration >= nthreads - 1);
}

/**
 * put threads in pause
 *
 * the main thread must wait for all child threads to pause
 * the child threads must resume when main thread calls resume
 */
TEST_CASE("PausePoint", "[threading]") {
    const int nthreads = 15;
    atomic_uint min_time = 1000;
    PausePoint ps;
    vector<std::thread*> v;
    SyncPoint sp(nthreads + 1);

    ps.initialize(nthreads);
    for (int i = 1; i <= nthreads; i++) {
        std::thread* t = new std::thread(
            [i, &min_time, &sp](PausePoint& ps) {
                sp.sync();
                sleep_ms(10);
                auto wait_time = ps.pauseIfRequred();
                if (wait_time < min_time) min_time = wait_time;
            },
            std::ref(ps));
        v.push_back(t);
    }

    sp.sync();
    // std::cout << "main thread asks to pause." << std::endl;
    ps.pause();
    // safe_stdout( "main thread thinks all children paused." );
    sleep_ms(10);
    // safe_stdout( "main thread asks to resume." );
    ps.resume();

    for (int i = 0; i < nthreads; i++) {
        v[i]->join();
        delete v[i];
    }

    REQUIRE(min_time >= 10);
}

/**
 * main thread must wait for all child threads to finish
 *
 */
TEST_CASE("FanInPoint", "threading") {
    FanInPoint waiter;
    vector<std::thread> v;

    const int nthreads = 15;

    for (int i = 1; i <= nthreads; i++) {
        waiter.add();
        v.emplace_back(
            [i](FanInPoint& w) {
                sleep_ms(i);
                w.done();
            },
            std::ref(waiter));
    }
    auto dur = waiter.wait();
    // safe_stdout("dur=%d ms", dur);

    REQUIRE(
        dur >=
        nthreads);  // main thread must wait for the longest child thread to end

    for (auto& t : v) {
        t.join();
    }
}