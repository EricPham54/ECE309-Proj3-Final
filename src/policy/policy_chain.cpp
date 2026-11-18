// src/policy/policy_chain.cpp — YOU IMPLEMENT. Spec §5.3.

#include "policy/policy_chain.h"

#include <utility>

bool PolicyChain::add(std::unique_ptr<Policy> child) {
    if (child == nullptr) return false;                                                     // if the child policy is a nullptr, then don't add anything and return false

    children_.push_back(std::move(child));                                                  // if the child policy isn't a null ptr, then push the child onto children_ using a move assignment
    return true;                                                                            // return true if child got added
}

std::size_t PolicyChain::size() const noexcept { return children_.size(); }

Decision PolicyChain::evaluate(const CommandRequest& req) {
    if (children_.empty()) {                                                                // if children_ is empty, then deny and return Decision::deny(reason, name())
        return Decision::deny("the policy chain is empty, so nothing is allowed", name());
    }

    Decision first_ask;                                                                     // creates an object looking for the first ask from a child policy in children_
    bool saw_ask = false;                                                                   // boolean flag if a child policy said ask

    for (const auto& child : children_) {                                                   // iterate through children_ using auto& child because unique_ptr's can't be copied
        Decision temp = child->evaluate(req);                                               // temporarily get the evaluation of the CommandRequest for the child policy

        if (temp.kind == Decision::Kind::Deny){                                             // if temp is denied, then return the evaluation of the CommandRequest for the child policy (later children are not evaluated)
            return temp;
        }

        if (temp.kind == Decision::Kind::Ask && !saw_ask) {                                 // if temp is ask and an ask hasn't been seen yet, assign first_ask with temp and set the saw_ask flag as true
            first_ask = temp;
            saw_ask = true;
        }
    }

    if (saw_ask){                                                                           // return the first ask unchanged if there is at least one ask
        return first_ask;
    }

    return Decision::allow("every policy in the chain allowed it", name());                 // if there wasn't an ask in children_, return Decision::allow(reason, name()) with policy `"chain"`
}
