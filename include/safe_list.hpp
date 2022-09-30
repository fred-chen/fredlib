/*
 * Copyright (c) 2022 Fred Chen
 *
 * This file includes implementation of intrusive linked lists.
 *
 * Created on Thu Sep 29 2022
 * Author: Fred Chen
 */

#pragma once

#include <atomic>
#include <mutex>
#include <thread>

#include "testonly.hpp"

namespace FRED {

class listHook;  // just a forward declaration

/**
 * @brief a interface to all safe queue implementation
 *
 */
template <typename NodeType = listHook>
class SafeList {
private:
    bool _empty() const { return mHead == nullptr && mTail == nullptr; }
    std::mutex& _pass_mutex() {
        // expose list mutex to outside, mainly for iterators
        return mListMutex;
    }

protected:
    TEST_ONLY(public:)
    mutable std::mutex mListMutex;  // the mutex to protect the whole list
    NodeType* mHead;  // point to the first node of the linked list
    NodeType* mTail;  // point to the last node of the linked list
    uint32_t mSize;   // number of elements
public:
    SafeList() : mHead(nullptr), mTail(nullptr), mSize(0) {}

    /**
     * @brief insert a new node in front of existingNode.
     *
     * @param existingNode, an existing node. nullptr if inserting to a new
     * list.
     * @param newNode, the new node that needs to be inserted
     * @return true, success
     * @return false, fail
     */
    bool insert(NodeType* existingNode, NodeType* newNode) {
        std::lock_guard<std::mutex> locker(mListMutex);
        // parameter check
        if (newNode == nullptr) {
            return false;
        } else if ((_empty() && existingNode != nullptr) ||
                   (!_empty() && existingNode == nullptr)) {
            return false;
        }

        // danger zone
        if (_empty()) {
            mHead = newNode;
            mTail = newNode;
        } else {
            newNode->link_before(existingNode);
            if (mHead == existingNode) {
                mHead = newNode;
            }
        }
        mSize++;

        return true;
    }

    /**
     * @brief remove a node from list
     *
     * @param node, the node that needs to be removed from list
     * @return true if success
     * @return false if fail
     */
    bool erase(NodeType* node) {
        std::lock_guard<std::mutex> locker(mListMutex);
        // sanity check
        if (node == nullptr) {
            return false;
        }

        if (node == mTail) {
            mTail = node->prev;
        }
        if (node == mHead) {
            mHead = node->next;
        }
        node->unlink();
        mSize--;
        return true;
    }

    /**
     * @brief push_back a new node to the end of list
     *
     * @param newNode is the node to be appended
     * @return true if success
     * @return false if fail
     */
    bool push_back(NodeType* newNode) {
        std::lock_guard<std::mutex> locker(mListMutex);
        // parameter check
        if (newNode == nullptr) {
            return false;
        }

        if (_empty()) {
            mHead = newNode;
            mTail = newNode;
        } else {
            newNode->link_after(mTail);
            mTail = newNode;
        }
        mSize++;
        return true;
    }
    bool push_front(NodeType* newNode) { return insert(mHead, newNode); }

    /**
     * @brief pop front node
     *
     * @return NodeType* is the popped front node
     */
    NodeType* pop_front() {
        std::lock_guard<std::mutex> locker(mListMutex);
        NodeType* front = nullptr;

        if (!_empty()) {
            front = mHead;
            mHead = mHead->next;
            mTail = mTail == front ? nullptr : mTail;  // last element removed
            front->unlink();
            mSize--;
        }
        return front;
    }

    /**
     * @brief pop tailing node
     *
     * @return NodeType* is the old tailing node
     */
    NodeType* pop_back() {
        std::lock_guard<std::mutex> locker(mListMutex);
        NodeType* back = nullptr;

        if (!_empty()) {
            back = mTail;
            mTail = mTail->prev;
            mHead = mHead == back ? nullptr : mHead;  // last element removed
            back->unlink();
            mSize--;
        }
        return back;
    }

    uint32_t size() const {
        std::lock_guard<std::mutex> locker(mListMutex);
        return mSize;
    }
    uint32_t empty() const {
        std::lock_guard<std::mutex> locker(mListMutex);
        return _empty();
    }

    class Iterator {
    private:
        NodeType* mCurrent;
        SafeList* mParent;

