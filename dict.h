#ifndef WORDLE_DICT_H
#define WORDLE_DICT_H

#include <string>
#include <vector>

class Dict {
   protected:
    std::string path;

    int word_len;
    int num_words;

    std::vector<char> store;

    Dict() = default;

   public:
    static Dict *create(const std::string &path, int word_len);

    inline int size() const {
        return num_words;
    }

    inline const char *get(int index) const {
        return store.data() + index * word_len;
    }

    inline std::string get_str(int index) const {
        return std::string(get(index), word_len);
    }

    Dict(const Dict &) = delete;
    Dict(Dict &&) = default;
};

#endif