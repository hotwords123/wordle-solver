#ifndef WORDLE_WORDLE_H
#define WORDLE_WORDLE_H

#include "dict.h"

class Wordle {
   protected:
    std::string dict_name;
    std::string dict_lang;
    const Dict *answer_dict = nullptr;
    const Dict *full_dict = nullptr;
    int word_len;

    friend class WordleState;
    friend struct GuessChoice;

    Wordle() = default;

   public:
    static Wordle *create(const std::string &dict_name,
                          const std::string &dict_lang, int word_len);

    Wordle(const Wordle &) = delete;
    Wordle(Wordle &&) = default;

    inline const std::string &get_dict_name() const {
        return dict_name;
    }

    inline const std::string &get_dict_lang() const {
        return dict_lang;
    }

    inline int get_word_len() const {
        return word_len;
    }

    inline const Dict *get_dict(bool is_full) const {
        return is_full ? full_dict : answer_dict;
    }

    ~Wordle();
};

struct GuessResult {
    using data_t = unsigned int;

    static constexpr data_t absent = 0;
    static constexpr data_t misplaced = 1;
    static constexpr data_t correct = 2;

    static constexpr data_t bits_per_elem = 2;
    static constexpr data_t elem_mask = ((data_t)1 << bits_per_elem) - 1;
    static constexpr int max_length = 8 * sizeof(data_t) / bits_per_elem;

    data_t data;

    GuessResult(data_t data = 0) : data(data) {}

    static GuessResult from(const std::string &str);

    inline static GuessResult invalid() {
        return GuessResult(static_cast<data_t>(-1));
    }

    inline bool operator==(const GuessResult &other) const {
        return data == other.data;
    }

    inline bool operator!=(const GuessResult &other) const {
        return data != other.data;
    }

    inline data_t get(int index) const {
        return data >> (bits_per_elem * index) & elem_mask;
    }

    inline void set(int index, data_t value) {
        data |= value << (bits_per_elem * index);
    }

    inline void reset(int index) {
        data &= ~(elem_mask << (bits_per_elem * index));
    }

    inline std::string to_str(int len) const {
        std::string str(len, ' ');
        for (int i = 0; i < len; i++) str[i] = "-mC?"[get(i)];
        return str;
    }
};

GuessResult match_word(int word_len, const char *answer, const char *guess);

struct GuessChoice {
    int guess_id;
    double estimated_entropy;
    double entropy;

    std::string word_str(const Wordle &wordle) const {
        return wordle.full_dict->get_str(guess_id);
    }
};

struct GuessAssessment {
    struct Case {
        GuessResult result;
        std::vector<int> answers;

        template <typename T>
        Case(GuessResult result, T &&answers)
            : result(result), answers(std::forward<T>(answers)) {}
    };

    std::vector<Case> cases;
    double entropy;
};

class WordleState {
   protected:
    const Wordle *wordle;
    std::vector<int> candidates;
    std::vector<GuessChoice> choices;
    bool calculated;

   public:
    WordleState(const Wordle *wordle);

    WordleState(const WordleState &) = default;
    WordleState(WordleState &&) = default;

    WordleState &operator=(const WordleState &) = default;
    WordleState &operator=(WordleState &&) = default;

    inline int candidate_count() const {
        return candidates.size();
    }

    void reset();

    void reset_calculations();

    void filter(const std::string &guess, GuessResult result);

    void calculate(int num_threads = 1);

    GuessAssessment assess(const std::string &guess) const;

    const std::vector<int> &get_candidates() const {
        return candidates;
    }

    const std::vector<GuessChoice> &get_choices() const {
        return choices;
    }
};

#endif