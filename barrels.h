// barrels.h
// Barrel storage for the inverted index.
//
// Each barrel = 2 files on disk:
//   barrel_N.txt : the posting lines (same format as inverted_index.txt), stay on DISK
//   barrel_N.idx : small directory "wordId byteOffset docFreq" per word, loaded into RAM on demand
// barrels_meta.txt tells which range of wordIds lives in which barrel.
//
// To fetch one word: find its barrel -> load that barrel's directory (LRU cache)
//                    -> seek straight to the word's line -> parse only that line.

#pragma once
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <list>
#include <mutex>
#include <shared_mutex>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <algorithm>
using namespace std;

// same layout as the Posting struct in build_inverted.cpp
// (ideally move both into one shared header later)
struct Posting {
    int docId;
    int frequency;
    int inTitle;
};

// one word of a newly added document (used by dynamic content addition)
struct DocWord {
    int wordId;
    int frequency;
    int inTitle;
};

inline string barrelDataFile(int barrelId)  { return "barrel_" + to_string(barrelId) + ".txt"; }
inline string barrelIndexFile(int barrelId) { return "barrel_" + to_string(barrelId) + ".idx"; }
const string BARRELS_META_FILE = "barrels_meta.txt";
const string DELTA_FILE        = "delta_index.txt";   // postings of documents added after the barrels were built

class BarrelManager {
public:
    // maxLoadedBarrels = how many barrel directories may sit in RAM at once
    explicit BarrelManager(size_t maxLoadedBarrels = 3) : maxLoaded(maxLoadedBarrels) {}

    // read barrels_meta.txt and the delta file. returns false if barrels were never built
    bool init() {
        ifstream meta(BARRELS_META_FILE);
        if (!meta.is_open()) return false;

        int numBarrels;
        meta >> numBarrels >> baseTotalDocs;
        ranges.resize(numBarrels);
        for (int i = 0; i < numBarrels; i++) {
            int id, first, last;
            meta >> id >> first >> last;
            ranges[id] = {first, last};
        }
        loadDelta();
        return true;
    }

    // all postings of a word = postings from its barrel + postings from the delta (new docs)
    vector<Posting> getPostings(int wordId) {
        vector<Posting> result;

        int barrelId, docFreq;
        long long offset;
        if (lookupEntry(wordId, barrelId, offset, docFreq)) {
            // a fresh ifstream per read, so threads never share a file handle
            ifstream data(barrelDataFile(barrelId), ios::binary);
            data.seekg(offset);              // jump straight to this word's line, no scanning
            string line;
            getline(data, line);
            parsePostingLine(line, result);
        }

        // new docs always have bigger docIds than old ones, so appending keeps the list sorted
        shared_lock<shared_mutex> lock(deltaMutex);
        auto it = delta.find(wordId);
        if (it != delta.end())
            result.insert(result.end(), it->second.begin(), it->second.end());
        return result;
    }

    // number of docs containing the word. cheap: only reads the directory, not the postings
    int getDocFreq(int wordId) {
        int barrelId, docFreq = 0;
        long long offset;
        if (!lookupEntry(wordId, barrelId, offset, docFreq)) docFreq = 0;

        shared_lock<shared_mutex> lock(deltaMutex);
        auto it = delta.find(wordId);
        if (it != delta.end()) docFreq += (int)it->second.size();
        return docFreq;
    }

    int totalDocs() {
        shared_lock<shared_mutex> lock(deltaMutex);
        return max(baseTotalDocs, deltaMaxDocId + 1);
    }

    // DYNAMIC CONTENT: call this after the doc was added to lexicon/forward/docs files.
    // Barrels stay read-only; new postings go to a small delta index (RAM + file).
    // Writers take an exclusive lock only for the few ms of the insert, so searches are barely blocked.
    void addDocument(int docId, const vector<DocWord>& words) {
        unique_lock<shared_mutex> lock(deltaMutex);
        ofstream out(DELTA_FILE, ios::app);
        for (const DocWord& w : words) {
            delta[w.wordId].push_back({docId, w.frequency, w.inTitle});
            out << w.wordId << " " << docId << ":" << w.frequency << ":" << w.inTitle << "\n";
        }
        deltaMaxDocId = max(deltaMaxDocId, docId);
    }

private:
    struct BarrelRange { int firstWordId; int lastWordId; };
    vector<BarrelRange> ranges;        // ranges[b] = wordId range stored in barrel b (sorted, no overlap)
    int baseTotalDocs = 0;

