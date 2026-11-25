/*
this handles single and multiword queries

Algo:
1. Split the query into words using tokenize() defined for indexing.
2. For each word, get its list of documents from the inverted index.
3. For each document, add up the word scores and count how many query words it has.
4. Sort: documents with MORE of the query words come first,and among equal ones, 
         the higher final score comes first.

- So a document containing all the words is always above one containing only some of them.
- Documents containing only some words still appear (below), which helps when
one word is misspelled or missing.

*/

#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include "text_utils.h"
#include "index_loader.h"
#include "ranking.h"
using namespace std;

struct SearchResult {
    int docId;
    double score;
    // how many of the query words this document contains
    int matchedWords;   
};

// unknownWords is filled with query words that are not in the lexicon.
inline vector<SearchResult> searchQuery(IndexLoader& index, const string& query,
                                        vector<string>& unknownWords) {
    // split the query 
    // sort and remove duplicates
    // this ensures that repeated words dont artificially boost up the score 
    vector<string> words = tokenize(query);
    sort(words.begin(), words.end());
    words.erase(unique(words.begin(), words.end()), words.end());

    int totalDocs = index.totalDocs();
    // textscore = how much score has this doc accumulated by matching of words?
    vector<double> textScore(totalDocs, 0.0);   
    // store the count of number of times the doc matched queury words
    vector<int> matchedCount(totalDocs, 0);     
    // documents that matched at least one word
    // instead of scanning each doc, we'll only look at candidate docs.
    vector<int> candidateDocs;                  
    
    
    // for each word in query that has passed above check, get its wordid
        // go through the list of docs it is in
    for (const string& word : words) {
        int wordId = index.getWordId(word);
        // for words that dont appear in the lexicon and hence are unknown are stored to display error to user
        if (wordId == -1) {
            unknownWords.push_back(word);
            continue;
        }

        // find occurences of that particular wordID
        // and find rarity
        // the rare the word, the greater the score
        vector<Posting> postings = index.getPostings(wordId);
        double rarity = calculateRarity(totalDocs, (int)postings.size());

        // go to every document that has that word
        // {word is basically the one that the loop is currently at}
        // keep count of #of times that doc has that word
        for (const Posting& posting : postings) {
            if (matchedCount[posting.docId] == 0) {
                candidateDocs.push_back(posting.docId);   // first time we see this document
            }

            /*
            eg - if search is quantum computer {suppose both come in same doc}
               then 
               when the loop is running for quantum, matchedcount = 1
               when it goes to computer, matchedcount = 2

               - The document keeps accumulating score as more matching words are found
            */
            matchedCount[posting.docId]++;
            textScore[posting.docId] += calculateWordScore(posting, rarity);
        }
    }

    // add the metadata score and make the result list
    // only loop through candidate documents
    // metadata will act like bonus score for the docs that matched
    vector<SearchResult> results;
    results.reserve(candidateDocs.size());
    for (int docId : candidateDocs) {
        SearchResult result;
        result.docId = docId;
        result.matchedWords = matchedCount[docId];
        result.score = textScore[docId] + calculateMetadataScore(index.docs[docId]);
        results.push_back(result);
    }

    // sort the result
    // more matched words first, then higher score first
    sort(results.begin(), results.end(), [](const SearchResult& a, const SearchResult& b) {
        if (a.matchedWords != b.matchedWords) return a.matchedWords > b.matchedWords;
        return a.score > b.score;
    });

    return results;
}
