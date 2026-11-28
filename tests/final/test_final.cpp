// tests/final/test_final.cpp — YOUR TEST SUITE (spec §8).
//
// You need at least 20 tests. The 8 examples below are given and count:
//   Memory  (4): heap, lru, bst, ctx
//   Policy  (2): pol x2
//   Skills  (2): graph, index
// Write at least 12 more of your own, covering the list in spec §8.
// Name tests <area>_<what> with the areas heap, lru, bst, ctx, pol, graph,
// index, so the summary groups them.
//
//   ./build/test_final              run everything
//   ./build/test_final lru_         only tests whose names start with lru_
//   ./build/test_final =lru_evicts_least_recently_used    one test, exactly
//   ./build/test_final --list       list test names
//
// If a sanitizer error (ASan/UBSan) aborts the run, the tests after it do
// not run: rerun the failing test alone with =its_name and read the FIRST
// stack frame that is in your own file.
//
// CHECK_EQ(a, b) prints both values when it fails (vectors too), so prefer
// it over CHECK(a == b).
//
// Helpers you can use (tests/test_helpers.h): TempDir, write_text,
// write_skill, FixedPolicy, ScriptedConfirmer, LinesInput, StringOutput,
// parse_ok, args1. ManualClock is in policy/policy.h.

#include "../mini_test.h"
#include "../test_helpers.h"

#include "memory/context_builder.h"
#include "memory/memory_index.h"
#include "memory/memory_store.h"
#include "policy/policies.h"
#include "policy/policy_chain.h"
#include "skills/dep_graph.h"
#include "skills/skill_catalog.h"
#include "skills/skill_index.h"
#include "util/lru_cache.h"
#include "util/min_heap.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>

// =========================================================================
// Memory — 4 examples
// =========================================================================

// Example 1 (heap): items come back smallest first, duplicates included.
TEST(heap_pops_in_order) {
    MinHeap<int> h;
    for (int x : {5, 1, 4, 1, 3}) h.push(x);
    CHECK_EQ(h.size(), std::size_t{5});
    CHECK_EQ(h.top(), 1);
    std::vector<int> got;
    while (!h.empty()) got.push_back(h.pop());
    CHECK_EQ(got, (std::vector<int>{1, 1, 3, 4, 5}));
}

// Example 2 (lru): the least recently USED entry is evicted, and get()
// counts as a use.
TEST(lru_evicts_least_recently_used) {
    LruCache<std::string, int> c(2);
    c.put("a", 1);
    c.put("b", 2);
    REQUIRE(c.get("a") != nullptr);   // "a" is now the most recent
    c.put("c", 3);                    // full: "b" must go
    CHECK(c.contains("a"));
    CHECK(!c.contains("b"));
    CHECK_EQ(c.keys_mru_to_lru(), (std::vector<std::string>{"c", "a"}));
    CHECK_EQ(c.stats().evictions, std::size_t{1});
}

// Example 3 (bst): removing a node with two children keeps the tree sorted,
// keeps every other record, and lowers size() by exactly one.
TEST(bst_remove_two_children) {
    MemoryIndex t;
    for (std::uint64_t id : {50, 30, 70, 60, 80}) REQUIRE(t.insert(MemoryRecord{id, "m" + std::to_string(id)}));
    REQUIRE(t.remove(70));
    std::vector<std::uint64_t> ids;
    t.for_each_in_order([&](const MemoryRecord& r) { ids.push_back(r.id); });
    CHECK_EQ(ids, (std::vector<std::uint64_t>{30, 50, 60, 80}));
    CHECK(t.find(70) == nullptr);
    REQUIRE(t.find(80) != nullptr);
    CHECK_EQ(t.find(80)->text, std::string("m80"));
    CHECK_EQ(t.size(), std::size_t{4});
}

