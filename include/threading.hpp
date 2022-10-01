/*
 * Copyright (c) 2022 Fred Chen
 *
 * This file includes implementations of multi-threading facilities like
 * syncpoint, waiter, pauser, thread pool etc.
 *
 * Created on Thu Sep 29 2022
 * Author: Fred Chen
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include "time.hpp"

namespace FRED {

using std::atomic;
using std::condition_variable;
using std::mutex;
using std::unique_lock;

/**
 * @brief simple sync point, threads waits at sync points
 */
class SyncPoint {
private:
    atomic<int>
        mSyncedThreads;    // number of threads that have reached the syncpoint
    int mExpectedThreads;  // expected number of threads that reached the
                           // syncpoint
    condition_variable
        mCvSync;       // the condition variable where threads check sync at
    mutex mSyncMutex;  // protects the condition variables
public:
    SyncPoint(const SyncPoint&) = delete;
    SyncPoint& operator=(const SyncPoint&) = delete;
    SyncPoint(int expected = 1)
        : mSyncedThreads(0), mExpectedThreads(expected) {}

    /**
     * @brief   function should be called by threads that need to sync
     * sleep until mSyncedThreads == mExpectedThreads (all threads
     * synced).
     * @return  n milliseconds waited
     */
    size_t sync() {
        auto start = timer_start();
        mSyncedThreads++;
        if (mSyncedThreads == mExpectedThreads) {
            // wakeup all if all threads have reached this sync point
            mCvSync.notify_all();
        } else {
            unique_lock locker(mSyncMutex);
            mCvSync.wait(locker, [this]() {
                return mSyncedThreads == mExpectedThreads;
            });
        }
        return ms_elapsed_since(start);
    }

    /**
     * @brief set expected thread number
     *
     * @param expected is the thread number that reach the sync point
     */
    void initialize(int expected) {
        mSyncedThreads = 0;
        mExpectedThreads = expected;
    }

    void add_thread(int num) { mExpectedThreads += num; }
    void set_threadnum(int num) { mExpectedThreads = num; }
};

/**
 * @brief Threads fan in at a FanInPoint
 *
 */
class FanInPoint {
private:
    atomic<int> mExpectedThreads;  // number of threads expected
    condition_variable
        mCvFanIn;       // the condition variable where threads check sync at
    mutex mFanInMutex;  // protects the condition variables
public:
    FanInPoint(int expected = 0) : mExpectedThreads(expected) {}

    /**
     * @brief child threads call done() when they finish their job
     *
     */
    void done() {
        mExpectedThreads--;
        mCvFanIn.notify_one();
    }

    /**
     * @brief parent thread call wait() to wait for all children
     * wait() returns when number of children done meets expectation
     *
     * @return size_t n milliseconds waited
     */
    size_t wait() {
        /// wait will cause caller to sleep until mExpectedThreads == 0
        auto start = timer_start();
        unique_lock<mutex> locker(mFanInMutex);
        mCvFanIn.wait(locker, [this]() { return mExpectedThreads == 0; });
        return ms_elapsed_since(start);
    }

    /**
     * @brief increase expected child thread number
     *
     */
    void add() { mExpectedThreads++; }

    /**
     * @brief set expected thread number
     *
     * @param expected is the thread number that reach the sync point
     */
    void initialize(int expected) { mExpectedThreads = expected; }
    void add_thread(int num) { mExpectedThreads += num; }
    void set_threadnum(int num) { mExpectedThreads = num; }
};

/**
 * @brief threads stop at PausePoint if required
 */
class PausePoint {
private:
    atomic<bool> mPause;  // true: pause, false: resume
    atomic<int>
        mPausedThreads;  // number of threads that have achieved the pause state
    int mExpectedThreads;  // expected number of threads that reached the
                           // syncpoint
    condition_variable
        mCvPause;  // the condition variable where threads check pause at
    condition_variable mCvPaused;  // the condition variable where main thread
                                   // checks paused number
    mutex mPauseMutex;             // used by the condition variables
public:
    PausePoint(size_t expected = 1)
        : mPause(false), mPausedThreads(0), mExpectedThreads(expected) {}

    /**
     * @brief   the function should be called by threads that need to stop
     *          sleep(pause) if required.
     * @return  n milliseconds waited
     */
    size_t pauseIfRequred() {
        auto start = timer_start();
        unique_lock<mutex> locker(mPauseMutex);

        if (mPause == true) {
            mPausedThreads++;
            mCvPaused.notify_one();
            mCvPause.wait(locker, [this]() { return !mPause; });
        }

        return ms_elapsed_since(start);
    }

    /**
     * @brief   pause all threads, the function should be called by main thread
     * in order to ask child threads to pause sleep until all threads have
     * paused
     * @return  n milliseconds waited
     */
    size_t pause() {
        auto start = timer_start();
        mPause = true;
        unique_lock<mutex> locker(mPauseMutex);
        mCvPaused.wait(locker,
                       [this] { return mPausedThreads == mExpectedThreads; });

        return ms_elapsed_since(start);
    }