    public:
        // STL algorithm compliant
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = NodeType;
        using difference_type = std::ptrdiff_t;
        using pointer = NodeType*;
        using reference = NodeType&;

    public:
        Iterator(SafeList* parent = nullptr) : mParent(parent) {
            mCurrent = mParent ? mParent->mHead
                               : nullptr;  // create a null iterator by default
            if (!mCurrent) {
                mParent = nullptr;  // invalidate the iterator if list is empty
            }
        }
        Iterator(const Iterator& iter) {
            mCurrent = iter.mCurrent;
            mParent = iter.mParent;
        }

        /// operators
        Iterator& operator=(const Iterator& rhs) {
            mCurrent = rhs.mCurrent;
            mParent = rhs.mParent;
            return *this;
        }

        Iterator& operator++() {  // prefix ++
            std::lock_guard<std::mutex> locker(mParent->_pass_mutex());
            mCurrent = mCurrent->next;
            if (mCurrent == nullptr) {
                mParent =
                    nullptr;  // invalidate iterator if it reaches the boundary
            }
            return *this;
        }
        Iterator operator++(int) {  // postfix ++
            // ? note the postfix ++ returns another iterator by value
            Iterator temp = *this;
            operator++();
            return temp;
        }

        Iterator& operator--() {  // prefix --
            std::lock_guard<std::mutex> locker(mParent->_pass_mutex());
            mCurrent = mCurrent->prev;
            if (mCurrent == nullptr) {
                mParent =
                    nullptr;  // invalidate iterator if it reaches the boundary
            }
            return *this;
        }
        Iterator operator--(int) {
            // postfix --
            // ? note the postfix -- returns another iterator by value
            Iterator temp = *this;
            operator--();
            return temp;
        }

        friend bool operator==(const Iterator& lhs, const Iterator& rhs) {
            return lhs.mCurrent == rhs.mCurrent && lhs.mParent == rhs.mParent;
        }
        friend bool operator!=(const Iterator& lhs, const Iterator& rhs) {
            return !(lhs == rhs);
        }
        friend bool operator<(const Iterator& lhs, const Iterator& rhs) {
            // todo: find a better method other than memory address comparason
            // one simple but expensive idea is to traverse the list
            // and find out whether rhs is beond or behind the lhs
            return lhs.mCurrent < rhs.mCurrent;
        }
        friend bool operator>(const Iterator& lhs, const Iterator& rhs) {
            return rhs < lhs;
        }
        friend bool operator<=(const Iterator& lhs, const Iterator& rhs) {
            return !(lhs > rhs);
        }
        friend bool operator>=(const Iterator& lhs, const Iterator& rhs) {
            return !(lhs < rhs);
        }

        NodeType* operator->() { return mCurrent; }
        NodeType& operator*() { return *mCurrent; }

        bool is_valid() { return !(mParent == nullptr); }
    };

    Iterator begin() { return Iterator(this); }
    Iterator end() {
        return Iterator(nullptr);  // a null iterator
    }
};

/**
 * @brief this is a node of a liked list, it hooks a bigger object on to a
 * linked list. listHook is a part of the 'real' data structure. listHook hooks
 * the real data structure on to a linked list. the data structure needs to
 * include a listHook object in its definition. then the data structure can be
 * linked together. this is also abbreviated as 'intrusive linked list'.
 *
 * @tparam ParentObjType, the actual data object that contains this hook
 * @tparam the offset of this hook object inside the data object
 */
struct listHook {
    listHook* prev = nullptr;  // previous node
    listHook* next = nullptr;  // next node

    void unlink() {
        if (prev) prev->next = next;
        if (next) next->prev = prev;
        prev = nullptr;
        next = nullptr;
    }

    /**
     * @brief link this node after the preceding node
     *
     * @param prevNode
     */
    void link_after(listHook* prevNode) {
        prev = prevNode;
        next = prevNode->next;
        if (prev) prev->next = this;
        if (next) next->prev = this;
    }

