// this source file reads Posts.xml and creates
//   lexicon.txt        -> word and its id
//   forward_index.txt  -> docId  wordId:frequency:inTitle ...
//   docs.txt           -> metadata of every document (for ranking/display)

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include "text_utils.h"
using namespace std;

// define a struct that stores info related to each word 
struct WordInfo {
    int frequency = 0;   // how many times the word appears
    int inTitle = 0;     // 1 if the word appears in title or tags
};

// this hashmap named lexicon will be storing words and their corresponding word IDs
// for searching, I will use the wordids
// its faster and ofc easier to use integers instead of strings
unordered_map<string, int> lexicon;   

// either return the id of a word
// or add the word to the lexicon if it is new
    // using find(word), we get to know whether that word has already been assigned a wordid in the table or not
    // "it" is an iterator pointing to the location of the entry that stores that {word: wordid}
int getOrAddWord(const string& word) {
    // lexicon.end() is a special mem loc which is after the last entry
    // so if we havent reached that, this means we found the word, return it
    auto it = lexicon.find(word);                   
    if (it != lexicon.end()) 
        return it->second;     
    
    // runs incase find fails and that word is a new word
    // create an id
    // Next unused ID = number of existing words {as it starts from 0}
    int newId = (int)lexicon.size();
    lexicon[word] = newId;
    return newId;
}

