# Final Project Design Log
**Author:** Eric Pham · **P2 code used:** <instructor reference> · **P3 code used:** <instructor reference>
**Tools used:** The AI assistant I used is Claude's Sonnet model. I used it to understand the Project 3 spec, the overall context of this project AI miniharness, and the C++ standard libary functions. Specifically, I used it to understand how to use std::pair with the unordered_map for the MinHeap and in skill_index.cpp, how to use standard library linked list functions, for better context of how the final project files and functions work with the provided files, how the insert function in memory_index.cpp manipulates the non-reference parameter, how to use MinHeap<std::pair<int,std::size_t>> for context_builder.cpp, when to not use auto for loops in context_builder.cpp, how the programs_ vector should function with the other parts of the mini harness for policies, how the provided types in policy.h work, how to iterate through patterns_ with a reference string in policies.cpp, how to use std::move with a standard library push function for the add function of policy_chain.cpp, what the enum means for decision::kind:deny, how to return an empty vecotr in skill_index.cpp, general data structures, understanding what the test cases need/want, how to use a random number generator for the test file, how to do the counting time feature for the test file, and using std::unit64_t on the bst_remove_leaf_and_one_child test. I also used it to see if my code made sense, debugging the test file, how to optimize the test file with helper functions, and learning to put void(req) for RateLimitPolicy::evaluate in policies.cpp for the compiler to compile without warnings. 

## 1. LRU cache: why get and put are O(1)
For a get() hit on a middle node n, move_to_front(n) calls unlink(n) and then push_front(n). unlink writes "n->prev->next = n->next", "n->next->prev = n->prev", "n->prev = nullpt"r and "n->next = nullptr". push_front writes "n->prev = nullptr", "n->next = head_", "head_->prev = n" and "head_ = n". tail_ does not change throughout these. For a put() that evicts, the victim is tail_. unlink(victim) writes "victim->prev->next = nullptr" and "tail_ = victim->prev", then clears the victim's prev and next pointers. After the victim is erased from "map_" and deleted, the new node goes through the four write lines in push_front. As for why they are both O(1), both functions make a fixed number of pointer writes plus one hash lookup or erase with map_.find(n), which is O(1) on average. Nothing else should really touch the list.

## 2. Heap and top_k
top_k keeps a MinHeap of at most k items, with the minimum of k items on top. Each of the (n) items costs one comparison with the top. An item that is less than the top also causes one "pop" and one "push", which is an O(log k) for each operation. The total is O(n log k), while sorting everything is O(n log n).


## 3. BST height
For ids 1..1000, the height was 1000 when they were inserted in-order and 25 when they were shuffled. For in-order, each new id is larger than every earlier id, so it becomes the right child of the previous node and the tree is a linked list, which degenerates the tree. find() walks through the entire tree with n nodes, so it is O(n) instead of O(log n). An AVL tree would fix this by rotating after each insert and remove, which keeps the height at a minimum of 1.5*log_2(n).

## 4. Three colors in DFS
Gray means a node is on the current path, and black means it is finished. They must be different because a cycle exists only when the DFS reaches a gray node. For the example of a visited/not-visited DFS reporting a cycle that isn't there, let vertix a require vertix b and vertix c, and let both vertix b and c require vertix d. The DFS visits vertix a, then vertix b, then vertix d. When it later reaches vertix c and sees vertix d again, a visited/not-visited DFS reports a cycle even though vertix d has already been visited, but there is no loop. With three colors, vertix d is black and is skipped, so the order is d, b, c, a.


## 5. Inverted index vs. linear scan
| documents | index (ms/query) | linear (ms/query) |
|---|---|---|
| 100   | 0.041 | 3.158   |
| 1,000 | 0.101 | 32.744  |
| 5,000 | 0.415 | 156.952 |
 
The linear scan grew roughly proportionally to the document count, because it has to re-read every document. The index only iterates through the posting lists of the query words, so it was about 77, 324 and 378 times faster at their respective three sizes. Its time still grew, because each word appears in more documents as more documents are added, so its posting lists get longer.

## 6. Threat model
The default policy chain defends against ";", "|", "&", "`", "$(", ">", "<", "rm -rf", "sudo", denies programs that are not on the allowlist, asks for confirmation before "git", "make", "g++" and "python3", and allows only five commands per 60 seconds. It does not stop an allowed program from doing harm, which includes "python3" because it's on the allow list, and python3 commands that avoids the blocked patterns can run arbitrary commands. The pattern check only matches substrings, so it is easy to get around, and "cat" or "grep" can read any file the process can reach. The rate limit also doesn't prevent any bad commands from being processed.


## 7. What I would design differently
One thing that I would design differently is to implement memory_index.cpp with AVL rotations to prevent the problem of having a BST with a height of 1000 after inserting 1..1000 ids in order. 
