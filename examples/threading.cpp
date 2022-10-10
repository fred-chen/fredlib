/*
 * Copyright (c) 2022 Fred Chen
 *
 * This example file demonstrate how to use thread pools and
 * multi-thread facilities like SyncPoint, PausePoint, and FanInPoint etc.
 *
 * Note, all Fred library has the same namespace FRED::
 *
 * Created on Mon Oct 10 2022
 * Author: Fred Chen
 */

#include "threading.hpp"

using namespace FRED;

/**
 * @brief a SyncPoint object can be used to synchronize multiple threads.
 *
 */
void syncpoint_demo() {
    std::vector<std::thread> threads;
    const int nthreads = 10;

    SyncPoint sp(
        nthreads);  // declare a sync point and initialize it with nthreads

    auto f = [&sp] {  // the thread function
        sp.sync();    // threads sync here
                      // sync() will return when all threads have called sync
    };

    // start threads
    for (int i = 0; i < nthreads; i++) {
        threads.emplace_back(f);
    }

    for (auto& t : threads) {
        t.join();
    }

    // it's also possible to sync main thread with child threads
    // just add one more thread to syncpoint
    threads.clear();
    sp.set_threadnum(nthreads);  // child thread number is nthreads
    sp.add_thread();             // plus main thread
    // start threads
    for (int i = 0; i < nthreads; i++) {
        threads.emplace_back(f);
    }

    sp.sync();  // now main thread must call sync() too to allow all threads to
                // resume.

    for (auto& t : threads) {
        t.join();
    }
}

/**
 * @brief a PausePoint object can be used to pause multiple threads.
 *        typically, the main thread should call 'pause()' to pause all the
 *        child threads.
 *        And the child threads should call 'PauseIfRequired()' to check if it's
 *        required to pause.
 *
 */
void pausepoint_demo() {
    std::vector<std::thread> threads;
    const int nthreads = 10;

    PausePoint pp(
        nthreads);  // declare a PausePoint and initialize it with nthreads

    auto f = [&pp] {  // the thread function
        sleep_s(1);   // wait a while to make sure main thread calls pause()
                      // before we call pauseIfRequred()
                      // ! normally you don't have to do that, this is just for
                      // ! this example function.
        pp.pauseIfRequred();  // threads pause here if main thread called
                              // pause().
                              // pauseIfRequred() will return immediately if
                              // main thread didn't call pause().
        // do stuff ...
    };

    // start threads
    for (int i = 0; i < nthreads; i++) {
        threads.emplace_back(f);
    }
    pp.pause();  // main thread call pause() to pause all child threads
    // do stuff ...
    pp.resume();  // main thread call resume() to resume all child threads

    for (auto& t : threads) {
        t.join();
    }
}

/**
 * @brief a fan in point is actually a converge point.
 *        main thread uses a fan in point to wait for child to reach a special
 *        point, the exit point for example.
 *
 */
void faninpoint_demo() {
    std::vector<std::thread> threads;
    const int nthreads = 10;
    FanInPoint fp;

    auto f = [&fp] {  // the thread function
        sleep_s(1);   // wait a while to make sure main thread calls wait()
                      // before we call done()
        // do stuff ...

        fp.done();  // child threads call done() before they exit to notify the
                    // main thread that the child threads have reached the exit
                    // point.
    };

    // start threads
    for (int i = 0; i < nthreads; i++) {
        threads.emplace_back(f);
    }
    fp.wait();  // main thread call wait(). wait() will sleep the main thread
                // until all child threads called done().

    for (auto& t : threads) {
        t.join();
    }
}

/**
 * @brief A ThreadControl is actually a combination of SyncPoint, PausePoint,
 *        FanInPoint, plus a stop control point.
 *        Main thread uses a ThreadControl to do anything a SyncPoint,
 * PausePoint, or a FanInPoint can do. Actually a ThreadControl is a child class
 * of above classes.
 *
 */
void threadcontrol_demo() {
    std::vector<std::thread> threads;
    const int nthreads = 10;
    ThreadControl tc(nthreads);

    auto f = [&tc] {  // the thread function
        tc.sync();    // ThreadControl is-a SyncPoint
        sleep_s(1);   // wait a while to make sure main thread calls pause()
                      // before we call pauseIfRequred()
        tc.pauseIfRequred();  // ThreadControl is-a PausePoint

        // do stuff ...

        sleep_s(1);  // wait a while to make sure main thread calls wait()
                     // before we call done()
        while (true) {
            if (tc.stopRequired()) {  // plus, ThreadControl has a stop
                                      // indicator
                break;
            }
        }
        tc.done();  // ThreadControl is-a FanInPoint.
    };

    // start threadsfaninpoint_demo
    for (int i = 0; i < nthreads; i++) {
        threads.emplace_back(f);
    }
    tc.pause();
    tc.resume();
    tc.stop();
    for (auto& t : threads) {
        t.join();
    }
}

/**
 * @brief WorkerThreads is a thread pool meant for some long term tasks.
 *
 */
void workerthread_demo() {
    // This is a example thread function for WorkerThreads thread pool
    auto func = [](void* arg, bool& result, ThreadControl& tc) {
        while (true) {
            tc.pauseIfRequred();  // you can use a ThreadControl object (tc) to
                                  // accept control from main thread.

            if (tc.stopRequired()) {  // you can use the ThreadControl object
                                      // (tc) to determ whether or not the main
                                      // thread wants us to stop.
                break;
            }
        }

        result = true;  // result argument can be used to return the execution
                        // status.

        *(long*)arg = 5;  // the void* arg can be customized with any meanings.

        tc.done();  // notify the waiting threads your job is done if it's
                    // desired. the main thread relys on all child threads to
                    // call done() before WorkThread::wait() can return.
        tc.sync();  // you can use the ThreadControl object to sync with other
                    // threads.
    };

    long factor = 100;
    const int nthreads = 10;
    bool result = false;

    WorkerThreads pool1 = WorkerThreads(func, (void*)&factor, result,
                                        nthreads);  // start thread pool

    // you can pause/resume, sync, and stop a thread pool
    pool1.pause();
    pool1.resume();
    pool1.stop();
}

/**
 * @brief A generic thread pool takes a task from a function queue
 *        and execute them in a pool of threads.
 *
 */
void genericthreadpool_demo() {
    const int nthreads = 10;
    GenericThreadPool pool(nthreads);
    int assignMe = 0;

    auto callback = [&assignMe] {
        std::cout << "callback of genericthreadpool_demo: " << assignMe
                  << std::endl;  // 11
    };

    // push a lambda function without(or with) parameters to thread pool.
    // the function will be popped and executed later by one of the threads in
    // the pool.
    pool.push_function([&assignMe, &callback] {
        assignMe = 11;
        callback();
    });

    // you can pause/resume a pool
    pool.pause();
    pool.resume();

    // you can increase number of threads for performance
    pool.add_thread(2);

    // you can get current thread number
    int num = pool.get_numthreads();
    std::cout << "number of threads: " << num << std::endl;

    // you can stop the pool
    pool.stop();
    num = pool.get_numthreads();
    std::cout << "number of threads after stop(): " << num << std::endl;
}

int main() {
    syncpoint_demo();
    pausepoint_demo();
    faninpoint_demo();
    threadcontrol_demo();
    workerthread_demo();
    genericthreadpool_demo();
}