/* 

Decides how good a document is for a query.

Final score of a document =
                            text score  (how well the query words match: frequency, rarity, title)
                           + metadata score (upvotes, views, how recent)

*/

#pragma once
#include <cmath>
#include <cstdlib>
#include <string>
#include <algorithm>
#include "index_loader.h"
using namespace std;

const double TITLE_MULTIPLIER = 2.0;   // a word in the title/tags counts double
const double UPVOTE_WEIGHT    = 0.5;   // importance of the post's score (upvotes)
const double VIEWS_WEIGHT     = 0.2;   // importance of the view count
const double RECENCY_WEIGHT   = 0.1;   // importance of how new the post is
const int    OLDEST_YEAR      = 2011;  // the site started around this year

// Rare words are more important than common words.
// basic words/articels like "the" is in almost every document == small value.
// actual to be searched words like "inception" is in few documents == big value.
inline double calculateRarity(int totalDocs, int docsWithWord) {
    return log(1.0 + (double)totalDocs / docsWithWord);
}

// find the score of ONE query word inside ONE document 
// takes in the occurence/posting and rarity as input
// apply importance of title as well
    // log makes sure a word repeated 100 times is not 100 times better eg "the"
    // this is done by looking at how common/unrare that word is.
inline double calculateWordScore(const Posting& posting, double rarity) {
  
    double score = (1.0 + log((double)posting.frequency)) * rarity;
    if (posting.inTitle == 1) 
        score *= TITLE_MULTIPLIER;
    
    return score;
}

// in this dataset, date looks like "2011-01-01T10:00:00.000"
// hence the year is to be extracted from the first 4 characters
inline int getYear(const string& date) {
    if (date.size() < 4) 
    return OLDEST_YEAR;
    return atoi(date.substr(0, 4).c_str());
}

// this score depends only on the document, not on the query
// takes in docinfo/metadata as an input
inline double calculateMetadataScore(const DocInfo& doc) {
    double upvotes = max(doc.score, 0);                     // ignore negative scores
    double views = doc.views;
    int yearsSinceOldest = max(getYear(doc.date) - OLDEST_YEAR, 0);

    return UPVOTE_WEIGHT * log(1.0 + upvotes)
         + VIEWS_WEIGHT * log(1.0 + views)
         + RECENCY_WEIGHT * yearsSinceOldest;

}
