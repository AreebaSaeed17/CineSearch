// this header file has utility functions for parsing the dataset

#pragma once        // include this headerfile only once while compiling
#include <string>
#include <vector>
using namespace std;

// the function below takes in some text and 2 substrings as an input
// throughout that string, it will swap the "from" data with the "to" data
// from is the substring that i want to find from the entered text
// to is what should replace it
// this is done for all the occurences of those substrings

// the text gets passed by value so the original text isnt altered, only its copy is
inline string replaceAll(string text, const string& from, const string& to) {
    size_t pos = 0;     

    // text.find() finds the position of occurence of "from"
    // loop runs till we keep finding matches

    // text.replace() will replace the full length of the word to be replaced
        // eg "hello" = length 5
        // if i want to replace hello with hi {which has length = 2}
        // the change would happen properly and will remove full hello

    // pos += to.length() means that continue the search after the newly inserted word
    // otherwise it would fall into an infinite loop

    while ((pos = text.find(from, pos)) != string::npos) {
        text.replace(pos, from.length(), to);
        pos += to.length();
    }
    // return the changed text
    return text;
}


// applying the above function onto post.xml's structure ( the selected datafile )
// Posts.xml stores special characters instead of using symbols
inline string decodeXmlEntities(string text) {
    text = replaceAll(text, "&lt;", "<");
    text = replaceAll(text, "&gt;", ">");
    text = replaceAll(text, "&quot;", "\"");
    text = replaceAll(text, "&apos;", "'");
    text = replaceAll(text, "&#xA;", " ");
    text = replaceAll(text, "&#xD;", " ");
    text = replaceAll(text, "&#x9;", " ");
    text = replaceAll(text, "&amp;", "&");   
    return text;
}


// this function removes everything with similar structure to a tag
    // eg <p> or <a href="...">
// identifies the "tag" by looking at the structure
// replace each tag with a space
inline string removeHtmlTags(const string& text) {
    string result;
    bool insideTag = false;

    // traverse over the full text to find all
    // when it reaches <, now we are inside the tag
        // ignore any characters inside it
    // when it reaches >, tag has ended so insidetag = false
        // keep all normal text ahead, dont discard
    
    // keep appending the change to an empty string called result
    for (char c : text) {
        if (c == '<') {
            insideTag = true;
            result += ' ';
        } else if (c == '>') {
            insideTag = false;
        } else if (!insideTag) {
            result += c;
        }
    }
    return result;
}


/* Get the value of an attribute from each row (line)

    // eg if string line ="(<row Id="5" Score="12" Name="xyz" />)";
        the meaningful data in this is:
        Id    = 5
        Score = 12
        Name  = xyz

    what this function does as a whole is
        Find:   Score="
        Jump:   to first character after "
        Stop:   at next "
        Return: everything in between

        if nothing is found, return empty space

key builds the way to search for that attribute
    if attribute name = "Score"
    key = " Score\""

*/

inline string getAttribute(const string& line, const string& name) {
    string key = " " + name + "=\"";
    size_t start = line.find(key);
    
    if (start == string::npos) return "";
    start += key.length();
    size_t end = line.find('"', start);
    
    if (end == string::npos) return "";
    return line.substr(start, end - start);
}


// this function takes in a string as input
inline vector<string> tokenize(const string& text) {
    vector<string> words;           // stores all extracted words 
    
    // This is an optimization
    // Without reserve(), the vector may repeatedly grow as words are added
        // 5 means that the avg word length would be around 5 
        // so based on that if text length is 100, it would have around 20 words
    words.reserve(text.size() / 5);

    // by traversing char by char, build a word
    // that is stored initially in the string "word"
    // allocate 32 bytes to prevent the string from resizing again and again
        // this improves efficiency
    string word;
    word.reserve(32);

    // unsigned will safely handle any neg numbers between string
    // isalnum = is it alphanumeric?
    // if so, simply make it lowercase and append to the temp string "word"
    for (unsigned char ch : text) {
        if (isalnum(ch)) {
            word += (char)tolower(ch);
        } 

        // if smth isnt alphanumeric then it must be some special char
        // what this means is that the word we were building has been completed
        // check valid length 
        // transfer it to the string WORDS (not copying to make operation faster) 
        else {
            if (word.size() >= 1 && word.size() <= 30) {
                words.push_back(move(word));
            }
            word.clear();           // clear for next word
        }
    }

    // handles edge case:   what if the word doesnt have any punctuation at the end? 
    //                      wouldnt get pushed without external intervention
    if (word.size() >= 1 && word.size() <= 30) {
        words.push_back(move(word));
    }

    return words;
}

// Make text safe to store on one line of a tab-separated file
inline string makeOneLine(string text) {
    for (char& c : text) {
        if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    }
    return text;
}

