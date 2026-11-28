/* Loads the files made by the indexer so the search code can use them.
The inverted index is NOT loaded fully into memory.
We only remember where each word's line starts in the file and read that single line when a query needs the word.
*/
#pragma once
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <string>
#include <vector>
#include <unordered_map>
using namespace std;

// One entry in a word's list = "the word is in document docId"
struct Posting {
    int docId;
    int frequency;   // how many times the word appears in that document
    int inTitle;     // 1 if the word is in the title or tags
};

// Stores metadata about one document
    // one document = (one line of docs.txt)
struct DocInfo {
    string postId;
    int postType = 1;    // 1 = question, 2 = answer
    int score = 0;       // upvotes
    int views = 0;
    string date;
    string tags;
    string title;
    string snippet;
};

class IndexLoader {
public:
    // map of lexicons
        // lexicon stores each word and its wordid as key-value pair
    unordered_map<string, int> lexicon;  
    // list of docs
        // each entry in the list is info of one document
    vector<DocInfo> docs;        

    // Loads everything from the given folder
    // Returns false if any of the 3 fetched files are missing
    bool loadAll(const string& dataFolder) {
        return loadLexicon(dataFolder + "/lexicon.txt") &&
               loadDocs(dataFolder + "/docs.txt") &&
               loadInvertedIndexPositions(dataFolder + "/inverted_index.txt");
    }

    // takes the word being searched as an input
    // Returns the id of a word or -1 if the word is not in the lexicon
    int getWordId(const string& word) const {
        auto it = lexicon.find(word);
        if (it == lexicon.end()) return -1;
        return it->second;
    }

    int totalDocs() const { return (int)docs.size(); }

    // Reads the list of documents for one word from the inverted index file
    // uses the directory made by loadInvertedIndexPositions() to jump straight to the line containing that word.
    vector<Posting> getPostings(int wordId) {
        vector<Posting> postings;

        auto it = lineStart.find(wordId);
        if (it == lineStart.end()) return postings;

        invertedFile.clear();
        invertedFile.seekg(it->second);   // jump to this word's line

        string line;
        getline(invertedFile, line);

        istringstream lineStream(line);
        int wordIdInFile, documentCount;
        lineStream >> wordIdInFile >> documentCount;

        postings.reserve(documentCount);
        string token;
        while (lineStream >> token) {
            Posting posting;
            sscanf(token.c_str(), "%d:%d:%d", &posting.docId, &posting.frequency, &posting.inTitle);
            postings.push_back(posting);
        }
        return postings;
    }

private:
    // read the actual invertedindex.txt
    ifstream invertedFile;
    unordered_map<int, streampos> lineStart;   

    bool loadLexicon(const string& path) {
        ifstream file(path);
        if (!file.is_open()) { cout << "Cannot open " << path << endl; return false; }

        string word;
        int wordId;
        while (file >> word >> wordId) {
            lexicon[word] = wordId;
        }
        return true;
    }

    bool loadDocs(const string& path) {
        ifstream file(path);
        if (!file.is_open()) { cout << "Cannot open " << path << endl; return false; }

        string line;
        while (getline(file, line)) {
            istringstream lineStream(line);
            string docIdText, postType, score, views;
            DocInfo doc;

            // fields are separated by tabs
            // read until a tab appears
            // the tab means that the particular field has ended
            getline(lineStream, docIdText, '\t');
            getline(lineStream, doc.postId, '\t');
            getline(lineStream, postType, '\t');
            getline(lineStream, score, '\t');
            getline(lineStream, views, '\t');
            getline(lineStream, doc.date, '\t');
            getline(lineStream, doc.tags, '\t');
            getline(lineStream, doc.title, '\t');
            getline(lineStream, doc.snippet);

            // atoi = convert strings into integers
            doc.postType = atoi(postType.c_str());
            doc.score = atoi(score.c_str());
            doc.views = atoi(views.c_str());
            docs.push_back(doc);   // line number = docId
        }
        return true;
    }

    // Goes through the inverted index file once and remembers where each line starts.
    // Instead of loading the entire inverted index into RAM, it builds a directory.
    // When loadInvertedIndexPositions() runs, it visits every line and stores:
    //          wordId -> start position for each of the words
    // now later when we pass in a word to search in the later defined function, we seek help from here to jump straight 
    // to the line that has that wordid instead of reading every line of the file.
    bool loadInvertedIndexPositions(const string& path) {
        invertedFile.open(path);
        if (!invertedFile.is_open()) { cout << "Cannot open " << path << endl; return false; }

        string line;
        while (true) {
            streampos position = invertedFile.tellg();
            if (!getline(invertedFile, line)) break;

            int wordId;
            if (sscanf(line.c_str(), "%d", &wordId) == 1) {
                lineStart[wordId] = position;
            }
        }
        invertedFile.clear();
        return true;
    }
};