// Example 4 (ctx): the worked example in spec §4.4.
TEST(ctx_spec_example) {
    std::vector<ContextItem> items{
        {Role::System, 10, false},     // 0  protected
        {Role::User, 10, false},       // 1
        {Role::Assistant, 10, false},  // 2  dropped 3rd (oldest assistant)
        {Role::Tool, 10, false},       // 3  dropped 1st
        {Role::Assistant, 10, false},  // 4
        {Role::Tool, 10, false},       // 5  dropped 2nd
        {Role::User, 10, false},       // 6  recent: protected
        {Role::Assistant, 10, false}}; // 7  recent: protected
    ContextPlan p = select_context(items, 50, 2);
    CHECK_EQ(p.keep, (std::vector<std::size_t>{0, 1, 4, 6, 7}));
    CHECK_EQ(p.total_tokens, std::size_t{50});
    CHECK(!p.over_budget);
}

// =========================================================================
// Policy — 2 examples
// =========================================================================

// Example 5 (pol): the sliding window. A request exactly `window` seconds
// after an allowed one no longer counts it.
TEST(pol_rate_limit_window_boundary) {
    ManualClock clock(100);
    RateLimitPolicy p(2, 10.0, clock);   // at most 2 per 10 s
    CommandRequest req;
    req.program = "ls";
    CHECK(p.evaluate(req).kind == Decision::Kind::Allow);   // t = 100
    clock.advance(1);
    CHECK(p.evaluate(req).kind == Decision::Kind::Allow);   // t = 101
    clock.advance(1);
    CHECK(p.evaluate(req).kind == Decision::Kind::Deny);    // t = 102: two in the window
    clock.advance(8);
    CHECK(p.evaluate(req).kind == Decision::Kind::Allow);   // t = 110: 100 has left (110 - 100 = 10)
    CHECK(p.tracked() <= 2u);
}

// Example 6 (pol): first Deny wins and later policies are not asked; with no
// Deny, the FIRST Ask is returned unchanged.
TEST(pol_chain_first_deny_stops_first_ask_wins) {
    auto ask1 = std::make_unique<FixedPolicy>(Decision::Kind::Ask, "ask1");
    auto ask2 = std::make_unique<FixedPolicy>(Decision::Kind::Ask, "ask2");
    PolicyChain chain;
    REQUIRE(chain.add(std::make_unique<FixedPolicy>(Decision::Kind::Allow, "ok")));
    REQUIRE(chain.add(std::move(ask1)));
    REQUIRE(chain.add(std::move(ask2)));
    CommandRequest req;
    req.program = "git";
    Decision d = chain.evaluate(req);
    CHECK(d.kind == Decision::Kind::Ask);
    CHECK_EQ(d.policy, std::string("ask1"));

    auto deny = std::make_unique<FixedPolicy>(Decision::Kind::Deny, "no");
    auto after = std::make_unique<FixedPolicy>(Decision::Kind::Allow, "after");
    FixedPolicy* after_ptr = after.get();   // still valid: the chain owns it now
    REQUIRE(chain.add(std::move(deny)));
    REQUIRE(chain.add(std::move(after)));
    d = chain.evaluate(req);
    CHECK(d.kind == Decision::Kind::Deny);
    CHECK_EQ(d.policy, std::string("no"));
    CHECK_EQ(after_ptr->calls, 0);     // never evaluated: the chain stopped at the Deny
}

// =========================================================================
// Skills — 2 examples
// =========================================================================

// Example 7 (graph): post-order load order, and a cycle message that lists
// only the nodes still on the DFS path (b is already finished).
TEST(graph_order_and_cycle_message) {
    DepGraph g;
    for (const char* n : {"a", "b", "c", "d"}) g.add_node(n);
    g.add_edge("a", "b");   // a requires b
    g.add_edge("a", "c");
    g.add_edge("b", "d");
    g.add_edge("c", "d");   // diamond: d is reached twice, but it is not a cycle
    std::vector<std::string> order;
    std::string error;
    REQUIRE(g.order_from("a", &order, &error));
    CHECK_EQ(order, (std::vector<std::string>{"d", "b", "c", "a"}));

    g.add_edge("c", "a");   // now a -> c -> a is a cycle
    CHECK(!g.order_from("a", &order, &error));
    CHECK_EQ(error, std::string("cycle: a -> c -> a"));
}

