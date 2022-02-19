#include "wordle.h"

#include <iostream>
#include <sstream>
#include <string>
#include <cctype>
#include <cmath>
#include <algorithm>
#include <chrono>

int main() {
    Wordle *wordle = Wordle::create("wordlegame.org", "en", 5);
    if (wordle == nullptr) {
        std::cerr << "Failed to create Wordle." << std::endl;
        return 1;
    }

    WordleState state(wordle);

    bool should_exit = false;

    auto process_input = [&](const std::string &line) {
        std::istringstream ss(line);
        std::string cmd;

        if (!(ss >> cmd)) return false;
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), tolower);

        if (cmd == "load") {
            std::string dict_name, dict_lang;
            int word_len;
            if (!(ss >> dict_name >> dict_lang >> word_len)) {
                std::cout << "Format: load <dict_name> <dict_lang> <word_len>" << std::endl;
                return false;
            }

            Wordle *wordle_new = Wordle::create(dict_name, dict_lang, word_len);
            if (wordle_new == nullptr) {
                std::cout << "Failed to load Wordle." << std::endl;
                return false;
            }

            delete wordle;
            wordle = wordle_new;
            state = WordleState(wordle);
            return true;
        }

        if (cmd == "s" || cmd == "status") {
            std::cout << "Current wordle: " << wordle->get_dict_name() << "/" << wordle->get_dict_lang()
                      << ", word_len = " << wordle->get_word_len() << std::endl;
            std::cout << "Dictionary size: answers = " << wordle->get_dict(false)->size()
                      << ", full = " << wordle->get_dict(true)->size() << std::endl;
            return true;
        }

        if (cmd == "g" || cmd == "guess") {
            std::string guess, result_str;
            if (!(ss >> guess >> result_str)) {
                std::cout << "Format: guess <guess> <result>" << std::endl;
                return false;
            }

            int word_len = wordle->get_word_len();
            if (word_len != (int)guess.length() || word_len != (int)result_str.length()) {
                std::cout << "Incorrect word length." << std::endl;
                return false;
            }

            GuessResult result = GuessResult::from(result_str);
            if (result == GuessResult::invalid()) {
                std::cout << "Invalid result string." << std::endl;
                return false;
            }

            state.filter(guess, result);
            std::cout << state.candidate_count() << " candidates left." << std::endl;
            return true;
        }

        if (cmd == "l" || cmd == "list") {
            auto &candidates = state.get_candidates();
            int candidate_cnt = candidates.size();
            std::cout << candidate_cnt << " candidates left." << std::endl;

            int list_cnt = std::min(50, candidate_cnt);
            for (int i = 0; i < list_cnt; i++) {
                std::string str = wordle->get_dict(false)->get_str(candidates[i]);
                std::cout << str;
                if ((i + 1) % 10 == 0 || i + 1 == list_cnt) {
                    std::cout << '\n';
                } else {
                    std::cout << " ";
                }
            }

            return true;
        }

        if (cmd == "c" || cmd == "calc" || cmd == "calculate") {
            if (0 == state.candidate_count()) {
                std::cout << "No candidates left, nothing to calculate." << std::endl;
                return false;
            }

            state.calculate();

            auto &choices = state.get_choices();
            int list_cnt = std::min(10, (int)choices.size());

            std::cout << std::fixed;
            std::cout.precision(3);
            for (int i = 0; i < list_cnt; i++) {
                auto &choice = choices[i];
                std::cout << (i + 1) << ": " << choice.word_str(*wordle) << " " << choice.entropy << '\n';
            }

            int candidate_cnt = state.candidate_count();
            std::cout << candidate_cnt << " candidates (entropy = " << std::log2(candidate_cnt) << ")." << std::endl;

            return true;
        }

        if (cmd == "a" || cmd == "assess") {
            std::string guess;
            if (!(ss >> guess)) {
                std::cout << "Format: assess <guess>" << std::endl;
                return false;
            }

            int word_len = wordle->get_word_len();
            if (word_len != (int)guess.length()) {
                std::cout << "Incorrect length of guess word." << std::endl;
                return false;
            }

            if (0 == state.candidate_count()) {
                std::cout << "No candidates left, nothing to assess." << std::endl;
                return false;
            }

            GuessAssessment assm = state.assess(guess);
            for (const auto &c : assm.cases) {
                int answer_cnt = c.answers.size();
                std::cout << c.result.to_str(word_len) << " (" << answer_cnt << "):";
                int list_cnt = std::min(10, answer_cnt);
                for (int i = 0; i < list_cnt; i++)
                    std::cout << " " << wordle->get_dict(false)->get_str(c.answers[i]);
                if (list_cnt < answer_cnt)
                    std::cout << " ...";
                std::cout << '\n';
            }
            std::cout << "entropy = " << assm.entropy << std::endl;
            return true;
        }

        if (cmd == "m" || cmd == "match") {
            std::string answer, guess;
            if (!(ss >> answer >> guess)) {
                std::cout << "Format: match <answer> <guess>" << std::endl;
                return false;
            }

            int len = answer.length();
            if (len != (int)guess.length()) {
                std::cout << "Lengths do not match." << std::endl;
                return false;
            }
            if (len > GuessResult::max_length) {
                std::cout << "Words too long: length should be no more than "
                          << GuessResult::max_length << "." << std::endl;
                return false;
            }

            GuessResult result = match_word(len, answer.data(), guess.data());
            std::cout << "Result: " << result.to_str(len) << ", raw = " << result.data << std::endl;
            return true;
        }

        if (cmd == "r" || cmd == "reset") {
            state.reset();
            return true;
        }

        if (cmd == "h" || cmd == "help" || cmd == "?") {
            std::cout << "Available commands: "
                         "load, s(tatus), g(uess), l(ist), c(alculate), a(ssess), m(atch), r(eset), h(elp), q(uit)."
                      << std::endl;
            return true;
        }

        if (cmd == "q" || cmd == "quit" || cmd == "exit") {
            should_exit = true;
            return true;
        }

        std::cout << "Unrecognized command \"" << cmd << "\". Type \"help\" for instructions." << std::endl;
        return false;
    };

    while (!should_exit) {
        std::string line;
        std::cout << "Wordle > ";
        std::getline(std::cin, line);
        process_input(line);
    }

    delete wordle;
    return 0;
}