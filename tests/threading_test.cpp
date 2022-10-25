#define UNDER_TEST

#include "threading.hpp"

#include <chrono>
#include <iostream>
#include <limits>
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

    // boundaries
    waiter.initialize(0);
    auto start = timer_start();
    waiter.wait();  // should return immediately
    dur = ms_elapsed_since(start);
    REQUIRE(dur <= 10);
}

// This is a example thread function for WorkerThreads thread pool
void func(void* arg, bool& result, ThreadControl& tc) {
    int j = 0;
    for (long i = 1; i <= *(long*)(arg); i++) {
        tc.pauseIfRequred();  // you can use a ThreadControl object (tc) to
                              // accept control from main thread.

        if (tc.stopRequired()) {  // you can use the ThreadControl object (tc)
                                  // to determ whether or not the main thread
                                  // wants us to stop.
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
    REQUIRE(nthreads == pool2.get_numthreads());

    pool2.add_thread(f, (void*)&factor, result, 1);
    REQUIRE(nthreads + 1 == pool2.get_numthreads());

    ms = pool2.stop();  // ask and wait for all threads to stop (gracefully).
    REQUIRE(ms >= 10);  // threads should run at least 10ms.
}

TEST_CASE("GenericThreadPool", "threading") {
    const int nthreads = 10;
    GenericThreadPool pool(nthreads);
    FanInPoint fip(1);
    std::atomic_int result(0);

    REQUIRE(pool.get_numthreads() == nthreads);  // worker thread number

    /// a simple function to put into thread pool
    auto f = [&result](int n, FanInPoint& fip) {
        // the function to be called in thread pool
        // increase a number by n every execution
        std::atomic_int i = 0;
        result += n;
        fip.done();
    };
    pool.push_function(
        f, 3,
        std::ref(fip));  // * note the reference types must be wrapped in
                         // * std::ref() otherwise the std::function won't
                         // * receve it as a reference type
    fip.wait();
    REQUIRE(result == 3);

    fip.set_threadnum(1);
    pool.push_function(f, 6, std::ref(fip));
    fip.wait();
    REQUIRE(result == 9);

    /// mix types of functions
    int assignMe = 0;
    auto callback = [&assignMe] { REQUIRE(assignMe == 11); };

    fip.set_threadnum(1);
    pool.push_function(f, 1, std::ref(fip));  // a function with parameters
    pool.push_function([&assignMe, &callback] {
        assignMe = 11;
        callback();
    });  // a lambda function without parameters

    fip.wait();
    REQUIRE(result == 10);

    /// pause the pool
    pool.pause();  // wait until all worker threads paused
    fip.set_threadnum(1);
    pool.push_function(f, 1, std::ref(fip));  // a function with parameters
    sleep_s(1);
    REQUIRE(
        result ==
        10);  // the function shouldn't be executed because the pool is paused
    pool.resume();
    fip.wait();
    REQUIRE(result == 11);  // pool resumed, the result should be altered

    /// pool stop
    pool.stop();
    REQUIRE(pool.get_numthreads() == 0);  // worker thread number
}