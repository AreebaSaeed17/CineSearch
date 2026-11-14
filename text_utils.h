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
