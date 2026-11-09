// include/util/lru_cache.h
//
// YOU IMPLEMENT (this whole file; it is a template). Spec §4.2.
//
// A fixed-capacity cache that evicts the Least Recently Used entry.
// Both get() and put() must be O(1) (expected), so it is built from two
// structures that point at the same nodes:
//
//   std::unordered_map<Key, Node*>   finds a key's node in O(1)
//   a doubly linked list of Nodes    keeps recency order:
//        head_ = most recently used ... tail_ = least recently used
//
// Rules for this file:
//   * Write the doubly linked list yourself (Node with prev/next pointers,
//     new/delete inside this class only). Do NOT use std::list, std::deque
//     or std::forward_list (don't even #include their headers), or a
//     std::vector you shift around (spec §7.3).
//   * std::unordered_map is allowed (and expected) for the key -> Node* index.
//   * No leaks, no double frees: ASan runs on every test.

#pragma once
#include <cstddef>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

template <typename Key, typename Value, typename Hash = std::hash<Key>>
class LruCache {
public:
    struct Stats {
        std::size_t hits = 0;        // get() found the key
        std::size_t misses = 0;      // get() did not find the key
        std::size_t evictions = 0;   // put() removed the LRU entry to make room
    };

    // A cache holding at most `capacity` entries. Capacity 0 is legal: such a
    // cache never stores anything (every get() is a miss).
    explicit LruCache(std::size_t capacity) : capacity_(capacity) {}

    // Frees every node.
    ~LruCache() { clear(); }

    // Copying a cache of raw-pointer nodes is not needed; forbid it.
    LruCache(const LruCache&) = delete;
    LruCache& operator=(const LruCache&) = delete;

    // Hit: marks the entry most recently used, counts a hit, and returns a
    //      pointer to its value (valid until the next put/erase/clear).
    // Miss: counts a miss and returns nullptr.
    const Value* get(const Key& key) {
        auto it = map_.find(key);                                                           // looks up key in map_
        if (it == map_.end()) {                                                             // if key found is at the end of map_, then increases misses field of stats_ by 1 and return nullptr
            ++stats_.misses;                                                                
            return nullptr;                                                                 
        }
        ++stats_.hits;                                                                      // increases hits field of stats_ by 1 
        move_to_front(it->second);                                                          // moves second field of it to the front of map_
        return &it->second->value;                                                          // returns the reference to the value of it->second, which was moved to the front
    }

    // Key present: replaces its value and marks it most recently used (no
    //   eviction, no hit/miss counted).
    // Key absent: if the cache is full, evicts the least recently used entry
    //   (counting one eviction), then inserts the new entry as most recently
    //   used. With capacity 0, does nothing.
    void put(const Key& key, Value value) {
        if (capacity_ == 0){                                                                // does nothing if the capacity is 0
            return;
        }
        auto it = map_.find(key);                                                           // looks up key in map_ to see if it exists
        if (it != map_.end()) {                                                             // if the value doesn't exist, move the value into the value field of it->second
            it->second->value = std::move(value);
            move_to_front(it->second);                                                      // moves it->second to the front of map_ and returns
            return;
        }
        if (map_.size() >= capacity_) {                                                     // if the size of the map_ is full,
            Node* evicted = tail_;                                                          // then unlink the list used entry, delete it, and then increment stats_.evictions by 1
            unlink(evicted);
            map_.erase(evicted->key);
            delete evicted;
            ++stats_.evictions;
        }
        Node* node = new Node{key, std::move(value), nullptr, nullptr};                     // creates a new node with the key and value parameters
        push_front(node);                                                                   // pushes that node to the front of the doubly linked list
        map_.emplace(key, node);                                                            // adds node with key to map_
    }

    // True if the key is cached. Does NOT change recency or stats.
    bool contains(const Key& key) const {
        return map_.find(key) != map_.end();                                                // returns true if the key can be found in map_. False if not
    }

    // Removes the key if present (not an eviction). Returns whether it was.
    bool erase(const Key& key) {
        auto it = map_.find(key);                                                           // finds key in map_
        if (it == map_.end()){                                                              // returns false if key doesn't exist
            return false;
        }
        Node* node = it->second;                                                            // creates a node pointer of value it->second
        unlink(node);                                                                       // unlinks the node from the doubly linked list
        map_.erase(it);                                                                     // removes it (key) from map_
        delete node;                                                                        // deletes the node pointer from the heap and returns true
        return true;
    }

    // Removes every entry. Stats are kept.
    void clear() {
        Node* n = head_;                                                                    // creates a node pointer to the head of the unordered map
        while (n != nullptr) {                                                              // traverses the unordered map to delete each node of it
            Node* next = n->next;
            delete n;
            n = next;
        }
        head_ = nullptr;                                                                    // sets the head_ node poniter field to null
        tail_ = nullptr;                                                                    // sets the tail_ node poniter field to null
        map_.clear();                                                                       // clears map_
    }

    std::size_t size() const noexcept { return map_.size(); }
    std::size_t capacity() const noexcept { return capacity_; }
    Stats       stats() const noexcept { return stats_; }

    // Keys from most to least recently used, by walking the list from head_.
    // Used by the tests to check your recency order.
    std::vector<Key> keys_mru_to_lru() const {
        std::vector<Key> out;
        for (Node* n = head_; n != nullptr; n = n->next) out.push_back(n->key);
        return out;
    }

private:
    struct Node {
        Key   key;
        Value value;
        Node* prev;
        Node* next;
    };

    // ===== YOUR HELPERS AND MEMBERS =====
    // Suggested helpers: unlink(Node*), push_front(Node*), move_to_front(Node*).
    // Clue for unlink(n): four cases. If n has a prev, prev->next skips n,
    // otherwise n was head_ and head_ moves to n->next. Likewise, if n has
    // a next, next->prev skips n, otherwise n was tail_ and tail_ moves to
    // n->prev. Unlinking the only node leaves head_ == tail_ == nullptr.
    // Spec Appendix B lists every pointer write.

    void unlink(Node* n) {                                                                  // unlinks a node from the doubly linked list
        if (n->prev != nullptr){                                                            // skips n if n has a previous (not a head)
            n->prev->next = n->next;                                                        
        }                                    
        else{                                                                               // skips n, which is the head of the doubly linked list
            head_ = n->next;
        }
        if (n->next != nullptr){                                                            // skips n if n has a next (not a tail)
            n->next->prev = n->prev;
        }
        else{                                                                               // skips n, which is the tail of the doubly linked list
            tail_ = n->prev;
        }
        n->prev = nullptr;                                                                  // makes previous node of n nullptr
        n->next = nullptr;                                                                  // makes previous node of n nullptr
    }

    void push_front(Node* n) {                                                              // pushes node to the front of the doubly linked list
        n->prev = nullptr;
        n->next = head_;
        if (head_ != nullptr){                                                              // if doubly linked list is not empty, make the current head's previous field point to new head (n)
            head_->prev = n;
        }
        else{                                                                               // if doubly linked list is empty, make the tail pointer point to the node head (n)
            tail_ = n;
        }
        head_ = n;                                                                          // makes the head n for all cases
    }

    void move_to_front(Node* n) {                                                           // makes node n the most recently added node
        if (n == head_){                                                                    // if node n is the head of map_, then return because it's already at the front
            return;
        }
        unlink(n);                                                                          // unlink node n from map_
        push_front(n);                                                                      // move node n to the front of map_
    }

    std::unordered_map<Key, Node*, Hash> map_;
    Node*       head_ = nullptr;
    Node*       tail_ = nullptr;
    std::size_t capacity_;
    Stats       stats_;
};
