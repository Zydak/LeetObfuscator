// String manipulation stress test for the Leet obfuscator.
// Focuses on complex string operations including concatenation,
// searching, transformation, encoding, and parsing.
// Deterministic output for verification.

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <chrono>

#include "../Leet.h"

static inline uint64_t mix64(uint64_t x) {
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

static inline uint64_t add_f(uint64_t h, float f) {
    uint32_t u;
    std::memcpy(&u, &f, sizeof(u));
    return mix64(h ^ u);
}

static inline uint64_t add_d(uint64_t h, double d) {
    uint64_t u;
    std::memcpy(&u, &d, sizeof(u));
    return mix64(h ^ u);
}

static uint64_t hashString(const std::string &value) {
    uint64_t h = 0x13579bdf2468ace0ULL;
    for (unsigned char c : value) {
        h = mix64(h ^ (uint64_t)c);
    }
    return h;
}

// String concatenation with various patterns
static uint64_t stringConcatenation(const std::vector<std::string> &parts) {
    uint64_t h = 0xdeadbeefcafebabeULL;
    std::string result;
    
    for (const auto &part : parts) {
        result += part;
        h = mix64(h ^ hashString(result));
    }
    
    // Reverse concatenation
    std::string reverseResult;
    for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
        reverseResult += *it;
        h = mix64(h ^ hashString(reverseResult));
    }
    
    return h;
}

// String searching with multiple patterns
static uint64_t stringSearching(const std::string &text, const std::vector<std::string> &patterns) {
    uint64_t h = 0xfeedfacebadc0ffeULL;
    
    for (const auto &pattern : patterns) {
        size_t pos = text.find(pattern);
        if (pos != std::string::npos) {
            h = mix64(h ^ (uint64_t)pos);
            h = mix64(h ^ hashString(pattern));
        } else {
            h = mix64(h ^ 0xFFFFFFFFULL);
        }
        
        // Find from end
        size_t rpos = text.rfind(pattern);
        if (rpos != std::string::npos) {
            h = mix64(h ^ (uint64_t)rpos);
        }
        
        // Count occurrences
        size_t count = 0;
        size_t searchPos = 0;
        while ((searchPos = text.find(pattern, searchPos)) != std::string::npos) {
            count++;
            searchPos += pattern.length();
        }
        h = mix64(h ^ (uint64_t)count);
    }
    
    return h;
}

// String transformation operations
static uint64_t stringTransformations(std::string text) {
    uint64_t h = 0x0badf00d1337c0deULL;
    
    // To upper
    std::string upper = text;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    h = mix64(h ^ hashString(upper));
    
    // To lower
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    h = mix64(h ^ hashString(lower));
    
    // Reverse
    std::string reversed = text;
    std::reverse(reversed.begin(), reversed.end());
    h = mix64(h ^ hashString(reversed));
    
    // Sort characters (use stable_sort for determinism)
    std::string sorted = text;
    std::stable_sort(sorted.begin(), sorted.end());
    h = mix64(h ^ hashString(sorted));
    
    return h;
}

// String slicing and substring operations
static uint64_t stringSlicing(const std::string &text, const std::array<int, 10> &positions) {
    uint64_t h = 0xcafebabedeadfaceULL;
    
    for (int pos : positions) {
        if (pos >= 0 && pos < (int)text.length()) {
            std::string substr = text.substr(pos, text.length() - pos);
            h = mix64(h ^ hashString(substr));
            
            if (pos + 5 < (int)text.length()) {
                std::string substr5 = text.substr(pos, 5);
                h = mix64(h ^ hashString(substr5));
            }
        }
    }
    
    // Get first and last characters
    if (!text.empty()) {
        h = mix64(h ^ (uint64_t)text[0]);
        h = mix64(h ^ (uint64_t)text[text.length() - 1]);
    }
    
    return h;
}