    /**
     * @brief resume all threads, called by main threads
     *
     */
    void resume() {
        mPause = false;
        mCvPause.notify_all();
    }

    /**
     * @brief set expected thread number
     *
     * @param expected is the thread number that reach the sync point
     */
    void initialize(int expected) {
        mPausedThreads = 0;
        mExpectedThreads = expected;
    }
    void add_thread(int num) { mExpectedThreads += num; }
    void set_threadnum(int num) { mExpectedThreads = num; }
};

/**
 * @brief a single class that can be a sync, fan in, and pause point.
 *        also, for convinience, an control flag is include for stop control.
 */
class ThreadControl : public SyncPoint, public FanInPoint, public PausePoint {
private:
    std::atomic_bool _stopper;  // a sync object actually asing children to stop

public:
    ThreadControl(int expected = 0)
        : SyncPoint(expected),
          FanInPoint(expected),
          PausePoint(expected),
          _stopper(false) {}

    void initialize(int expected) {
        SyncPoint::initialize(expected);
        FanInPoint::initialize(expected);
        PausePoint::initialize(expected);
    }

    /**
     * @brief main thread calls stop to notify the child threads asking them to
     *        stop.
     *        main thread waits until all threads stop.
     *        child threads should call FanInPoint::done() function to notify
     *        the main thread.
     *
     * @return size_t n milliseconds waited
     *
     */
    size_t stop() {
        _stopper = true;
        return FanInPoint::wait();
    }

    /**
     * @brief child threads call this function to determ if the _stopper is set.
     *
     * @return true
     * @return false
     */
    bool stopRequired() { return _stopper; }

    void add_thread(int num) {
        SyncPoint::add_thread(num);
        FanInPoint::add_thread(num);
        PausePoint::add_thread(num);
    }
    void set_threadnum(int num) {
        SyncPoint::set_threadnum(num);
        FanInPoint::set_threadnum(num);
        PausePoint::set_threadnum(num);
    }
};

typedef void (*ThreadRoutine)(void* arg, bool& result, ThreadControl& tc);
/**
 * @brief A thread pool that run ThreadRoutine(s).
 *        WorkerThreads is meant for some long term tasks.
 *        WorkerThreads takes a ThreadRoutine function as its target.
 */
class WorkerThreads {
private:
    std::vector<std::thread> _vecThreads;  // the list of threads
    ThreadControl _tc;

public:
    WorkerThreads(const WorkerThreads&) = delete;             // no copy
    WorkerThreads& operator=(const WorkerThreads&) = delete;  // no assignment

    /**
     * @brief Construct a new Worker Threads object
     *
     * @param visitor is the thread function
     * @param arg any pointer passed into thread function
     * @param result a bool varible to accept thread function result
     * @param nthreads how many threads should be created, default is hardware
     *                 core number.
     * @param scale final thread number = nthreads * scale
     */
    WorkerThreads(ThreadRoutine visitor, void* arg, bool& result,
                  int nthreads = 0, double scale = 1.0) {
        if (nthreads == 0) {
            nthreads = std::thread::hardware_concurrency();
        }
        nthreads *= scale;
        _tc.initialize(nthreads);
        for (int i = 0; i < nthreads; i++) {
            _vecThreads.emplace_back(visitor, arg, std::ref(result),
                                     std::ref(_tc));
        }
    }

    void join() {
        for (auto& v : _vecThreads) {
            v.join();
        }
    }

    size_t pause() { return _tc.pause(); }
    size_t sync() { return _tc.sync(); }
    void resume() { _tc.resume(); }
    size_t wait() {
        size_t waited = _tc.wait();
        join();
        return waited;
    }
    /**
     * @brief ask and wait for all threads to stop.
     *
     * @return size_t n milliseconds waited.
     */
    size_t stop() {
        size_t waited = _tc.stop();
        join();
        return waited;
    }

    /**
     * @brief add threads to pool. The new threads will share the same
     *        ThreadControl with existing ones.
     *
     * @param visitor is the thread function
     * @param arg any pointer passed into thread function
     * @param tc a control object for threads
     * @param result a bool varible to accept thread function result
     * @param nthreads how many threads should be created, default is hardware
     *                 core number
     * @param scale final thread number = nthreads * scale
     *
     */
    void add_thread(ThreadRoutine& visitor, void* arg, bool& result,
                    int nthreads = 0, double scale = 1.0) {
        if (nthreads == 0) {
            nthreads = std::thread::hardware_concurrency();
        }
        nthreads *= scale;
        _tc.add_thread(nthreads);
        for (int i = 0; i < nthreads; i++) {
            _vecThreads.emplace_back(visitor, arg, std::ref(result),
                                     std::ref(_tc));
        }
    }
};

}  // namespace FRED
