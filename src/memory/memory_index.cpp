// src/memory/memory_index.cpp — YOU IMPLEMENT. Spec §4.3.

#include "memory/memory_index.h"

#include <algorithm>
#include <utility>

MemoryIndex::~MemoryIndex() {
    destroy(root_);                                                                         // uses helper function to delete the BST
}

bool MemoryIndex::insert(MemoryRecord rec) {
    Node* parent = nullptr;                                                                 // sets the current parent to the parent of the root (null)
    Node* current = root_;                                                                  // sets the current node to the root of the BST
    while (current != nullptr) {                                                            // iterate through the BST
        if (rec.id == current->rec.id){                                                     // returns false if node with the same id is already in the BST
            return false;
        }
        parent = current;                                                                   // makes the parent the current node
        if(rec.id < current->rec.id){                                                       // makes current node its left child node if the id of rec is less than the current node's id
            current = current->left;
        }
        else{                                                                               // makes current node its right child node if the id of rec is more than the current node's id
            current = current->right;
        }
    }
    Node* node = new Node{std::move(rec), nullptr, nullptr};                                // creates new node based on rec and makes its left and right child nodes null
    if (parent == nullptr){                                                                 // makes root_ the newly created node if the BST is empty (root has no parent)
        root_ = node;
    }
    else if (node->rec.id < parent->rec.id){                                                // makes left child of parent the newly created node if the new node's id is less than the parent's id
        parent->left = node;
    }
    else{                                                                                   // makes right child of parent the newly created node if the new node's id is more than the parent's id
        parent->right = node;
    }
    count_++;                                                                               // increment insertion count by 1
    return true;                                                                            // return true if node was successfully inserted
}

const MemoryRecord* MemoryIndex::find(std::uint64_t id) const {
    const Node* current = root_;                                                            // creates current node pointing to the root of the BST
    while (current != nullptr) {                                                            // iterates through the BST to see if the node with the given id can be found
        if (id == current->rec.id){                                                         // returns the address of rec if the correct node is found
            return &current->rec;
        }
        if(id < current->rec.id){                                                           // makes curernt node pointer point to its left child if the given id is less than the current node's id to look in the left subtree
            current = current->left;
        }
        else{                                                                               // makes curernt node pointer point to its right child if the given id is less than the current node's id to look in the right subtree
            current = current->right;
        }
    }
    return nullptr;                                                                         // returns null if the node with the given id can be found
}

bool MemoryIndex::remove(std::uint64_t id) {
    bool removed = false;                                                                   // creates default case for removed, which will be returned later
    root_ = remove_node(root_, id, &removed);                                               // uses helper function to recursively find the node that needs to be deleted
    if (removed){                                                                           // if anything gets deleted, then decrement count by 1
        count_--;
    }
    return removed;                                                                         // return the removed
}

std::size_t MemoryIndex::size() const noexcept { return count_; }                           // returns the size of the BST (which is just count_)

std::size_t MemoryIndex::height() const {
    return height_of(root_);                                                                // uses helper function to recursively find the height of the BST
}

void MemoryIndex::for_each_in_order(const std::function<void(const MemoryRecord&)>& fn) const {
    in_order(root_, fn);                                                                    // uses helper function to recursively get the BST in an in-order sequence
}

void MemoryIndex::clear() {
    destroy(root_);                                                                         // uses a helper function to get rid of the entire BST
    root_ = nullptr;                                                                        // resets the root_ to nullptr
    count_ = 0;                                                                             // resets count_ to 0
}

void MemoryIndex::destroy(Node* n) {
    if (n == nullptr){                                                                      // base case of recursive function destroy (gets to a leaf node)
        return;
    }
    destroy(n->left);                                                                       // recursivelly call destroy to get rid of the left subtree
    destroy(n->right);                                                                      // recursivelly call destroy to get rid of the right subtree
    delete n;                                                                               // deletes the root node of a subtree once its children gets destroyed
}

std::size_t MemoryIndex::height_of(const Node* n) {
    if (n == nullptr){                                                                      // base case of recursive function height_of (gets to a leaf node)
        return 0;
    }
    return std::max(height_of(n->left), height_of(n->right)) + 1;                           // recursively called height_of on the left and right subtrees to see the max of the 2 and add 1 to get the height of the given node
}

void MemoryIndex::in_order(const Node* n, const std::function<void(const MemoryRecord&)>& fn) {
    if (n == nullptr){                                                                      // base case of recursive function in_order (gets to a leaf node)
        return;
    }
    in_order(n->left, fn);                                                                  // recursivelly call in_order to get the left subtree in-order first into fn
    fn(n->rec);                                                                             // gets the node into fn
    in_order(n->right, fn);                                                                 // recursivelly call in_order to get the right subtree in-order first into fn
}

MemoryIndex::Node* MemoryIndex::remove_node(Node* n, std::uint64_t id, bool* removed) {
    if (n == nullptr){                                                                      // returns null if the given node is nonexistent
        return nullptr;
    }
    if (id < n->rec.id) {                                                                   // recursively calls remove_node if id is less than the given node's id
        n->left = remove_node(n->left, id, removed);
        return n;
    }
    if (id > n->rec.id) {                                                                   // recursively calls remove_node if id is more than the given node's id
        n->right = remove_node(n->right, id, removed);
        return n;
    }
    *removed = true;                                                                        // makes the given removed pointer's value true
    if (n->left == nullptr) {                                                               // if the node's left child is null, then return the right child of the given node and delete the node  (handles the case of the node being the leaf node or having only a right child node)
        Node* child = n->right;
        delete n;
        return child;
    }
    if (n->right == nullptr) {                                                              // if the node's right child is null, then return the elft child of the given node and delete the node (handles the case of the node being the leaf node or having only a left child node)
        Node* child = n->left;
        delete n;
        return child;
    }
    Node* successor = n->right;                                                             // makes the successor node the right subtree of the given node (handles the case of 2 child nodes)   
    while (successor->left != nullptr){                                                     // iteratively find the left most node of the right subtree (successor)
        successor = successor->left;
    }
    n->rec = successor->rec;                                                                // makes the given node pointer point to the successor node
    n->right = remove_node(n->right, successor->rec.id, removed);                           // remove the successor node through recursion
    return n;                                                                               // return the removed node
}