// String replacement operations
static uint64_t stringReplacement(std::string text, const std::vector<std::pair<std::string, std::string>> &replacements) {
    uint64_t h = 0xfacefeed0ddba11eULL;
    
    for (const auto &rep : replacements) {
        std::string current = text;  // Start fresh for each replacement pattern
        size_t pos = 0;
        while ((pos = current.find(rep.first, pos)) != std::string::npos) {
            current.replace(pos, rep.first.length(), rep.second);
            pos += rep.second.length();
        }
        h = mix64(h ^ hashString(current));
    }
    
    return h;
}

// String trimming operations
static uint64_t stringTrimming(const std::string &text) {
    uint64_t h = 0xbaddcafebabe1337ULL;
    
    // Trim left
    std::string leftTrimmed = text;
    leftTrimmed.erase(leftTrimmed.begin(), 
                     std::find_if(leftTrimmed.begin(), leftTrimmed.end(), 
                                 [](int c) { return !std::isspace(c); }));
    h = mix64(h ^ hashString(leftTrimmed));
    
    // Trim right
    std::string rightTrimmed = text;
    rightTrimmed.erase(std::find_if(rightTrimmed.rbegin(), rightTrimmed.rend(),
                                    [](int c) { return !std::isspace(c); }).base(),
                      rightTrimmed.end());
    h = mix64(h ^ hashString(rightTrimmed));
    
    // Trim both
    std::string trimmed = leftTrimmed;
    trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(),
                              [](int c) { return !std::isspace(c); }).base(),
                trimmed.end());
    h = mix64(h ^ hashString(trimmed));
    
    return h;
}

// String splitting operations
static uint64_t stringSplitting(const std::string &text, const std::vector<char> &delimiters) {
    uint64_t h = 0xc0dedeadbadf00dULL;
    
    for (char delim : delimiters) {
        std::vector<std::string> parts;
        std::string current;
        
        for (char c : text) {
            if (c == delim) {
                parts.push_back(current);
                current.clear();
            } else {
                current += c;
            }
        }
        if (!current.empty()) {
            parts.push_back(current);
        }
        
        for (const auto &part : parts) {
            h = mix64(h ^ hashString(part));
        }
        
        h = mix64(h ^ (uint64_t)parts.size());
    }
    
    return h;
}

// String padding operations
static uint64_t stringPadding(const std::string &text, int targetLength, char padChar) {
    uint64_t h = 0xfeedfacedeadbeefULL;
    
    // Pad left
    std::string leftPadded = text;
    if ((int)leftPadded.length() < targetLength) {
        leftPadded = std::string(targetLength - leftPadded.length(), padChar) + leftPadded;
    }
    h = mix64(h ^ hashString(leftPadded));
    
    // Pad right
    std::string rightPadded = text;
    if ((int)rightPadded.length() < targetLength) {
        rightPadded += std::string(targetLength - rightPadded.length(), padChar);
    }
    h = mix64(h ^ hashString(rightPadded));
    
    return h;
}

// String comparison operations
static uint64_t stringComparison(const std::string &a, const std::string &b) {
    uint64_t h = 0xfaceface12345678ULL;
    
    // Equality
    bool equal = (a == b);
    h = mix64(h ^ (uint64_t)equal);
    
    // Less than
    bool less = (a < b);
    h = mix64(h ^ (uint64_t)less);
    
    // Compare
    int cmp = a.compare(b);
    int ordered = (cmp > 0) - (cmp < 0);
    h = mix64(h ^ (uint64_t)ordered);
    
    // Lexicographical compare
    auto lexicographical = std::lexicographical_compare(
        a.begin(), a.end(), b.begin(), b.end());
    h = mix64(h ^ (uint64_t)lexicographical);
    
    return h;
}

// String encoding simulation (Base64-like)
static uint64_t stringEncodingSimulation(const std::string &text) {
    uint64_t h = 0xdead1234face5678ULL;
    std::string encoded;
    
    for (char c : text) {
        // Simple encoding: add 1 to each character
        encoded += (char)(c + 1);
    }
    
    h = mix64(h ^ hashString(encoded));
    
    // Decode
    std::string decoded;
    for (char c : encoded) {
        decoded += (char)(c - 1);
    }
    
    h = mix64(h ^ hashString(decoded));
    
    return h;
}

