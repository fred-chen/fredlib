
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "catch.hpp"

#define UNDER_TEST

#include "safe_list.hpp"
#include "safe_stdout.hpp"

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

    IntrusiveSafeList<DataObj, listHook, &DataObj::hook>
        list;  // a Intrusive List
    IntrusiveSafeList<DataObj2, listHook, &DataObj2::hook>
        list2;  // a Intrusive List

    // offset correctness
    REQUIRE(list.mOffset ==
            4 + 4 + 8);  // sizeof(int) + 4B padding + sizeof(double)
    REQUIRE(list2.mOffset ==
            40 + 8);  // sizeof(int) * 10 + 0 padding + sizeof(double)

    DataObj objs[numElements];

    // check data values
    int idx = 0;
    for (auto& v : objs) {
        v.data = idx++;
        list.push_back(v);
    }
    REQUIRE(list.size() == numElements);

    for (idx -= 1; idx >= 0; idx--) {
        DataObj obj = list.pop_back();
        REQUIRE(obj.data == idx);
    }
    REQUIRE(list.size() == 0);

    idx = 0;
    for (auto& v : objs) {
        v.data = idx++;
        list.push_front(v);
    }
    REQUIRE(list.size() == numElements);

    for (idx -= 1; idx >= 0; idx--) {
        DataObj obj = list.pop_front();
        REQUIRE(obj.data == idx);
    }
    REQUIRE(list.size() == 0);
}