// Example 8 (index): rank by number of distinct query words; ties go to the
// smaller doc id.
TEST(index_ranking_and_ties) {
    SkillIndex ix;
    ix.add(0, "format numbers");
    ix.add(1, "csv report with table and numbers");
    ix.add(2, "table format");
    ix.add(3, "unrelated");
    CHECK_EQ(ix.search("csv table numbers", 10), (std::vector<std::size_t>{1, 0, 2}));
    CHECK_EQ(ix.search("format table", 1), (std::vector<std::size_t>{2}));
    REQUIRE(ix.postings("numbers") != nullptr);
    CHECK_EQ(*ix.postings("numbers"), (std::vector<std::size_t>{0, 1}));
}

// =========================================================================
// Your tests (at least 12 more). Spec §8 lists what they must cover.
// =========================================================================
// Small helper functions that test functions would use.

// Builds a CommandRequest (ex: {"git", {"status"}})
static CommandRequest make_request(const std::string& program, const std::vector<std::string>& args = {}) {
    CommandRequest req;
    req.program = program;
    req.args = args;
    return req;
}

// Returns the ids stored in a MemoryIndex in in-order (ascending order)
static std::vector<std::uint64_t> ids_in_order(const MemoryIndex& tree) {
    std::vector<std::uint64_t> ids;
    tree.for_each_in_order([&](const MemoryRecord& r) { ids.push_back(r.id); });
    return ids;
}

// Puts a, b, c, d into a cache in that order with d being the most recent
static void fill_abcd(LruCache<std::string, int>& cache) {
    cache.put("a", 1);
    cache.put("b", 2);
    cache.put("c", 3);
    cache.put("d", 4);
}

// Builds a DepGraph from node names and `from -> to` edges
static DepGraph make_graph(const std::vector<std::string>& nodes, const std::vector<std::pair<std::string, std::string>>& edges) {
    DepGraph g;
    for (const std::string& name : nodes) g.add_node(name);
    for (const auto& edge : edges) g.add_edge(edge.first, edge.second);
    return g;
}

// tests that top() and pop() on an empty heap throws
TEST(heap_empty_throws) {
    MinHeap<int> h;
    CHECK_THROWS(h.top(), std::out_of_range);
    CHECK_THROWS(h.pop(), std::out_of_range);
}

// tests top_k with k = 0 and k > n
TEST(heap_top_k_edge_cases) {
    std::vector<int> items{5, 1, 9, 3, 7};
    CHECK(top_k(items, 0).empty());                                                     // k = 0
    CHECK_EQ(top_k(items, 10), (std::vector<int>{9, 7, 5, 3, 1}));                      // k > n
    CHECK(top_k(std::vector<int>{}, 3).empty());                                        // no items
    CHECK_EQ(top_k(std::vector<int>{4, 4, 2, 4}, 3), (std::vector<int>{4, 4, 4}));      // duplicates
}

// tests the heap against a full sort on random data (using a random number generator)
TEST(heap_top_k_matches_full_sort) {
    std::mt19937 rng(12345);                                                            // random number generator with constant seed
    for (int round = 0; round < 50; ++round) {
        std::size_t n = rng() % 200;
        std::vector<int> data;
        for (std::size_t i = 0; i < n; ++i){
            data.push_back(static_cast<int>(rng() % 50));
        }
        std::size_t k = rng() % 60;

        std::vector<int> expected = data;
        std::sort(expected.begin(), expected.end(), std::greater<int>());
        if (expected.size() > k){ 
            expected.resize(k);
        }

        CHECK_EQ(top_k(data, k), expected);
    }
}

