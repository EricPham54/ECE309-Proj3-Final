// src/policy/policies.cpp — YOU IMPLEMENT. Spec §5.2.

#include "policy/policies.h"

#include <utility>

AllowlistPolicy::AllowlistPolicy(const std::vector<std::string>& programs) {
    programs_.insert(programs.begin(), programs.end());                                                 // copies the programs' names into programs_ by using insert, not push_back
}

Decision AllowlistPolicy::evaluate(const CommandRequest& req) {
    if (programs_.count(req.program) > 0) {                                                             // if programs_ has the CommandRequest, then build the result Decision::allow(reason, name())
        return Decision::allow("'" + req.program + "' is on the allowlist", name());
    }

    return Decision::deny("'" + req.program + "' is not on the allowlist", name());                     // if programs_ doesn't contain the CommandRequest, then build the result Decision::deny(reason, name())
}

ArgumentPatternPolicy::ArgumentPatternPolicy(std::vector<std::string> patterns)
    : patterns_(std::move(patterns)) {}

Decision ArgumentPatternPolicy::evaluate(const CommandRequest& req) {
    std::string search_line = req.command_line();                                                       // create string assigned with req.command_line() to find patterns within the CommandRequest

    for (const std::string& pattern : patterns_) {                                                      // iterates through each pattern within patterns_
        if (pattern.empty()){                                                                           // if the pattern is empty, skip the pattern
            continue;
        }

        if (search_line.find(pattern) != std::string::npos) {                                           // if req.command_line contains any patterns through comparison of std::string::npos, then return result Decision::deny(reason, name())
            return Decision::deny("command contains forbidden pattern \"" + pattern + "\"", name());
        }
    }

    return Decision::allow("no forbidden pattern found", name());                                       // if req.command_line doesn't contain any pattern, then return result Decision::allow(reason, name())
}

ConfirmPolicy::ConfirmPolicy(const std::vector<std::string>& programs) {
    programs_.insert(programs.begin(), programs.end());                                                 // copies the programs' names into programs_ by using insert, not push_back
}

Decision ConfirmPolicy::evaluate(const CommandRequest& req) {
    if (programs_.count(req.program) > 0) {                                                             // if programs_ doesn't have the program of the CommandRequest, then ask if req.program is one of the programs
        return Decision::ask("'" + req.program + "' needs your confirmation", name());
    }

    return Decision::allow("'" + req.program + "' does not need confirmation", name());                 // if programs_ doesn't have the program of the CommandRequest, then Allow the req.program
}

RateLimitPolicy::RateLimitPolicy(std::size_t max_commands, double window_seconds, const Clock& clock)
    : max_(max_commands), window_(window_seconds), clock_(clock) {}

Decision RateLimitPolicy::evaluate(const CommandRequest& req) {
    (void)req;
    double now = clock_.now_seconds();                                                                  // captures the current time in seconds

    while (!times_.empty() && (now - times_.front() >= window_)) {                                      // while the queue isn't empty and its front timestamp has left the window, pop the top of the queue
        times_.pop();
    }

    if (times_.size() >= max_) {                                                                        // if the queue already holds max_ timestamps, return Decision::deny(reason, name()) and don't push onto times_
        return Decision::deny("rate limit: " + std::to_string(max_) + " commands per " + std::to_string(window_) + " s", name());
    }

    times_.push(now);                                                                                   // if the queue has room, push now onto times_
    return Decision::allow("within the rate limit", name());                                            // return with Decision::allow(reason, name())  after pushing now onto times_ queue
}

std::size_t RateLimitPolicy::tracked() const noexcept { return times_.size(); }
