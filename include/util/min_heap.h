// include/util/min_heap.h
//
// YOU IMPLEMENT (this whole file; it is a template, so the code lives in the
// header). Spec §4.1.
//
// A binary MIN-heap stored in an array (a std::vector), plus top_k(), which
// uses a size-k min-heap to pick the k largest items in O(n log k).
//
// Rules for this file:
//   * Write sift-up and sift-down yourself.
//   * Do NOT use std::priority_queue, std::make_heap / push_heap / pop_heap /
//     sort_heap, std::sort / stable_sort / partial_sort / nth_element, or
//     std::set / multiset / map / multimap (full list: spec §7.3).
//   * std::vector is fine as the storage array; std::swap, std::reverse and
//     std::pair are fine too.
//
// `Less` is a "less-than" function object, exactly like the one std::sort
// takes: less(a, b) is true when a should come before b. The heap keeps the
// SMALLEST element (by less) at the top.

#pragma once
#include <algorithm>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

template <typename T, typename Less = std::less<T>>
class MinHeap {
public:
    explicit MinHeap(Less less = Less()) : less_(std::move(less)) {}

    // Adds value. O(log n).
    void push(T value) {
        data_.push_back(std::move(value));                                                                  // pushes value to the back of private data member data_
        sift_up(data_.size() - 1);                                                                          // sifts up the value if it is smaller than its parent node
    }

    // The smallest element. Throws std::out_of_range if empty. O(1).
    const T& top() const {
        if (data_.empty()) throw std::out_of_range("MinHeap::top on an empty heap");                        // throws std::out_of_range if data_ is empty
        return data_[0];                                                                                    // returns the top of the data vector (data_[0])
    }

    // Removes and returns the smallest element. Throws std::out_of_range if
    // empty. O(log n).
    T pop() {
        if (data_.empty()) throw std::out_of_range("MinHeap::pop on an empty heap");                        // throws std::out_of_range if data_ is empty
        T topData_ = std::move(data_[0]);                                                                   // saves the root node (top of data_)
        if (data_.size() > 1) {                                                                             // checks to see if data_ size is more than 1 to avoid repetitive actions
            data_[0] = std::move(data_.back());                                                             // moves the last element to the root
            data_.pop_back();                                                                               // shrinks the data_ vector
            sift_down(0);                                                                                   // sifts the root node down
        } else {
            data_.pop_back();                                                                               // pops out the root node if data_ only has a size of 1 (there is only a root node)
        }
        return topData_;                                                                                    // returns what got popped (the root node)
    }

    std::size_t size() const noexcept { return data_.size(); }
    bool        empty() const noexcept { return data_.empty(); }
    void        clear() noexcept { data_.clear(); }

private:
    // ===== YOUR HELPERS AND MEMBERS =====
    // Children of index i are 2i+1 and 2i+2; its parent is (i-1)/2.
    // Suggested: void sift_up(std::size_t i);  void sift_down(std::size_t i);
    // Clue: use std::size_t (not int) for indices, or -Wsign-compare warns
    // every time you compare with data_.size(). Careful: (i-1)/2 with i == 0
    // underflows, so sift_up must stop at i == 0.
    // Clue: in sift_down, the right child may not exist: check
    // 2*i+2 < data_.size() before reading it. Swap with the SMALLER child.

    void sift_up(std::size_t i) {                                                                           // sift up function for insert
        while (i > 0) {                                                                                     // while the index is more than 0
            std::size_t parent = (i - 1) / 2;                                                               // parent is just (index - 1) / 2
            if (!less_(data_[i], data_[parent])){                                                           // if data[i] is not less than its parent, then break out of while loop
                break;
            }
            std::swap(data_[i], data_[parent]);                                                             // swap data[i] and parent if data[i] is less than data[parent]
            i = parent;                                                                                     // make the current index the original parent index and keep continuing the loop
        }
    }

    void sift_down(std::size_t i) {                                                                         // sift down function to help with pop (removes root)
        std::size_t n = data_.size();                                                                       // gets the size of data_;
        while (((2 * i) + 1) < n) {                                                                         // while the child nodes of the current index is within the size of data_
            std::size_t childIndex = 2 * i + 1;                                                             // gets child node of current index
            if (((childIndex + 1) < n) && (less_(data_[childIndex + 1], data_[childIndex]))){               // adds 1 to child if child+1 is within data_ size limit and sibling of child node is less than the original child node
                childIndex += 1;
            }
            if (!less_(data_[childIndex], data_[i])){
                break;
            }
            std::swap(data_[i], data_[childIndex]);
            i = childIndex;
        }
    }

    std::vector<T> data_;
    Less           less_;
};

// Returns the k LARGEST items of `items` (by `less`), largest first.
//   * k == 0 or items empty -> empty result
//   * k >= items.size()     -> every item, largest first
// Must keep a MinHeap of at most k items (the k largest seen so far, with
// the smallest of them on top), so it runs in O(n log k) time and O(k) extra
// space. Items that compare equal may come out in any order.
template <typename T, typename Less = std::less<T>>
std::vector<T> top_k(const std::vector<T>& items, std::size_t k, Less less = Less()) {
    if (k == 0 || items.empty()) return {};                                                                 // returns nothing if k is 0 and items is empty
    MinHeap<T, Less> heap(less);                                                                            // creates MinHeap with T as type and less as functor
    for (const T& item : items) {                                                                           // for every reference item in reference list of items
        if (heap.size() < k) {                                                                              // if the heap's size is the wanted number of largest items, push item from items list onto heap
            heap.push(item);
        } 
        else if (less(heap.top(), item)) {                                                                  // if the heap has more than k items, then start getting replacing the root with the item from items list
            heap.pop();
            heap.push(item);
        }
    }
    std::vector<T> result;                                                                                  // create vector of type T for the result of the MinHeap push and pops
    while (!heap.empty()){                                                                                  // while the heap is not empty, pop items from the heap and push it onto result with the smallest node being first
        result.push_back(heap.pop());
    }                                                     
    std::reverse(result.begin(), result.end());                                                             // flip result vector
    return result;                                                                                          // return result vector
}
