#include "dict.h"

#include <fstream>
#include <iostream>

Dict *Dict::create(const std::string &path, int word_len) {
    std::string fullpath = "dict/" + path + ".txt";
    std::ifstream fin(fullpath);
    if (!fin.is_open()) {
        std::cerr << "Dict::create() failed: unable to open dictionary \""
                  << fullpath << "\"." << std::endl;
        return nullptr;
    }

    Dict *dict = new Dict();
    dict->path = path;
    dict->word_len = word_len;
    dict->num_words = 0;

    int total_words;
    std::string str;

    if (!(fin >> total_words)) {
        std::cerr << "Dict::create() failed: unable to parse dictionary: "
                     "expected word count"
                  << std::endl;
        goto fail;
    }

    for (int i = 0; i < total_words; i++) {
        if (!(fin >> str)) {
            std::cerr << "Dict::create() failed: unable to parse dictionary: "
                         "expected word"
                      << std::endl;
            goto fail;
        }
        if ((int)str.length() == word_len) {
            dict->store.insert(dict->store.end(), str.begin(), str.end());
            dict->num_words++;
        }
    }

    if (dict->num_words == 0) {
        std::cerr << "Dict::create() failed: unable to load dictionary: "
                     "no words with length " << word_len << " present"
                  << std::endl;
        goto fail;
    }

    return dict;

fail:
    delete dict;
    return nullptr;
}