    // ----- LRU cache of barrel directories -----
    // directory: wordId -> (byte offset in barrel_N.txt, docFreq)
    struct LoadedBarrel {
        int barrelId;
        unordered_map<int, pair<long long, int>> directory;
    };
    list<LoadedBarrel> lruList;                                   // front = most recently used
    unordered_map<int, list<LoadedBarrel>::iterator> cacheMap;    // barrelId -> its place in lruList (O(1) lookup)
    size_t maxLoaded;
    mutex cacheMutex;

    // ----- delta index (documents added at runtime) -----
    unordered_map<int, vector<Posting>> delta;
    int deltaMaxDocId = -1;
    shared_mutex deltaMutex;           // many readers OR one writer

    // binary search over the barrel ranges: O(log numBarrels). returns -1 if no barrel has this word
    int findBarrel(int wordId) const {
        int lo = 0, hi = (int)ranges.size() - 1;
        while (lo <= hi) {
            int mid = (lo + hi) / 2;
            if (wordId < ranges[mid].firstWordId)      hi = mid - 1;
            else if (wordId > ranges[mid].lastWordId)  lo = mid + 1;
            else return mid;
        }
        return -1;
    }

    // find where a word's postings live. loads the barrel's directory if it isn't in the cache
    bool lookupEntry(int wordId, int& barrelId, long long& offset, int& docFreq) {
        barrelId = findBarrel(wordId);
        if (barrelId < 0) return false;

        lock_guard<mutex> lock(cacheMutex);
        list<LoadedBarrel>::iterator found;

        auto cached = cacheMap.find(barrelId);
        if (cached != cacheMap.end()) {
            // HIT: move this barrel to the front (most recently used)
            lruList.splice(lruList.begin(), lruList, cached->second);
            found = cached->second;
        } else {
            // MISS: read the small .idx file into RAM
            LoadedBarrel b;
            b.barrelId = barrelId;
            ifstream idx(barrelIndexFile(barrelId));
            int w, df;
            long long off;
            while (idx >> w >> off >> df) b.directory[w] = {off, df};

            lruList.push_front(move(b));
            cacheMap[barrelId] = lruList.begin();
            found = lruList.begin();

            // too many barrels in RAM? throw out the least recently used one (the back)
            if (lruList.size() > maxLoaded) {
                cacheMap.erase(lruList.back().barrelId);
                lruList.pop_back();
            }
        }

        auto entry = found->directory.find(wordId);
        if (entry == found->directory.end()) return false;
        offset  = entry->second.first;
        docFreq = entry->second.second;
        return true;
    }

    // "wordId docFreq d:f:t d:f:t ..." -> postings. strtol on a raw pointer is much faster
    // than istringstream/sscanf and this runs for every query word
    static void parsePostingLine(const string& line, vector<Posting>& out) {
        const char* p = line.c_str();
        char* end;
        strtol(p, &end, 10);                    // wordId (we already know it)
        p = end;
        long docFreq = strtol(p, &end, 10);
        p = end;
        out.reserve(out.size() + docFreq);

        while (true) {
            while (*p && !isdigit((unsigned char)*p)) p++;   // skip spaces / stray '\r'
            if (!*p) break;
            int docId = (int)strtol(p, &end, 10);  p = end + 1;   // +1 skips ':'
            int freq  = (int)strtol(p, &end, 10);  p = end + 1;
            int title = (int)strtol(p, &end, 10);  p = end;
            out.push_back({docId, freq, title});
        }
    }

    void loadDelta() {
        ifstream in(DELTA_FILE);
        if (!in.is_open()) return;
        string line;
        while (getline(in, line)) {
            int wordId, docId, freq, title;
            if (sscanf(line.c_str(), "%d %d:%d:%d", &wordId, &docId, &freq, &title) == 4) {
                delta[wordId].push_back({docId, freq, title});
                deltaMaxDocId = max(deltaMaxDocId, docId);
            }
        }
    }
};