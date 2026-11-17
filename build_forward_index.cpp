// this source file reads Posts.xml and creates the following files:
//   lexicon.txt           word and its id
//   forward_index.txt     docId  wordId:frequency:inTitle ...
//   docs.txt              metadata of every document (for ranking/display)
                        // + answers have a ParentId pointing to their question, questions have none

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include "text_utils.h"
using namespace std;

// store info related to each word 
struct WordInfo {
    int frequency = 0;   // how many times the word appears
    int inTitle = 0;     // 1 if the word appears in title or tags
};

// this hashmap named lexicon will be storing words and their corresponding word IDs
// for searching, I will use the wordids it is faster and ofc easier to use instead of strings
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

// clean the rows by removing tags
//  eg "<sci-fi><time-travel>" is changed to "sci-fi,time-travel"
string tagsToCommaList(const string& rawTags) {
    string decoded = decodeXmlEntities(rawTags);
    decoded = replaceAll(decoded, "><", ",");
    decoded = replaceAll(decoded, "<", "");
    decoded = replaceAll(decoded, ">", "");
    return decoded;
}

// each row = post
//Opens the Files: 
    // checks the given input file = Posts.xml
    // opens two blank files (forward_index.txt and docs.txt) to start writing into

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "Usage: ./build_forward Posts.xml" << endl;
        return 1;
    }

    ifstream inputFile(argv[1]);
    if (!inputFile.is_open()) {
        cout << "Could not open " << argv[1] << endl;
        return 1;
    }

    ofstream forwardFile("forward_index.txt");
    ofstream docsFile("docs.txt");

    string line;
    int docId = 0;

    // this loop reads the input passed file line by line
    // each post {having qs and answers} = one row or record

    // it is going to filter out the qs into 2 groups
        // questions 1, answers 2
        // any other kind of questions eg moderator comments or any other jargon is skipped
    while (getline(inputFile, line)) {
        if (line.find("<row ") == string::npos) continue;

        string postType = getAttribute(line, "PostTypeId");
        if (postType != "1" && postType != "2") continue;  

        // read the fields 
        // uses existing functions defined in text_utils.h
        string postId   = getAttribute(line, "Id");
        string score    = getAttribute(line, "Score");
        string views    = getAttribute(line, "ViewCount");
        string date     = getAttribute(line, "CreationDate");
        string parentId = getAttribute(line, "ParentId");
        string title    = decodeXmlEntities(getAttribute(line, "Title"));
        string tags     = tagsToCommaList(getAttribute(line, "Tags"));
        string bodyText = removeHtmlTags(decodeXmlEntities(getAttribute(line, "Body")));

        // fill out stuff that could possibly be empty
        if (score == "") score = "0";
        if (views == "") views = "0";  
        if (parentId == "") parentId = "0";   // since a qs doesnt have parent  

        // count words in this document 
        unordered_map<int, WordInfo> wordsInDoc;

        // TOKENIZATION
        // part 1:      title
        // It chops the title and tags into individual words
        // gets their ID numbers
        // counts their frequency
        // marks inTitle = 1 (high importance)
            // so basically all the words that appear in titles are given a kind of flag
        for (const string& word : tokenize(title + " " + tags)) {
            int wordId = getOrAddWord(word);
            wordsInDoc[wordId].frequency++;
            wordsInDoc[wordId].inTitle = 1;
        }
        // part 2:      body
        for (const string& word : tokenize(bodyText)) {
            int wordId = getOrAddWord(word);
            wordsInDoc[wordId].frequency++;
        }

        // safety check
        if (wordsInDoc.empty()) continue;   


        /* 
        WRITING TO THE FILES:
        data has to be written to:
            1. forwardindex: 
                Writes the post's ID followed by all its word stats 
                format = wordID:frequency:inTitle 
                (for example: 0 45:3:1 89:1:0 means Post 0 contains Word 45 three times including in the title, and Word 89 once in the body)

            2. docs.txt:
                Writes a simple single-line summary of the post 
                (ID, score, views, title and the first 200 characters as a preview snippet)

        */
        forwardFile << docId;
        for (const auto& entry : wordsInDoc) {
            forwardFile << " " << entry.first << ":" << entry.second.frequency
                        << ":" << entry.second.inTitle;
        }
        forwardFile << "\n";

        string snippet = makeOneLine(bodyText.substr(0, 200));
               docsFile << docId << "\t" << postId << "\t" << postType << "\t" << score << "\t"
                 << views << "\t" << date << "\t" << tags << "\t"
                 << makeOneLine(title) << "\t" << snippet << "\t" << parentId << "\n";

        docId++;
        // just to see progress
        if (docId % 10000 == 0) cout << "Processed " << docId << " documents..." << endl;
    }

    // write to lexicon.txt
    // Once it finishes reading the whole file, it pushed the entire lexicon dictionary into lexicon.txt and shuts down
    ofstream lexiconFile("lexicon.txt");
    for (const auto& entry : lexicon) {
        lexiconFile << entry.first << " " << entry.second << "\n";
    }

    cout << "Done. Documents: " << docId << ", unique words: " << lexicon.size() << endl;
    return 0;
        
}
