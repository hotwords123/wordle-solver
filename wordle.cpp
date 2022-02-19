#include "wordle.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <thread>
#include <unordered_map>
#include <utility>

Wordle *Wordle::create(const std::string &dict_name,
                       const std::string &dict_lang, int word_len) {
    Wordle *wordle = new Wordle();
    wordle->dict_name = dict_name;
    wordle->dict_lang = dict_lang;

    if (word_len > GuessResult::max_length) {
        std::cerr
            << "Wordle::create() failed: word length too large: expected <= "
            << GuessResult::max_length << ", found " << word_len << std::endl;
    }
    wordle->word_len = word_len;

    std::string dict_path = dict_name + "/" + dict_lang;

    wordle->answer_dict = Dict::create(dict_path + "/answers", word_len);
    if (wordle->answer_dict == nullptr) {
        std::cerr << "Wordle::create() failed: unable to load answer dictionary"
                  << std::endl;
        goto fail;
    }

    wordle->full_dict = Dict::create(dict_path + "/full", word_len);
    if (wordle->full_dict == nullptr) {
        std::cerr << "Wordle::create() failed: unable to load full dictionary"
                  << std::endl;
        goto fail;
    }

    return wordle;

fail:
    delete wordle;
    return nullptr;
}

Wordle::~Wordle() {
    delete answer_dict;
    delete full_dict;
}

GuessResult GuessResult::from(const std::string &str) {
    GuessResult result;
    int word_len = str.length();

    if (word_len > max_length) {
        std::cerr << "GuessResult::from() failed: string too long" << std::endl;
        return invalid();
    }

    for (int i = 0; i < word_len; i++) {
        switch (str[i]) {
            case '1': case '-': // absent
                break;
            case '2': case 'm': // misplaced
                result.set(i, misplaced);
                break;
            case '3': case 'C': // correct
                result.set(i, correct);
                break;
            default:
                std::cerr << "GuessResult::from() failed:"
                             "unrecognized character '" << str[i] << "'" << std::endl;
                return invalid();
        }
    }

    return result;
}

GuessResult match_word(int word_len, const char *answer, const char *guess) {
    GuessResult result;

    for (int i = 0; i < word_len; i++)
        if (guess[i] == answer[i])
            result.set(i, GuessResult::correct);

    for (int i = 0; i < word_len; i++) {
        if (guess[i] != answer[i]) {
            for (int j = 0; j < word_len; j++) {
                if (result.get(j) == GuessResult::absent && guess[j] == answer[i]) {
                    result.set(j, GuessResult::misplaced);
                    break;
                }
            }
        }
    }

    return result;
}

WordleState::WordleState(const Wordle *wordle) : wordle(wordle) { reset(); }

void WordleState::reset() {
    // fill in `candidates` with ids of all possible answers
    candidates.resize(wordle->answer_dict->size());
    std::iota(candidates.begin(), candidates.end(), 0);

    reset_calculations();
}

void WordleState::reset_calculations() {
    choices.clear();
    calculated = false;
}

void WordleState::filter(const std::string &guess, GuessResult result) {
    auto it = std::remove_if(
        candidates.begin(), candidates.end(),
        [len = wordle->word_len, dict = wordle->answer_dict,
         guess_p = guess.data(), result](int answer_id) {
            return result != match_word(len, dict->get(answer_id), guess_p);
        });
    if (it != candidates.end()) {
        candidates.erase(it, candidates.end());
        reset_calculations();
    }
}

GuessAssessment WordleState::assess(const std::string &guess) const {
    std::unordered_map<GuessResult::data_t, std::vector<int>> map;
    int word_len = wordle->word_len;

    for (int answer_id : candidates) {
        const char *answer = wordle->answer_dict->get(answer_id);
        GuessResult result = match_word(word_len, answer, guess.data());
        map[result.data].push_back(answer_id);
    }

    using iter_t = decltype(map)::iterator;
    std::vector<iter_t> iters;
    double temp = 0;

    iters.reserve(map.size());
    for (iter_t it = map.begin(); it != map.end(); ++it) {
        int cnt = it->second.size();
        temp += cnt * std::log2(cnt);
        iters.push_back(it);
    }

    std::sort(iters.begin(), iters.end(), [](iter_t a, iter_t b) {
        if (a->second.size() != b->second.size())
            return a->second.size() > b->second.size();
        return a->first < b->first;
    });

    GuessAssessment assessment;

    int candidate_cnt = candidates.size();
    assessment.entropy = std::log2(candidate_cnt) - temp / candidate_cnt;

    assessment.cases.reserve(iters.size());
    for (iter_t it : iters) {
        assessment.cases.push_back({ it->first, std::move(it->second) });
    }

    return assessment;
}

void WordleState::calculate(int num_threads) {
    if (calculated) return;

    auto time_start = std::chrono::high_resolution_clock::now();

    // TODO: add thread support

    int word_len = wordle->word_len;
    int guess_cnt = wordle->full_dict->size();
    int candidate_cnt = candidate_count();

    for (int i = 0; i < guess_cnt; i++) {
        const char *guess = wordle->full_dict->get(i);
        std::unordered_map<GuessResult::data_t, int> counts;

        for (int answer_id : candidates) {
            const char *answer = wordle->answer_dict->get(answer_id);
            GuessResult result = match_word(word_len, answer, guess);
            counts[result.data]++;
        }

        // H = -sum p_i log_2 p_i
        //   = -sum c_i/n log_2 c_i/n
        //   = log_2 n - (sum c_i log_2 c_i)/n
        double temp = 0;
        for (auto [_, count] : counts) {
            temp += count * std::log2(count);
        }
        double entropy = std::log2(candidate_cnt) - temp / candidate_cnt;

        choices.push_back({i, -1, entropy});
    }

    std::sort(choices.begin(), choices.end(),
              [](const GuessChoice &a, const GuessChoice &b) {
                  return a.entropy > b.entropy;
              });

    calculated = true;

    auto time_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> ms = time_end - time_start;

    std::cout << std::fixed;
    std::cout.precision(2);
    std::cout << "Calculation took " << ms.count() << "ms." << std::endl;
}