// String character analysis
static uint64_t stringCharacterAnalysis(const std::string &text) {
    uint64_t h = 0x1337deadbeef1337ULL;
    
    int uppercase = 0;
    int lowercase = 0;
    int digits = 0;
    int spaces = 0;
    int special = 0;
    
    for (char c : text) {
        if (std::isupper(c)) uppercase++;
        else if (std::islower(c)) lowercase++;
        else if (std::isdigit(c)) digits++;
        else if (std::isspace(c)) spaces++;
        else special++;
    }
    
    h = mix64(h ^ (uint64_t)uppercase);
    h = mix64(h ^ (uint64_t)lowercase);
    h = mix64(h ^ (uint64_t)digits);
    h = mix64(h ^ (uint64_t)spaces);
    h = mix64(h ^ (uint64_t)special);
    
    return h;
}

// String repeat operations
static uint64_t stringRepeat(const std::string &text, int count) {
    uint64_t h = 0xfacedeadbeefcafeULL;
    std::string repeated;
    
    for (int i = 0; i < count; ++i) {
        repeated += text;
    }
    
    h = mix64(h ^ hashString(repeated));
    h = mix64(h ^ (uint64_t)repeated.length());
    
    return h;
}

// String insert and erase operations
static uint64_t stringInsertErase(std::string text, const std::array<int, 5> &positions) {
    uint64_t h = 0xdeadcafe12345678ULL;
    
    for (int pos : positions) {
        std::string working = text;  // Fresh copy for each position
        if (pos >= 0 && pos <= (int)working.length()) {
            working.insert(pos, "X");
            h = mix64(h ^ hashString(working));
            
            if (pos < (int)working.length() - 1) {
                working.erase(pos, 1);
                h = mix64(h ^ hashString(working));
            }
        }
    }
    
    return h;
}

// String replace with different lengths
static uint64_t stringVariableReplace(std::string text) {
    uint64_t h = 0xbeefdeadface1234ULL;
    
    // Replace single character with multiple (use copy to avoid interference)
    std::string text1 = text;
    size_t pos = 0;
    while ((pos = text1.find('a', pos)) != std::string::npos) {
        text1.replace(pos, 1, "xyz");
        pos += 3;
    }
    h = mix64(h ^ hashString(text1));
    
    // Replace multiple characters with single (use original text)
    std::string text2 = text;
    pos = 0;
    while ((pos = text2.find("xyz", pos)) != std::string::npos) {
        text2.replace(pos, 3, "A");
        pos += 1;
    }
    h = mix64(h ^ hashString(text2));
    
    return h;
}

// String pattern matching
static uint64_t stringPatternMatching(const std::string &text, const std::string &pattern) {
    uint64_t h = 0xcafebeefdeadfaceULL;
    
    // Simple wildcard matching (single character)
    int wildcardMatches = 0;
    for (size_t i = 0; i < text.length(); ++i) {
        bool match = true;
        for (size_t j = 0; j < pattern.length(); ++j) {
            if (i + j >= text.length()) {
                match = false;
                break;
            }
            if (pattern[j] != '?' && pattern[j] != text[i + j]) {
                match = false;
                break;
            }
        }
        if (match) wildcardMatches++;
    }
    
    h = mix64(h ^ (uint64_t)wildcardMatches);
    
    return h;
}

// String case conversion
static uint64_t stringCaseConversion(const std::string &text) {
    uint64_t h = 0xfeeddeadcafebeefULL;
    
    std::string converted;
    for (char c : text) {
        if (std::islower(c)) {
            converted += std::toupper(c);
        } else if (std::isupper(c)) {
            converted += std::tolower(c);
        } else {
            converted += c;
        }
    }
    
    h = mix64(h ^ hashString(converted));
    
    return h;
}