// prints the time for top_k with k = 10 and for std::sort on 100,000 random numbers
TEST(heap_timing_top_k_vs_sort) {
    std::mt19937 rng(2024);
    std::vector<int> data;
    for (int i = 0; i < 100000; ++i){
        data.push_back(static_cast<int>(rng() % 1000000));
    }

    std::vector<int> sorted = data;                                                     // copy of data before the timer starts

    using Clock = std::chrono::steady_clock;
    Clock::time_point start = Clock::now();
    std::vector<int> best = top_k(data, 10);
    Clock::time_point middle = Clock::now();
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());
    Clock::time_point stop = Clock::now();

    std::chrono::duration<double, std::milli> heap_ms = middle - start;
    std::chrono::duration<double, std::milli> sort_ms = stop - middle;
    std::printf("    [measure] 100000 random numbers, k = 10: top_k %.3f ms, std::sort %.3f ms\n", heap_ms.count(), sort_ms.count());
}

// tests that using `put` on an existing key updates without evicting out of LRU cache
TEST(lru_put_existing_updates_without_evicting) {
    LruCache<std::string, int> c(2);
    c.put("a", 1);
    c.put("b", 2);
    c.put("a", 10);                                                                     // "a" is already there: update, no eviction

    CHECK_EQ(c.size(), std::size_t{2});
    CHECK_EQ(c.keys_mru_to_lru(), (std::vector<std::string>{"a", "b"}));
    CHECK_EQ(c.stats().evictions, std::size_t{0});

    const int* value = c.get("a");
    REQUIRE(value != nullptr);
    CHECK_EQ(*value, 10);
}

// tests that capacity 0 never stores anything and capacity 1 keeps only the newest entry for LRU cache
TEST(lru_capacity_zero_and_one) {
    LruCache<int, int> zero(0);
    zero.put(1, 100);
    CHECK_EQ(zero.size(), std::size_t{0});
    CHECK(!zero.contains(1));
    CHECK(zero.get(1) == nullptr);

    LruCache<int, int> one(1);
    one.put(1, 10);
    one.put(2, 20);                                                                     // the cache is full, so 1 is evicted
    CHECK(!one.contains(1));
    CHECK(one.contains(2));
    CHECK_EQ(one.keys_mru_to_lru(), (std::vector<int>{2}));
    CHECK_EQ(one.stats().evictions, std::size_t{1});
}

// test erase() on the head, a middle, and the tail nodes, and check `keys_mru_to_lru()` after each
TEST(lru_erase_head_middle_tail) {
    LruCache<std::string, int> head_case(4);
    fill_abcd(head_case);
    CHECK(head_case.erase("d"));                                                        // the head
    CHECK_EQ(head_case.keys_mru_to_lru(), (std::vector<std::string>{"c", "b", "a"}));

    LruCache<std::string, int> middle_case(4);
    fill_abcd(middle_case);
    CHECK(middle_case.erase("b"));                                                      // the middle node
    CHECK_EQ(middle_case.keys_mru_to_lru(), (std::vector<std::string>{"d", "c", "a"}));

    LruCache<std::string, int> tail_case(4);
    fill_abcd(tail_case);
    CHECK(tail_case.erase("a"));                                                        // the tail
    CHECK_EQ(tail_case.keys_mru_to_lru(), (std::vector<std::string>{"d", "c", "b"}));
}

// test the hit, miss and eviction counts after different function calls
TEST(lru_stats_hits_misses_evictions) {
    LruCache<int, int> c(2);
    CHECK(c.get(1) == nullptr);                                                         // miss 1 because of empty cache
    c.put(1, 1);
    c.put(2, 2);
    CHECK(c.get(1) != nullptr);                                                         // hit 1
    CHECK(c.get(3) == nullptr);                                                         // miss 2
    c.put(3, 3);                                                                        // full and evicts the 2 least recently used
    CHECK(c.get(2) == nullptr);                                                         // miss 3
    CHECK_EQ(c.stats().hits, std::size_t{1});
    CHECK_EQ(c.stats().misses, std::size_t{3});
    CHECK_EQ(c.stats().evictions, std::size_t{1});
}

