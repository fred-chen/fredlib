
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <thread>
#include <vector>

#include "catch.hpp"

#define UNDER_TEST

#include "safe_list.hpp"
#include "safe_stdout.hpp"
#include "time.hpp"

using namespace FRED;

TEST_CASE("listHook", "list") {
    listHook node1;
    listHook node2;
    listHook node3;

    // pointers are nullptr initially
    REQUIRE(node1.prev == nullptr);
    REQUIRE(node1.next == nullptr);

    // linkage correctness
    node3.link_after(&node1);  // node1-node3
    REQUIRE(node1.next == &node3);
    REQUIRE(node1.prev == nullptr);
    REQUIRE(node3.prev == &node1);
    REQUIRE(node3.next == nullptr);

    node2.link_before(&node3);  // node1-node2-node3
    REQUIRE(node1.next == &node2);
    REQUIRE(node1.prev == nullptr);
    REQUIRE(node2.prev == &node1);
    REQUIRE(node2.next == &node3);
    REQUIRE(node3.prev == &node2);
    REQUIRE(node3.next == nullptr);

    // correct removement
    node2.unlink();  // node1-node3
    REQUIRE(node1.next == &node3);
    REQUIRE(node1.prev == nullptr);
    REQUIRE(node3.prev == &node1);
    REQUIRE(node3.next == nullptr);
    REQUIRE(node2.prev == nullptr);
    REQUIRE(node2.next == nullptr);

    node2.link_after(&node1);  // node1-node2-node3
    node1.unlink();            // node2-node3
    REQUIRE(node2.next == &node3);
    REQUIRE(node2.prev == nullptr);
    REQUIRE(node3.prev == &node2);
    REQUIRE(node3.next == nullptr);

    node1.link_before(&node2);  // node1-node2-node3
    node3.unlink();             // node1-node2
    REQUIRE(node2.next == nullptr);
    REQUIRE(node2.prev == &node1);
    REQUIRE(node1.prev == nullptr);
    REQUIRE(node1.next == &node2);
    REQUIRE(node3.prev == nullptr);
    REQUIRE(node3.next == nullptr);
}

TEST_CASE("SafeList", "list") {
    listHook node1;
    listHook node2;
    listHook node3;

    SafeList<> list;

    // linkage correctness
    list.push_back(&node1);  // add to empty list, header->node1
    REQUIRE(node1.next == nullptr);
    REQUIRE(node1.prev == nullptr);
    REQUIRE(list.mHead == &node1);
    REQUIRE(list.mTail == &node1);
    REQUIRE(list.size() == 1);

    list.push_back(&node3);  // header->node1-node3
    REQUIRE(node1.next == &node3);
    REQUIRE(node1.prev == nullptr);
    REQUIRE(node3.next == nullptr);
    REQUIRE(node3.prev == &node1);
    REQUIRE(list.mHead == &node1);
    REQUIRE(list.mTail == &node3);
    REQUIRE(list.size() == 2);

    list.insert(&node3, &node2);  // header->node1-node2-node3
    REQUIRE(node1.next == &node2);
    REQUIRE(node1.prev == nullptr);
    REQUIRE(node2.next == &node3);
    REQUIRE(node2.prev == &node1);
    REQUIRE(node3.next == nullptr);
    REQUIRE(node3.prev == &node2);
    REQUIRE(list.mHead == &node1);
    REQUIRE(list.mTail == &node3);
    REQUIRE(list.size() == 3);

    list.pop_front();
    list.pop_front();
    list.pop_front();
    REQUIRE(list.size() == 0);
    REQUIRE(list.empty());

    list.push_back(&node1);
    list.push_back(&node2);
    list.push_back(&node3);

    listHook* temp =
        list.pop_front();  // node1 popped, header->node2-node3 left
    REQUIRE(temp == &node1);
    REQUIRE(node1.next == nullptr);
    REQUIRE(node1.prev == nullptr);
    REQUIRE(node2.next == &node3);
    REQUIRE(node2.prev == nullptr);
    REQUIRE(node3.next == nullptr);
    REQUIRE(node3.prev == &node2);
    REQUIRE(list.mHead == &node2);
    REQUIRE(list.mTail == &node3);
    REQUIRE(list.size() == 2);

    temp = list.pop_back();  // node3 popped, only header->node2 left
    REQUIRE(temp == &node3);
    REQUIRE(node2.next == nullptr);
    REQUIRE(node2.prev == nullptr);
    REQUIRE(node3.next == nullptr);
    REQUIRE(node3.prev == nullptr);
    REQUIRE(list.mHead == &node2);
    REQUIRE(list.mTail == &node2);
    REQUIRE(list.size() == 1);

    list.push_back(&node3);  // header->node2-node3
    REQUIRE(node2.next == &node3);
    REQUIRE(node2.prev == nullptr);
    REQUIRE(node3.next == nullptr);
    REQUIRE(node3.prev == &node2);
    REQUIRE(list.mHead == &node2);
    REQUIRE(list.mTail == &node3);
    REQUIRE(list.size() == 2);

    list.push_front(&node1);  // header->node1->node2-node3
    REQUIRE(node1.next == &node2);
    REQUIRE(node1.prev == nullptr);
    REQUIRE(node2.next == &node3);
    REQUIRE(node2.prev == &node1);
    REQUIRE(node3.next == nullptr);
    REQUIRE(node3.prev == &node2);
    REQUIRE(list.mHead == &node1);
    REQUIRE(list.mTail == &node3);
    REQUIRE(list.size() == 3);
}

