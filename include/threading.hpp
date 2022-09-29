/*
 * Copyright (c) 2022 Fred Chen
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
    atomic<uint32_t>
        mSyncedThreads;  // number of threads that have reached the syncpoint
    uint32_t mExpectedThreads;  // expected number of threads that reached the
                                // syncpoint
    condition_variable
        mCvSync;       // the condition variable where threads check sync at
    mutex mSyncMutex;  // protects the condition variables
public:
    SyncPoint(const SyncPoint&) = delete;
    SyncPoint& operator=(const SyncPoint&) = delete;
    SyncPoint(uint32_t expected = 1)
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
    void initialize(uint32_t expected) {
        mSyncedThreads = 0;
        mExpectedThreads = expected;
    }
};

/**
 * @brief Threads fan in at a FanInPoint
 *
 */
class FanInPoint {
private:
    atomic<uint32_t> mExpectedThreads;  // number of threads expected
    condition_variable
        mCvFanIn;       // the condition variable where threads check sync at
    mutex mFanInMutex;  // protects the condition variables
public:
    FanInPoint(uint32_t expected = 0) : mExpectedThreads(expected) {}

    /**
     * @brief child threads call done() when they finish their job
     *
     */
    void done() {
        mExpectedThreads--;
        mCvFanIn.notify_one();
    }

    /**
     * @brief   parent thread call wait() to wait for all children
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
    void initialize(uint32_t expected) { mExpectedThreads = expected; }
};

/**
 * @brief threads stop at PausePoint if required
 */
class PausePoint {
private:
    atomic<bool> mPause;  // true: pause, false: resume
    atomic<uint32_t>
        mPausedThreads;  // number of threads that have achieved the pause state
    uint32_t mExpectedThreads;  // expected number of threads that reached the
                                // syncpoint
    condition_variable
        mCvPause;  // the condition variable where threads check pause at
    condition_variable mCvPaused;  // the condition variable where main thread
                                   // checks paused number
    mutex mPauseMutex;             // protects the condition variables
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

        if (mPause) {
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
    void initialize(uint32_t expected) {
        mPausedThreads = 0;
        mExpectedThreads = expected;
    }
};

/**
 * @brief single class that can be a sync, fan in, and pause point
 */
class ThreadControl : public SyncPoint, FanInPoint, PausePoint {};

typedef void (*thread_routine)(void* arg, ThreadControl& tc, bool& result);
using ThreadRoutine = std::function<thread_routine>;
/**
 * @brief a thread pool that run ThreadRoutine(s)
 */
class WorkerThreads {};

};  // namespace FRED