    /**
     * @brief link this node after the preceding node
     *
     * @param nextNode
     */
    void link_before(listHook* nextNode) {
        prev = nextNode->prev;
        next = nextNode;
        if (prev) prev->next = this;
        if (next) next->prev = this;
    }
    void reset() {
        prev = nullptr;
        next = nullptr;
    }
};

/**
 * @brief a linked list implementing struct listHook.
 *        The list is thread safe.
 *
 * @tparam ParentObjType, the data type (that contains a list hook).
 * @tparam HookType, type of the list hook, typically a listHook type.
 * @tparam ParentObjType::*PtrToHook, the pointer to the hook object in the data
 *         object.
 */
template <typename ParentObjType, typename HookType,
          HookType ParentObjType::*PtrToHook>
class IntrusiveSafeList : public SafeList<HookType> {
    using ListType = IntrusiveSafeList<ParentObjType, HookType, PtrToHook>;

private:
    TEST_ONLY(public:)
    std::ptrdiff_t mOffset;  // the offset HookType to the head of ParentObjType

public:
    IntrusiveSafeList() {
        // address of PtrToHook from base address 0 (nullptr)
        // is the offset of the hook object in data object
        ParentObjType* temp = nullptr;
        mOffset = reinterpret_cast<std::ptrdiff_t>(&((*temp).*PtrToHook));
    }

    bool push_back(ParentObjType& obj) {
        return SafeList<HookType>::push_back(getHook(obj));
    }
    bool push_front(ParentObjType& obj) {
        return SafeList<HookType>::push_front(getHook(obj));
    }
    ParentObjType& pop_front() {
        HookType* hook = SafeList<HookType>::pop_front();
        return getParent(hook);
    }
    ParentObjType& pop_back() {
        HookType* hook = SafeList<HookType>::pop_back();
        return getParent(hook);
    }
    /**
     * @brief return the parent object of the hook node
     *
     * @param hook is a pointer to the hook inside the parent object
     * @return ParentObjType
     *
     */
    ParentObjType& getParent(HookType* hook) const {
        return *(
            (ParentObjType*)(reinterpret_cast<std::ptrdiff_t>(hook) - mOffset));
    }

    /**
     * @brief Get the Hook object from a parent object
     *
     * @param obj
     * @return HookType*
     */
    HookType* getHook(ParentObjType& obj) const { return &(obj.*PtrToHook); }

    bool empty() const { return SafeList<HookType>::empty(); }

    class Iterator {
        using BaseIterType = typename SafeList<HookType>::Iterator;
        using ParentType = ListType;

    private:
        BaseIterType _base_iter;
        ParentType* mParent;

    public:
        // STL algorithm compliant
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = ParentObjType;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;

    public:
        Iterator(ParentType* parent = nullptr) {
            mParent = parent;
            _base_iter = BaseIterType(mParent);
        }

        Iterator(const Iterator& iter) {
            mParent = iter.mParent;
            _base_iter = iter._base_iter;
        }

        /// operators
        Iterator& operator=(const Iterator& rhs) {
            _base_iter = rhs._base_iter;
            return *this;
        }

        Iterator& operator++() {  // prefix ++
            ++_base_iter;
            return *this;
        }
        Iterator operator++(int) {  // postfix ++
            Iterator temp = *this;
            ++_base_iter;
            return temp;
        }

        Iterator& operator--() {  // prefix --
            --_base_iter;
            return *this;
        }
        Iterator operator--(int) {
            Iterator temp = *this;
            --_base_iter;
            return temp;
        }

        friend bool operator==(const Iterator& lhs, const Iterator& rhs) {
            return lhs._base_iter == rhs._base_iter;
        }
        friend bool operator!=(const Iterator& lhs, const Iterator& rhs) {
            return !(lhs == rhs);
        }
        friend bool operator<(const Iterator& lhs, const Iterator& rhs) {
            return lhs._base_iter < rhs._base_iter;
        }
        friend bool operator>(const Iterator& lhs, const Iterator& rhs) {
            return rhs < lhs;
        }
        friend bool operator<=(const Iterator& lhs, const Iterator& rhs) {
            return !(lhs > rhs);
        }
        friend bool operator>=(const Iterator& lhs, const Iterator& rhs) {
            return !(lhs < rhs);
        }

        value_type* operator->() {
            return &mParent->getParent((&(*_base_iter)));
        }
        value_type& operator*() { return mParent->getParent((&(*_base_iter))); }

        bool is_valid() { return _base_iter.is_valid(); }
    };

    Iterator begin() { return Iterator(this); }
    Iterator end() { return Iterator(nullptr); }
};

}  // namespace FRED