/*
 * Copyright (c) 2022 Fred Chen
 *
 * This example file demonstrate how to use SafeList and IntrusiveSafeList.
 * Note, all Fred library has the same namespace FRED::
 *
 * Created on Fri Sep 30 2022
 * Author: Fred Chen
 */

#include <algorithm>
#include <iostream>

#include "safe_list.hpp"

using namespace FRED;

/**
 * @brief a demo to demonstrate SafeList
 *        SafeList is a intrusive, thread safe list.
 *        SafeList chains listHook together.
 *        Where listHook is the element type of a linked list.
 */
void safe_list_demo() {
    ///
    /// to use SafeList, you can either derive your own class from listHook.
    /// then use hook to chain your objects onto a SafeList.
    /// or just include a listHook member in your data types.
    ///

    /// @brief include a listHook member in your data types.
    ///
    struct MyData {  // define your own data type
        int data;
        listHook hook;  // the hook of IntrusiveSafeList
        MyData(int i) : data(i) {}
    };
    MyData data1{1}, data2{2},
        data3{3};  // your data that need to be chained on SafeList
    SafeList<listHook>
        list;  // define a SafeList, with listHook as the hook type

    // put your hooks into the list
    list.push_back(&(data1.hook));
    list.push_back(&(data2.hook));
    list.push_back(&(data3.hook));
    std::cout << "list size is " << list.size() << std::endl;  // 3

    // get your hooks from the list
    auto p_hook1 = list.pop_front();
    list.pop_front();
    list.pop_front();
    std::cout << "list size is " << list.size() << std::endl;  // 0

    // but this way, you must figure out how to convert hooks back to your
    // object type ... complex and boring.
    listHook MyData::*ptr = &MyData::hook;
    std::ptrdiff_t offset =
        reinterpret_cast<std::ptrdiff_t>(&(((MyData*)0)->*ptr));
    MyData* p_data1 =
        (MyData*)(reinterpret_cast<std::ptrdiff_t>(p_hook1) - offset);
    std::cout << "data of data1 is " << p_data1->data << std::endl;  // 1

    /// @brief easier to inherit from listHook
    ///        this way you don't bother convert the hook object back to your
    ///        own data type.
    ///
    struct MyDataDerived : public listHook {  // derived from listHook
        int data;
    };
    MyDataDerived data;
    data.data = 3;

    list.push_back(&data);
    auto p_data = (MyDataDerived*)list.pop_front();
    std::cout << "data of MyDataDerived is " << p_data->data << std::endl;  // 3
}

/**
 * @brief Demonstrate how to use a IntrusiveSafeList.
 *        A IntrusiveSafeList is just a SafeList.
 *        But you can chain your own data type (with a listHook member) onto
 *        IntrusiveSafeList.
 *        And you don't have to convert the hook back to your data type,
 *        IntrusiveSafeList takes care of that for you.
 *
 */
void intrusive_safe_list_demo() {
    struct MyDataType {
        int data;
        listHook hook;  // the hook of IntrusiveSafeList
    };
    MyDataType data{5, {}};

    IntrusiveSafeList<MyDataType, listHook, &MyDataType::hook> list;
    list.push_back(data);
    MyDataType* pdata = list.pop_front();
    std::cout << "data of MyDataType is " << pdata->data << std::endl;  // 5

    /// you can also use iterator to traverse a IntrusiveSafeList
    list.push_back(data);
    for (auto& v : list) {
        std::cout << "data of MyDataType is " << v.data << std::endl;
    }

    std::all_of(std::begin(list), std::end(list), [](MyDataType& v) {
        std::cout << "data of MyDataType is " << v.data << std::endl;
        return true;
    });
}

int main() {
    safe_list_demo();
    intrusive_safe_list_demo();
    return true;
}