TEST_CASE("SafeList Iterator", "list") {
    SafeList<listHook> list;

    listHook node1;
    listHook node2;
    listHook node3;

    list.push_back(&node1);
    list.push_back(&node2);
    list.push_back(&node3);  // header->node1-node2-node3

    // iterator resolvation
    auto iter = list.begin();  // iter->node1
    REQUIRE(iter->next == &node2);
    REQUIRE(&(*iter) == &node1);  // deref iter should return node1 object

    // traverse
    REQUIRE((iter++)->next ==
            &node2);  // iter++ returns a new iterator pointing to node1
    REQUIRE(iter->next == &node3);       // iter itself is pointing to node2
    REQUIRE((++iter)->next == nullptr);  // iter->node3
    REQUIRE(&(*iter) == &node3);      // deref iter should return node3 object
    REQUIRE(&(*(--iter)) == &node2);  // --iter put it to previous position
    REQUIRE(
        &(*(iter--)) ==
        &node2);  // iter-- returns a new iterator pointing to current position
    REQUIRE(&(*iter) ==
            &node1);  // iter itself is pointing to previous location

    // boundaries
    REQUIRE((--iter) ==
            SafeList<listHook>::Iterator());  // cross the beginning boundary,
                                              // iter is invalidated
    REQUIRE(iter.is_valid() == false);

    // STL algorithms
    // any of the nodes in list should be node1 or node2 or node3
    auto f = [&node1, &node2, &node3](listHook& node) {
        return &node == &node1 || &node == &node2 || &node == &node3;
    };
    REQUIRE(std::any_of(std::begin(list), std::end(list), f));

    // locate a node in list
    iter = std::find_if(
        std::begin(list), std::end(list),
        [&node1, &node2, &node3](listHook& node) { return &node == &node2; });
    REQUIRE(&(*iter) == &node2);
    REQUIRE(iter->next == &node3);
}

TEST_CASE("IntrusiveSafeList", "list") {
    const int numElements = 100;
    struct DataObj {
        int data;
        double data1;
        listHook hook;
    };

    struct DataObj2 {
        int data[10];
        double data1;
        listHook hook;
    };

    DataObj obj1;
    obj1.data = 1;
    DataObj2 obj2;
    obj2.data1 = 2.0;

    IntrusiveSafeList<DataObj, listHook, &DataObj::hook>
        list;  // a Intrusive List
    IntrusiveSafeList<DataObj2, listHook, &DataObj2::hook>
        list2;  // a Intrusive List

    list.push_front(obj1);
    list2.push_back(obj2);

    // offset correctness
    REQUIRE(list.mOffset ==
            4 + 4 + 8);  // sizeof(int) + 4B padding + sizeof(double)
    REQUIRE(list2.mOffset ==
            40 + 8);  // sizeof(int) * 10 + 0 padding + sizeof(double)
    REQUIRE(list.pop_back() == &obj1);
    REQUIRE(list2.pop_front() == &obj2);

    REQUIRE(list.size() == 0);
    REQUIRE(list2.size() == 0);
    REQUIRE(list.empty());
    REQUIRE(list2.empty());

    // push pop sequences
    list.push_front(obj1);
    list.pop_front();
    list.push_front(obj1);
    list.pop_back();
    list.push_back(obj1);
    list.pop_back();
    list.push_back(obj1);
    list.pop_front();

    REQUIRE(list.size() == 0);
    REQUIRE(list2.size() == 0);
    REQUIRE(list.empty());
    REQUIRE(list2.empty());

    // check data value correctness
    DataObj objs[numElements];
    int idx = 0;
    for (auto& v : objs) {
        v.data = idx++;
        list.push_back(v);
    }
    REQUIRE(list.size() == numElements);

    for (idx -= 1; idx >= 0; idx--) {
        DataObj* obj = list.pop_back();
        REQUIRE(obj->data == idx);
    }
    REQUIRE(list.size() == 0);

    idx = 0;
    for (auto& v : objs) {
        v.data = idx++;
        list.push_front(v);
    }
    REQUIRE(list.size() == numElements);

    for (idx -= 1; idx >= 0; idx--) {
        DataObj* obj = list.pop_front();
        REQUIRE(obj->data == idx);
    }
    REQUIRE(list.size() == 0);
}