// test `remove` on a leaf node and a node with one child
TEST(bst_remove_leaf_and_one_child) {
    MemoryIndex t;
    for (std::uint64_t id : {50, 30, 70, 20, 60}){
        REQUIRE(t.insert(MemoryRecord{id, "x"}));
    }

    CHECK(t.remove(20));                                                                // 20 is a leaf: just deleted
    CHECK_EQ(ids_in_order(t), (std::vector<std::uint64_t>{30, 50, 60, 70}));

    CHECK(t.remove(70));                                                                // 70 now has one child (60): 60 takes its place
    CHECK_EQ(ids_in_order(t), (std::vector<std::uint64_t>{30, 50, 60}));
}


// test a duplicate insert id is rejected, that and in-order traversal is sorted for BST
TEST(bst_insert_duplicate_and_in_order) {
    MemoryIndex t;
    for (std::uint64_t id : {40, 20, 60, 10, 30, 50, 70}){
        REQUIRE(t.insert(MemoryRecord{id, "first"}));
    }

    CHECK(!t.insert(MemoryRecord{30, "second"}));   // id 30 is already stored

    CHECK_EQ(ids_in_order(t), (std::vector<std::uint64_t>{10, 20, 30, 40, 50, 60, 70}));
}

// test `remove` on the root when it has two children
TEST(bst_remove_root) {
    MemoryIndex root;
    for (std::uint64_t id : {50, 30, 70, 20, 40, 60, 80}){
        REQUIRE(root.insert(MemoryRecord{id, "x"}));
    }
    CHECK(root.remove(50));
    CHECK_EQ(ids_in_order(root), (std::vector<std::uint64_t>{20, 30, 40, 60, 70, 80}));
    CHECK(root.find(50) == nullptr);
    CHECK_EQ(root.size(), std::size_t{6});
}

// test `height()` based of a BST (used the example from appendix C) and test it against my calculated height
TEST(bst_height_hand_drawn_shape) {
    MemoryIndex t;

    REQUIRE(t.insert(MemoryRecord{50, "x"}));
    CHECK_EQ(t.height(), std::size_t{1});
    REQUIRE(t.insert(MemoryRecord{30, "x"}));
    REQUIRE(t.insert(MemoryRecord{70, "x"}));
    CHECK_EQ(t.height(), std::size_t{2});
    REQUIRE(t.insert(MemoryRecord{20, "x"}));
    CHECK_EQ(t.height(), std::size_t{3});
    REQUIRE(t.insert(MemoryRecord{10, "x"}));
    CHECK_EQ(t.height(), std::size_t{4});
    REQUIRE(t.insert(MemoryRecord{80, "x"}));
    CHECK_EQ(t.height(), std::size_t{4});
}

// print the height after inserting ids 1 to 1000 in order and after inserting
// them shuffled
TEST(bst_height_sequential_vs_shuffled) {
    std::vector<std::uint64_t> ids;
    for (std::uint64_t id = 1; id <= 1000; id++){
        ids.push_back(id);
    }

    MemoryIndex in_order_tree;
    for (std::uint64_t id : ids){
        in_order_tree.insert(MemoryRecord{id, ""});
    }

    std::mt19937 rng(99);
    std::shuffle(ids.begin(), ids.end(), rng);
    MemoryIndex shuffled_tree;
    for (std::uint64_t id : ids){
        shuffled_tree.insert(MemoryRecord{id, ""});
    }

    std::printf("    [measure] BST height for ids 1..1000: in order %zu, shuffled %zu\n", in_order_tree.height(), shuffled_tree.height());
}

// test that system, pinned and the last keep_recent items survive, even when the budget can never be met for context
TEST(context_protected_items_survive) {
    std::vector<ContextItem> items{
        {Role::System, 100, false},
        {Role::User, 100, false},
        {Role::Tool, 100, true},
        {Role::Assistant, 100, false},
        {Role::User, 100, false},
        {Role::Assistant, 100, false}};
    ContextPlan p = select_context(items, 10, 2);                                       // contextplan with budget that can't be met
    CHECK_EQ(p.keep, (std::vector<std::size_t>{0, 2, 4, 5}));
}

