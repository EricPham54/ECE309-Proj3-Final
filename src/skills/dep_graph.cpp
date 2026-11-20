// src/skills/dep_graph.cpp — YOU IMPLEMENT. Spec §6.2.

#include "skills/dep_graph.h"

#include <algorithm>
#include <unordered_set>

bool DepGraph::add_node(const std::string& name) {
    if (requires_.count(name) > 0){                                         // if a node with name already exists, return false
        return false;
    }
    
    requires_[name];                                                        // creates the new node with an empty vector of requirements
    return true;
}

// add_edge(): Spec §6.2 table, "add_edge(from, to)" row. The edge means "from requires to".
bool DepGraph::add_edge(const std::string& from, const std::string& to) {
    auto it = requires_.find(from);                                         // finds the from node in requires_ with .find
    if (it == requires_.end()){                                             // if find isn't in requires_, then return false
        return false;
    }

    std::vector<std::string>& reqs = it->second;                            // get a reference to the vector of requirements that the from node already requires
    if (std::find(reqs.begin(), reqs.end(), to) != reqs.end()){             // if to is found in the vector, add no edge and return false
        return false;
    }

    reqs.push_back(to);                                                     // push the edge onto the vector of requirements that the from node already requires
    required_by_[to].push_back(from);                                       // records the edges reversed (when compared to requires_) onto required_by
    return true;
}

bool DepGraph::has_node(const std::string& name) const {
    return requires_.count(name) > 0;                                       // returns if name is a node in requires_
}

std::size_t DepGraph::node_count() const noexcept {
    return requires_.size();                                                // returns the number of nodes in requires_
}

const std::vector<std::string>& DepGraph::requirements(const std::string& name) const {
    static const std::vector<std::string> none;                             // create an empty string vector
    auto it = requires_.find(name);                                         // see if the name node is in requires_
    if (it == requires_.end()){                                             // if the name node isn't in requires_, then return the empty string vector
        return none;
    }

    return it->second;                                                      // return the name's requirements vector
}

namespace {

// The three colors from the project spect describing the vertices of the graph
const int White = 0;                                                        // node not visited yet
const int Gray  = 1;                                                        // node visit started and still on the current DFS path
const int Black = 2;                                                        // node finished

// helper function that builds the cycle message for order_from
std::string cycle_message(const std::vector<std::string>& path, const std::string& x) {
    std::size_t start = 0;                                                  // creates the start variable to traverse path
    while (path[start] != x){                                               // traverse path to see where to stop building the cycle message
        start++;   
    }
    std::string message = "cycle: ";                                        // creates the start of the message to return 
    for (std::size_t i = start; i < path.size(); i++){                      // adds the path to the message to return
        message += path[i] + " -> ";
    }
    message += x;                                                           // adds node x
    return message;                                                         // return the message
}

// DFS helper function for order_from
bool dfs(const DepGraph& g, const std::string& node, std::unordered_map<std::string, int>& color, std::vector<std::string>& path, std::vector<std::string>* out, std::string* error) {
    color[node] = Gray;                                                     // the visit starts at given node
    path.push_back(node);                                                   // push the given node onto path

    for (const std::string& x : g.requirements(node)) {                     // iterate through the edges node -> x in the order they were added
        if (!g.has_node(x)) {                                               // if x is not a node, then the requirement is missing, so return false and give error message.
            *error = "missing: " + x + " (required by " + node + ")";
            return false;
        }
        
        int c = color[x];                                                   // gets color of node x
        
        if (c == Gray) {                                                    // if x is gray, then it is on the current path, so give error message and return false
            *error = cycle_message(path, x);
            return false;
        }
        
        if (c == White) {                                                   // if x is white, then visit the node
            if (!dfs(g, x, color, path, out, error)){                       // recursively call dfs, and return false if the recursive calls returns false
                return false;
            }
        }
    }

    color[node] = Black;                                                    // turns the color of the node into black
    path.pop_back();                                                        // pop the node off of path
    out->push_back(node);                                                   // push the node onto out
    return true;                                                            // return true
}

}

bool DepGraph::order_from(const std::string& root, std::vector<std::string>* out,
                          std::string* error) const {
    if (!has_node(root)) {                                                  // if the root has no nodes, then give error message and return false
        *error = "unknown: " + root;
        return false;
    }
    
    out->clear();                                                           // Start with an empty result so the output is exactly this call's order.
    std::unordered_map<std::string, int> color;                             // unordered_map color holds the gray/black marks; path holds the gray nodes in order.
    std::vector<std::string> path;                                          // create list of nodes for the DFS’s route
    return dfs(*this, root, color, path, out, error);                       // return the result of the recursive dfs function and assign error parameter
}

std::vector<std::string> DepGraph::dependents(const std::string& name) const {
    std::vector<std::string> result;                                        // create result for the BFS

    std::unordered_set<std::string> seen;                                   // creates unordered_set seen for nodes that have been seen
    seen.insert(name);                                                      // name is added first onto seen for BFS

    std::vector<std::string> distance;                                      // vector for each distance away from name
    distance.push_back(name);                                               // name is added first onto distance for BFS

    while (!distance.empty()) {                                             // repeat until distance vector is empty
        std::vector<std::string> next;                                      // vector for the next level of nodes
        for (const std::string& node : distance) {                          // iterates through the nodes in the distance vector
            auto it = required_by_.find(node);                              // looks for the nodes that is dependent on node
            if (it == required_by_.end()){                                  // if no other nodes are dependent on onde, then continue with the for-loop
                continue;
            }
            for (const std::string& dependent : it->second) {               // iterates through the dependents ofthe node
                if (seen.insert(dependent).second){                         // push back dependent if the dependent of node has not already been seen
                    next.push_back(dependent);
                }
            }
        }

        std::sort(next.begin(), next.end());                                // sort each level/distance away from the starting node
        result.insert(result.end(), next.begin(), next.end());              // save the BFS traversal to result
        distance = next;                                                    // make the new distance the next vector
    }
    return result;                                                          // retrun the result of the BFS, which contains the dependents of the given node name
}
