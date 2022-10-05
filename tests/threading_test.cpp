#define UNDER_TEST

#include "threading.hpp"

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "catch.hpp"
#include "micros.hpp"
#include "safe_stdout.hpp"
#include "time.hpp"

using namespace FRED;
using namespace std;

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
    ps.pause();
    sleep_ms(10);
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

// This is a example thread function for WorkerThreads thread pool
void func(void* arg, bool& result, ThreadControl& tc) {
    int j = 0;
    for (long i = 1; i <= *(long*)(arg); i++) {
        tc.pauseIfRequred();  // you can use a ThreadControl object (tc) to
                              // accept control from main thread.

        if (tc.stopRequired()) {  // you can use the ThreadControl object (tc)
                                  // to determ whether or not the main thread wants us
                                  // to stop.
            break;
        }
        j += i;
    }

    tc.sync();  // you can use the ThreadControl object to sync with other
                // threads.

    result =
        true;  // result argument can be used to return the execution status.

    *(long*)arg = j;  // the void* arg can be customized with any meanings.

    tc.done();  // notify the waiting threads your job is done if it's desired.
                // the main thread relys on all child threads to call
                // done() before WorkThread::wait() can return.
}

TEST_CASE("WorkerThreads", "threading") {
    long factor = 100;
    const int nthreads = 10;
    bool result = false;
    long j = 0;

    for (long i = 1; i <= factor; i++) {
        j += i;
    }

    /// test basic function
    WorkerThreads pool1 = WorkerThreads(func, (void*)&factor, result,
                                        1);  // start thread pool
    pool1.wait();
    REQUIRE(result == true);  // to verify the thread pool actually run
    REQUIRE(factor == j);     // to verify the thread pool actually run

    /// test thread controls
    ThreadRoutine f = [](void* arg, bool& result, ThreadControl& tc) {
        tc.pauseIfRequred();  // pause all threads if required by main thread.
        tc.sync();            // sync all threads
        sleep_ms(50);
        result = true;
        *(long*)arg = 6;  // just return a wild number for main thread to check.
        tc.done();        // notify main thread when done.
    };

    WorkerThreads pool = WorkerThreads(f, (void*)&factor, result,
                                       nthreads);  // start thread pool

    auto ms = pool.wait();  // wait for all threads to call tc.done()
    REQUIRE(ms >= 50);      // the main thread should wait for at least 50ms.
    REQUIRE(factor == 6);

    /// test stopper
    f = [](void* arg, bool& result, ThreadControl& tc) {
        UNUSED(arg);
        UNUSED(result);
        while (true) {
            sleep_ms(10);
            if (tc.stopRequired()) {
                break;
            }
        }  // a dead loop until the main thread asks to break.
        tc.done();
    };

    WorkerThreads pool2 = WorkerThreads(f, (void*)&factor, result,
                                        nthreads);  // start thread pool
    ms = pool2.stop();  // ask and wait for all threads to stop (gracefully).
    REQUIRE(ms >= 10);  // threads should run at least 10ms.
}