// test that the `over_budget` is set correctly
TEST(context_over_budget_flag) {
    std::vector<ContextItem> items{
        {Role::System, 100, false},
        {Role::User, 50, false}};

    ContextPlan over = select_context(items, 50, 0);
    CHECK_EQ(over.keep, (std::vector<std::size_t>{0}));
    CHECK(over.over_budget);
}

// test AllowlistPolicy for allowing only an exact program name, otherwise Deny
TEST(pol_allowlist_exact_match) {
    AllowlistPolicy p({"ls", "git"});

    Decision allowed = p.evaluate(make_request("ls", {"-l"}));
    CHECK(allowed.kind == Decision::Kind::Allow);
    CHECK_EQ(allowed.policy, std::string("allowlist"));

    Decision denied = p.evaluate(make_request("rm"));
    CHECK(denied.kind == Decision::Kind::Deny);
    CHECK_EQ(denied.policy, std::string("allowlist"));
}

// test ArgumentPatternPolicy for allowing only an exact program name, and Deny if a pattern is identified
TEST(pol_patterns_substring_and_empty_pattern) {
    ArgumentPatternPolicy p({";", "rm -rf"});

    Decision semicolon = p.evaluate(make_request("cat", {"a;", "rm", "b"}));            // line is "cat a; rm b"
    CHECK(semicolon.kind == Decision::Kind::Deny);
    CHECK_EQ(semicolon.policy, std::string("patterns"));

    CHECK(p.evaluate(make_request("rm", {"-rf", "/"})).kind == Decision::Kind::Deny);   // line is "rm -rf /"
    CHECK(p.evaluate(make_request("rm", {"-r", "/"})).kind == Decision::Kind::Allow);   // "rm -r /" has no pattern
    CHECK(p.evaluate(make_request("ls", {"-l"})).kind == Decision::Kind::Allow);
}

// test ConfirmPolicy for asking for listed programs and allowing all others
TEST(pol_confirm_asks_for_listed_programs) {
    ConfirmPolicy p({"git", "make"});

    Decision asked = p.evaluate(make_request("git", {"status"}));
    CHECK(asked.kind == Decision::Kind::Ask);
    CHECK_EQ(asked.policy, std::string("confirm"));

    Decision allowed = p.evaluate(make_request("ls"));
    CHECK(allowed.kind == Decision::Kind::Allow);
    CHECK_EQ(allowed.policy, std::string("confirm"));
}

// test a nested chain in a policy (Deny from the inner chain still names the leaf policy, and an Allow is reported by the outer chain)
TEST(pol_chain_nested_keeps_leaf_name) {
    auto internal = std::make_unique<PolicyChain>();
    internal->add(std::make_unique<AllowlistPolicy>(std::vector<std::string>{"ls"}));
    PolicyChain outer;
    outer.add(std::make_unique<FixedPolicy>(Decision::Kind::Allow, "first"));
    outer.add(std::move(internal));

    Decision denied = outer.evaluate(make_request("rm"));                               // internal allowlist says no
    CHECK(denied.kind == Decision::Kind::Deny);
    CHECK_EQ(denied.policy, std::string("allowlist"));

    Decision allowed = outer.evaluate(make_request("ls"));
    CHECK(allowed.kind == Decision::Kind::Allow);
    CHECK_EQ(allowed.policy, std::string("chain"));
}

// test a regular chain DepGraph
TEST(graph_chain_order) {
    DepGraph g = make_graph({"a", "b", "c"}, {{"a", "b"}, {"b", "c"}});
    std::vector<std::string> order;
    std::string error;
    REQUIRE(g.order_from("a", &order, &error));
    CHECK_EQ(order, (std::vector<std::string>{"c", "b", "a"}));
}

// test a self-loop DepGraph (skill requires itself)
TEST(graph_self_loop_message) {
    DepGraph g = make_graph({"a"}, {{"a", "a"}});
    std::vector<std::string> order;
    std::string error;
    CHECK(!g.order_from("a", &order, &error));
    CHECK_EQ(error, std::string("cycle: a -> a"));
}