TEST_CASE("IntrusiveSafeList Iterator", "list") {
    struct DataObj {
        int data;
        double data1;
        listHook hook;
        DataObj(int i, double f) : data(i), data1(f) {}
    };

    DataObj node1{1, 1.1};
    DataObj node2{2, 2.2};
    DataObj node3(3, 3.3);

    IntrusiveSafeList<DataObj, listHook, &DataObj::hook> list;

    list.push_back(node1);
    list.push_back(node2);
    list.push_back(node3);  // header->node1-node2-node3

    // iterator resolvation
    auto iter = list.begin();     // iter->node1
    REQUIRE(&(*iter) == &node1);  // deref iter should return node1 object
    REQUIRE((*iter).data == 1);
    REQUIRE((*iter).data1 == 1.1);

    // traverse
    REQUIRE((iter++)->data == 1);
    REQUIRE(iter->data == 2);
    REQUIRE((++iter)->data == 3);
    REQUIRE(&(*iter) == &node3);      // deref iter should return node3 object
    REQUIRE(&(*(--iter)) == &node2);  // --iter put it to previous
    REQUIRE(iter->data == 2);
    REQUIRE(
        &(*(iter--)) ==
        &node2);  // iter-- returns a new iterator pointing to current position
    REQUIRE(&(*iter) ==
            &node1);  // iter itself is pointing to previous location

    // boundaries
    REQUIRE(
        (--iter) ==
        IntrusiveSafeList<DataObj, listHook,
                          &DataObj::hook>::Iterator());  // cross the beginning
                                                         // boundary, iter is
                                                         // invalidated
    REQUIRE(iter.is_valid() == false);

    // empty list
    REQUIRE(list.size() == 3);  // header->node1-node2-node3
    list.pop_back();
    list.pop_front();
    list.pop_back();
    REQUIRE(list.size() == 0);  // empty
    auto start = timer_start();
    list.pop_front(1000);
    REQUIRE(ms_elapsed_since(start) > 990);
    REQUIRE(list.begin() == list.end());

    // STL algorithms
    // any of the nodes in list should be node1 or node2 or node3
    list.push_back(node1);
    list.push_back(node2);
    list.push_back(node3);  // header->node1-node2-node3

    auto f = [&node1, &node2, &node3](DataObj& node) {
        return (DataObj*)&node != &node1 && (DataObj*)&node != &node2 &&
               (DataObj*)&node != &node3;
    };
    REQUIRE(std::any_of(std::begin(list), std::end(list), f) == false);

    // locate a node in list
    iter = std::find_if(std::begin(list), std::end(list),
                        [&node1, &node2, &node3](DataObj& node) {
                            return (DataObj*)&node == &node2;
                        });
    REQUIRE(&(*iter) == &node2);
    REQUIRE(iter->data == 2);

    // check the data
    for (auto& v : list) {
        REQUIRE(v.data1 == (double)v.data + ((double)v.data) / 10);
    }

    // iterator on empty list shouldn't cause trouble
    list.pop_front();
    list.pop_front();
    list.pop_front();
    REQUIRE(list.size() == 0);  // empty
    int n = 0;
    for (auto& v : list) {
        UNUSED(v);
        n++;
    }
    REQUIRE(n == 0);
}

TEST_CASE("SafeList Thread Safety", "list") {
    char n = 0;

    // a self-check structure
    struct Data {
        char data[100];
        long sum = 0;
        listHook hook;
        long checksum() {
            long s = 0;
            for (auto& v : data) {
                s += v;
            }
            return s;
        }
        long calsum() {
            sum = checksum();
            return sum;
        }
        bool verify() { return sum == checksum(); }
    };

    // thread function to put Data structures into a safe list
    const int num_data = 10;
    auto f = [&n, &num_data](
                 IntrusiveSafeList<Data, listHook, &Data::hook>& q) -> bool {
        for (int j = 0; j < num_data; j++) {
            Data* d = new Data();
            for (int i = 0; i < 100; n = n % 128) {
                d->data[i++] = n++;
            }
            d->calsum();
            REQUIRE(d->verify());
            if (!q.push_back(*d)) {
                return false;
            }
        }
        return true;
    };

    // create threads to push data into a list simultaneously
    const int numthreads = 100;

    IntrusiveSafeList<Data, listHook, &Data::hook> q;
    std::future<bool> futs[numthreads];

    for (int i = 0; i < numthreads; i++) {
        futs[i] = std::async(std::launch::async, f, std::ref(q));
    }

    for (auto& fut : futs) {
        fut.get();
    }

    // list consistency and data integrity
    REQUIRE(q.size() == numthreads * num_data);
    for (auto& v : q) {
        REQUIRE(v.verify());
    }
}