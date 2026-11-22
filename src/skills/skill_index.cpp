// src/skills/skill_index.cpp — YOU IMPLEMENT. Spec §6.3.

#include "skills/skill_index.h"
#include "util/min_heap.h"
#include "util/text.h"

#include <utility>

void SkillIndex::add(std::size_t doc_id, std::string_view text) {
    for (const std::string& word : unique_tokens(text)) {                               // for every distinct word in unique_tokens(text), append doc_id to that word's posting list
        postings_[word].push_back(doc_id);
    }
}

const std::vector<std::size_t>* SkillIndex::postings(std::string_view word) const {
    auto posting = postings_.find(std::string(word));                                   // finds if there is a posting list for word in postings
    if (posting == postings_.end()){                                                    // if there is no posting list for word, then return nullptr
        return nullptr;
    }

    return &posting->second;                                                            // return address of the word's posting list
}

std::size_t SkillIndex::word_count() const noexcept {
    return postings_.size();                                                            // returns the size of the postings_ map
}

std::vector<std::size_t> SkillIndex::search(std::string_view query, std::size_t k) const {
    if (k == 0){                                                                        // if k is 0, then return an empty vector
        return {};
    }

    std::unordered_map<std::size_t, std::size_t> score;                                 // creates an unordered_map called score
    for (const std::string& word : unique_tokens(query)) {                              // traverse through every word in unique_tokens(query)
        auto walk = postings_.find(word);                                               // find if word is in postings_
        if (walk == postings_.end()){                                                   // skip the word if not found in postings_
            continue;
        }
        for (std::size_t doc : walk->second){                                           // iterates through each doc id's in a word's posting lists and adds 1 to the score map
            score[doc]++;
        }
    }

    std::vector<std::pair<std::size_t, std::size_t>> matches;                           // vector of pairs for score and doc_id called hits
    for (const auto& entry : score){                                                    // adds the score and doc id for every element in score
        matches.push_back({entry.second, entry.first});
    }

    auto worse = [](const std::pair<std::size_t, std::size_t>& a, const std::pair<std::size_t, std::size_t>& b) {   // lambda function for the comparison rule that tells top_k which of two documents is the worse result (returns true when a is worse than b)
        if (a.first != b.first){
            return a.first < b.first;
        }
        return a.second > b.second;
    };

    std::vector<std::pair<std::size_t, std::size_t>> best = top_k(matches, k, worse);   // calls top_k with all pairs in matches and the worse lambda function to compare them, and return the k best with the best first

    std::vector<std::size_t> result;                                                    // creates a result vector to be returned
    for (const auto& h : best){                                                         // iterates through the best vector and adds the doc ids to result vector
        result.push_back(h.second);
    }

    return result;                                                                      // return the result vector
}