// test a three-node DepGraph (dependency loop)
TEST(graph_three_node_cycle_message) {
    std::vector<std::string> order;
    std::string error;

    DepGraph three = make_graph({"a", "b", "c"}, {{"a", "b"}, {"b", "c"}, {"c", "a"}});
    CHECK(!three.order_from("a", &order, &error));
    CHECK_EQ(error, std::string("cycle: a -> b -> c -> a"));
}

// test DepGraph when a vertice is missing a requirement
TEST(graph_missing_requirement_message) {
    std::vector<std::string> order;
    std::string error;

    DepGraph g = make_graph({"a", "b"}, {{"a", "b"}, {"b", "none"}});
    CHECK(!g.order_from("a", &order, &error));
    CHECK_EQ(error, std::string("missing: none (required by b)"));
}

// test DepGraph for `dependents` correct ordering
TEST(graph_dependents_ordering) {
    DepGraph chain = make_graph({"x1", "x2", "x3", "x4"}, {{"x1", "x2"}, {"x2", "x3"}, {"x3", "x4"}});  // x1 > x2 > x3 > x4. The nearest dependent of x4 is x3, so the order is NOT alphabetical.
    CHECK_EQ(chain.dependents("x4"), (std::vector<std::string>{"x3", "x2", "x1"}));

    DepGraph cyc = make_graph({"a", "b", "c", "e"}, {{"e", "c"}, {"b", "c"}, {"a", "b"}, {"c", "a"}});
    CHECK_EQ(cyc.dependents("c"), (std::vector<std::string>{"b", "e", "a"}));
}

// test index for a word that repeats inside one document is posted once
TEST(index_postings_once_per_document) {
    SkillIndex in;
    in.add(0, "table table TABLE");
    in.add(1, "table other");

    REQUIRE(in.postings("table") != nullptr);
    CHECK_EQ(*in.postings("table"), (std::vector<std::size_t>{0, 1}));                  // doc 0 is listed once
}

// test the prints of the average time per query for `search` and for `linear_search` on 100, 1,000 and 5,000 generated documents using std::chrono::steady_clock
TEST(index_timing_vs_linear_search) {
    std::mt19937 rng(7);

    auto random_text = [&rng](int count) {                                              // makes a text of `count` random words from a vocabulary of 300 words
        std::string text;
        for (int i = 0; i < count; ++i){
            text += "w" + std::to_string(rng() % 300) + " ";
        }
        return text;
    };

    for (std::size_t doc_count : {100u, 1000u, 5000u}) {
        std::vector<std::string> docs;
        SkillIndex ix;
        for (std::size_t id = 0; id < doc_count; ++id) {
            docs.push_back(random_text(12));
            ix.add(id, docs.back());
        }

        std::vector<std::string> queries;
        for (int q = 0; q < 50; ++q){
            queries.push_back(random_text(2));                                          // 50 queries of 2 words each
        }

        std::vector<std::vector<std::size_t>> from_index, from_linear;                  // keeps the results be compared later

        using Clock = std::chrono::steady_clock;                                        // allows for time to be chcekd
        Clock::time_point start = Clock::now();
        for (const std::string& q : queries){
            from_index.push_back(ix.search(q, 5));
        }
        Clock::time_point middle = Clock::now();
        for (const std::string& q : queries){
            from_linear.push_back(linear_search(docs, q, 5));
        }
        Clock::time_point stop = Clock::now();

        std::chrono::duration<double, std::micro> index_s = middle - start;
        std::chrono::duration<double, std::micro> linear_s = stop - middle;
        std::printf("    [measure] %zu documents: index %.3f us/query, linear_search %.3f us/query\n", doc_count, index_s.count() / static_cast<double>(queries.size()), linear_s.count() / static_cast<double>(queries.size()));
    }
}

int main(int argc, char** argv) { return mt::run_all(argc, argv); }