// String trimming of specific characters
static uint64_t stringTrimChars(const std::string &text, char trimChar) {
    uint64_t h = 0x0badf00d12345678ULL;
    
    // Trim left
    std::string leftTrimmed = text;
    leftTrimmed.erase(leftTrimmed.begin(), 
                     std::find_if(leftTrimmed.begin(), leftTrimmed.end(), 
                                 [trimChar](int c) { return c != trimChar; }));
    h = mix64(h ^ hashString(leftTrimmed));
    
    // Trim right
    std::string rightTrimmed = text;
    rightTrimmed.erase(std::find_if(rightTrimmed.rbegin(), rightTrimmed.rend(),
                                    [trimChar](int c) { return c != trimChar; }).base(),
                      rightTrimmed.end());
    h = mix64(h ^ hashString(rightTrimmed));
    
    return h;
}

// String unique characters
static uint64_t stringUniqueCharacters(const std::string &text) {
    uint64_t h = 0xfacefacecafe1234ULL;
    
    std::string unique;
    for (char c : text) {
        if (unique.find(c) == std::string::npos) {
            unique += c;
        }
    }
    
    h = mix64(h ^ hashString(unique));
    h = mix64(h ^ (uint64_t)unique.length());
    
    return h;
}

// String rotation
static uint64_t stringRotation(std::string text, int positions) {
    uint64_t h = 0xdeadbeeffacefaceULL;
    
    if (!text.empty()) {
        positions = positions % text.length();
        std::rotate(text.begin(), text.begin() + positions, text.end());
    }
    
    h = mix64(h ^ hashString(text));
    
    return h;
}

// String shuffle simulation (deterministic)
static uint64_t stringShuffleSimulation(std::string text) {
    uint64_t h = 0xcafebabedead1234ULL;
    
    // Deterministic shuffle using swap pattern
    for (size_t i = 0; i < text.length(); ++i) {
        size_t j = (i * 7) % text.length();
        std::swap(text[i], text[j]);
    }
    
    h = mix64(h ^ hashString(text));
    
    return h;
}

// String partition
static uint64_t stringPartition(const std::string &text, char delimiter) {
    uint64_t h = 0xfeedfacecafe5678ULL;
    
    size_t pos = text.find(delimiter);
    if (pos != std::string::npos) {
        std::string before = text.substr(0, pos);
        std::string after = text.substr(pos + 1);
        
        h = mix64(h ^ hashString(before));
        h = mix64(h ^ hashString(after));
    } else {
        h = mix64(h ^ hashString(text));
    }
    
    return h;
}

// String join operations
static uint64_t stringJoin(const std::vector<std::string> &parts, const std::string &delimiter) {
    uint64_t h = 0xdeadcafeface1234ULL;
    std::string result;
    
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            result += delimiter;
        }
        result += parts[i];
    }
    
    h = mix64(h ^ hashString(result));
    
    return h;
}

// String center operation
static uint64_t stringCenter(const std::string &text, int width, char fillChar) {
    uint64_t h = 0xbeefdeadcafe5678ULL;
    
    if ((int)text.length() >= width) {
        h = mix64(h ^ hashString(text));
        return h;
    }
    
    int totalPadding = width - text.length();
    int leftPadding = totalPadding / 2;
    int rightPadding = totalPadding - leftPadding;
    
    std::string centered = std::string(leftPadding, fillChar) + text + 
                          std::string(rightPadding, fillChar);
    
    h = mix64(h ^ hashString(centered));
    
    return h;
}

// String expand tabs
static uint64_t stringExpandTabs(const std::string &text, int tabSize) {
    uint64_t h = 0xfacefeeddead5678ULL;
    std::string expanded;
    
    int column = 0;
    for (char c : text) {
        if (c == '\t') {
            int spaces = tabSize - (column % tabSize);
            expanded += std::string(spaces, ' ');
            column += spaces;
        } else {
            expanded += c;
            column++;
            if (c == '\n') column = 0;
        }
    }
    
    h = mix64(h ^ hashString(expanded));
    
    return h;
}

