// src/memory/context_builder.cpp — YOU IMPLEMENT. Spec §4.4.

#include "memory/context_builder.h"
#include "util/min_heap.h"

ContextPlan select_context(const std::vector<ContextItem>& items,
                           std::size_t budget, std::size_t keep_recent) {
    std::size_t items_size = items.size();                                                                      // gets the size of the vector of ContextItems
    std::size_t first_recent;                                                                                   // creates a variable for the index of the oldest item in the recent group

    if(keep_recent >= items_size){
        first_recent = 0;                                                                                       // sets first_recent as 0 if keep_recent is bigger than the size of the vector of ContextItems (first case to handle)
    }
    else{
        first_recent = items_size - keep_recent;                                                                // sets first_recent as the size of the vector of ContextItems minus keep_recent.
    }

    std::size_t total = 0;                                                                                      // sets variable for the total tokens in items vector

    for (const ContextItem& item : items){                                                                      // counts tokens from every item in items vector
        total += item.tokens;
    }

    std::vector<bool> keep(items_size, true);                                                                   // creates a boolean vector with size of items_size and fill them with true
    MinHeap<std::pair<int, std::size_t>> heap;                                                                  // creates a minheap of pairs of data (integers for role rank and sizes for the index in items), which holds items for dropping
    if (total > budget) {                                                                                       // if the total is more than the budget, push all of the unprotected items onto the minheap
        for (std::size_t i = 0; i < items_size; i++) {
            bool is_protected = items[i].role == Role::System || items[i].pinned || i >= first_recent;
            if (!is_protected) heap.push({drop_rank(items[i].role), i});
        }
    }

    while (total > budget && !heap.empty()) {                                                                   // if the total is more than the budget and the minheap isn't empty, then keep dropping the oldest index (top of the minheap)
        std::size_t i = heap.pop().second;
        keep[i] = false;                                                                                        // makes the popped index of the minheap false (dropped) in the keep boolean vector 
        total -= items[i].tokens;                                                                               // subtracts the dropped item’s tokens from the token total
    }

    ContextPlan plan;                                                                                           // creates new ContextPlan object (empty)
    for (std::size_t i = 0; i < items_size; i++) {                                                              // iterates through the the keep boolean vector and pushes the items onto plan.keep if the keep index is true
        if (keep[i]){
            plan.keep.push_back(i);
        }
    }
    plan.total_tokens = total;                                                                                  // stores the token count of what is kept after adding the items form the keep for-loop.
    plan.over_budget = total > budget;                                                                          // returns true to plan.over_buket if total is greater than budget, false otherwise
    return plan;                                                                                                // returns the plan object
}
