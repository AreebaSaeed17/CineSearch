// reads inverted_index.txt and splits it into barrels. Creates:
//   barrel_N.txt      posting lines of the words in barrel N
//   barrel_N.idx      "wordId byteOffset docFreq" for each word in barrel N
//   barrels_meta.txt  first line: numBarrels totalDocs, then "barrelId firstWordId lastWordId"
//
// It streams the file line by line, so RAM use stays tiny however big the index gets.

#include "barrels.h"

// a barrel is closed once it holds this many postings.
// more data => more barrels (the number of barrels is not fixed, it scales with the data)
const long long MAX_POSTINGS_PER_BARREL = 1000000;

int main() {
    ifstream invertedFile("inverted_index.txt");
    if (!invertedFile.is_open()) {
        cout << "inverted_index.txt not found. Run build_inverted first." << endl;
        return 1;
    }

    struct BarrelRange { int firstWordId; int lastWordId; };
    vector<BarrelRange> ranges;

    ofstream dataOut, idxOut;           // files of the barrel currently being filled
    long long postingsInBarrel = 0;
    int lastWordInBarrel = -1;
    int maxDocId = -1;

    string line;
    while (getline(invertedFile, line)) {
        if (line.empty()) continue;

        // first two numbers of the line: wordId and how many postings it has
        int wordId = 0, docFreq = 0;
        sscanf(line.c_str(), "%d %d", &wordId, &docFreq);

        // trick to find the total number of docs without parsing every posting:
        // postings are sorted by docId, so the LAST token of each line holds that word's biggest docId.
        // the biggest of those over all lines = last docId in the dataset
        size_t lastSpace = line.rfind(' ');
        int lastDocId = atoi(line.c_str() + lastSpace + 1);   // atoi stops at the first ':'
        maxDocId = max(maxDocId, lastDocId);

        // start a new barrel if this is the first word, or adding this word would overflow the current one.
        // a word's list is never split across barrels, so a huge word just gets a barrel to itself
        bool needNewBarrel = ranges.empty() || (postingsInBarrel + docFreq > MAX_POSTINGS_PER_BARREL);
        if (needNewBarrel) {
            if (!ranges.empty()) ranges.back().lastWordId = lastWordInBarrel;   // finish the previous range

            dataOut.close();
            idxOut.close();
            int newId = (int)ranges.size();
            // binary mode so byte offsets from tellp() are exact on every OS
            dataOut.open(barrelDataFile(newId), ios::binary);
            idxOut.open(barrelIndexFile(newId), ios::binary);
            ranges.push_back({wordId, wordId});
            postingsInBarrel = 0;
        }

        // remember where this word's line starts, then write the line
        long long offset = (long long)dataOut.tellp();
        idxOut << wordId << " " << offset << " " << docFreq << "\n";
        dataOut << line << "\n";

        postingsInBarrel += docFreq;
        lastWordInBarrel = wordId;
    }

    if (!ranges.empty()) ranges.back().lastWordId = lastWordInBarrel;
    dataOut.close();
    idxOut.close();

    ofstream meta(BARRELS_META_FILE);
    meta << ranges.size() << " " << (maxDocId + 1) << "\n";
    for (int i = 0; i < (int)ranges.size(); i++)
        meta << i << " " << ranges[i].firstWordId << " " << ranges[i].lastWordId << "\n";

    cout << "Done. Barrels: " << ranges.size() << ", documents: " << (maxDocId + 1) << endl;
    return 0;
}