// String word count
static uint64_t stringWordCount(const std::string &text) {
    uint64_t h = 0x13371337deadfaceULL;
    int words = 0;
    bool inWord = false;
    
    for (char c : text) {
        if (std::isalnum(c)) {
            if (!inWord) {
                words++;
                inWord = true;
            }
        } else {
            inWord = false;
        }
    }
    
    h = mix64(h ^ (uint64_t)words);
    
    return h;
}

// String line count
static uint64_t stringLineCount(const std::string &text) {
    uint64_t h = 0xc0dec0dedeadbeefULL;
    int lines = 1;
    
    for (char c : text) {
        if (c == '\n') {
            lines++;
        }
    }
    
    h = mix64(h ^ (uint64_t)lines);
    
    return h;
}

int main() {
    auto start = std::chrono::high_resolution_clock::now();
    // Initialize test data
    std::vector<std::string> concatParts = {"Hello", " ", "World", " ", "Test", " ", "String"};
    std::string searchText = "The quick brown fox jumps over the lazy dog. The fox is very quick.";
    std::vector<std::string> searchPatterns = {"fox", "quick", "lazy", "dog", "the"};
    std::string transformText = "Hello World Test String";
    std::array<int, 10> slicePositions = {0, 5, 10, 15, 20, 25, 30, 35, 40, 45};
    std::vector<std::pair<std::string, std::string>> replacements = {
        {"fox", "cat"},
        {"quick", "slow"},
        {"lazy", "active"}
    };
    std::string trimText = "   \t  Hello World  \t   ";
    std::vector<char> splitDelimiters = {' ', ',', '.', ';'};
    std::vector<std::string> numberStrings = {"123", "456", "789", "3.14", "2.718", "-42", "0"};
    std::array<int, 5> insertPositions = {0, 5, 10, 15, 20};
    std::string patternText = "The quick brown fox jumps over the lazy dog";
    std::string pattern = "q?ick";
    std::vector<std::string> joinParts = {"part1", "part2", "part3", "part4", "part5"};
    
    uint64_t checksum = 0xdeadbeefdeadbeefULL;
    
    // Run all string manipulation tests
    checksum ^= stringConcatenation(concatParts);
    checksum ^= stringSearching(searchText, searchPatterns);
    checksum ^= stringTransformations(transformText);
    checksum ^= stringSlicing(patternText, slicePositions);
    checksum ^= stringReplacement(searchText, replacements);  // searchText is not modified here anymore
    checksum ^= stringTrimming(trimText);
    checksum ^= stringSplitting(patternText, splitDelimiters);
    checksum ^= stringPadding("test", 10, '*');
    checksum ^= stringPadding("hello", 8, '-');
    checksum ^= stringComparison("hello", "world");
    checksum ^= stringComparison("test", "test");
    checksum ^= stringEncodingSimulation("Hello World");
    checksum ^= stringCharacterAnalysis(patternText);
    checksum ^= stringRepeat("abc", 5);
    checksum ^= stringInsertErase("Hello World Test", insertPositions);
    checksum ^= stringVariableReplace("a quick brown fox");
    checksum ^= stringPatternMatching(patternText, pattern);
    checksum ^= stringCaseConversion("HeLLo WoRLd");
    checksum ^= stringTrimChars("XXXHello WorldXXX", 'X');
    checksum ^= stringUniqueCharacters("abracadabra");
    checksum ^= stringRotation("Hello World", 3);
    checksum ^= stringShuffleSimulation("abcdefgh");
    checksum ^= stringPartition("hello:world", ':');
    checksum ^= stringJoin(joinParts, "-");
    checksum ^= stringCenter("test", 10, '*');
    checksum ^= stringExpandTabs("Hello\tWorld\tTest", 4);
    checksum ^= stringWordCount(patternText);
    checksum ^= stringLineCount("Line1\nLine2\nLine3\nLine4");
    
    checksum = mix64(checksum);

    std::printf("CHECKSUM: 0x%016llx\n", (unsigned long long)checksum);

    auto end = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    printf("%ld\n", diff);
    return 0;
}