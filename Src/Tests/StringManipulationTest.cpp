#include <iostream>
#include <iomanip>
#include <string>
#include <cstring>
#include <cstdio>
#include <cctype>
#include <vector>
#include <map>
#include <algorithm>
#include <sstream>
#include <chrono>

#define LEET_IMPLEMENTATION
#include "../Leet.h"

__attribute__((noinline))
uint64_t fnv1aHash(const std::string& s) {
    uint64_t hash = 14695981039346656037ULL;
    const uint64_t prime = 1099511628211ULL;
    for (unsigned char c : s) {
        hash ^= static_cast<uint64_t>(c);
        hash *= prime;
    }
    return hash;
}

__attribute__((noinline))
uint64_t fnv1aHashCStr(const char* s) {
    uint64_t hash = 14695981039346656037ULL;
    const uint64_t prime = 1099511628211ULL;
    while (*s != '\0') {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(*s));
        hash *= prime;
        ++s;
    }
    return hash;
}

__attribute__((noinline))
size_t safeStrLen(const char* s, size_t maxLen) {
    size_t len = 0;
    while (len < maxLen && s[len] != '\0') {
        ++len;
    }
    return len;
}

__attribute__((noinline))
void safeStrCopy(char* dest, size_t destSize, const char* src) {
    if (destSize == 0) return;
    size_t i = 0;
    while (i + 1 < destSize && src[i] != '\0') {
        dest[i] = src[i];
        ++i;
    }
    dest[i] = '\0';
}

__attribute__((noinline))
void safeStrAppend(char* dest, size_t destSize, const char* src) {
    size_t destLen = safeStrLen(dest, destSize);
    if (destLen >= destSize - 1) return;
    size_t i = 0;
    while (destLen + i + 1 < destSize && src[i] != '\0') {
        dest[destLen + i] = src[i];
        ++i;
    }
    dest[destLen + i] = '\0';
}

__attribute__((noinline))
std::string toUpperStr(const std::string& s) {
    std::string result = s;
    for (char& c : result) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return result;
}

__attribute__((noinline))
std::string toLowerStr(const std::string& s) {
    std::string result = s;
    for (char& c : result) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return result;
}

__attribute__((noinline))
std::string reverseStr(const std::string& s) {
    std::string result = s;
    std::reverse(result.begin(), result.end());
    return result;
}

__attribute__((noinline))
void reverseCStrInPlace(char* s, size_t len) {
    if (len == 0) return;
    size_t left = 0;
    size_t right = len - 1;
    while (left < right) {
        char tmp = s[left];
        s[left] = s[right];
        s[right] = tmp;
        ++left;
        --right;
    }
}

__attribute__((noinline))
bool isAlnumChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0;
}

__attribute__((noinline))
char normalizeChar(char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

__attribute__((noinline))
bool isPalindrome(const std::string& s) {
    std::vector<char> filtered;
    filtered.reserve(s.size());
    for (char c : s) {
        if (isAlnumChar(c)) {
            filtered.push_back(normalizeChar(c));
        }
    }
    size_t left = 0;
    size_t right = filtered.empty() ? 0 : filtered.size() - 1;
    while (left < right) {
        if (filtered[left] != filtered[right]) return false;
        ++left;
        --right;
    }
    return true;
}

__attribute__((noinline))
std::vector<std::string> splitStr(const std::string& s, char delimiter) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : s) {
        if (c == delimiter) {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    parts.push_back(current);
    return parts;
}

__attribute__((noinline))
std::string joinStr(const std::vector<std::string>& parts, const std::string& delimiter) {
    std::string result;
    for (size_t i = 0; i < parts.size(); ++i) {
        result += parts[i];
        if (i + 1 < parts.size()) {
            result += delimiter;
        }
    }
    return result;
}

__attribute__((noinline))
bool isSpaceChar(char c) {
    return std::isspace(static_cast<unsigned char>(c)) != 0;
}

__attribute__((noinline))
std::string trimStr(const std::string& s) {
    size_t start = 0;
    size_t end = s.size();
    while (start < end && isSpaceChar(s[start])) ++start;
    while (end > start && isSpaceChar(s[end - 1])) --end;
    return s.substr(start, end - start);
}

__attribute__((noinline))
std::vector<std::string> tokenize(const std::string& s, const std::string& delimiters) {
    std::vector<std::string> tokens;
    std::string current;
    for (char c : s) {
        bool isDelim = delimiters.find(c) != std::string::npos;
        if (isDelim) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

__attribute__((noinline))
std::map<std::string, int> wordFrequency(const std::vector<std::string>& words) {
    std::map<std::string, int> freq;
    for (const auto& w : words) {
        std::string lower = toLowerStr(w);
        freq[lower] += 1;
    }
    return freq;
}

__attribute__((noinline))
std::string longestCommonPrefix(const std::vector<std::string>& strs) {
    if (strs.empty()) return "";
    std::string prefix = strs[0];
    for (size_t i = 1; i < strs.size(); ++i) {
        size_t j = 0;
        size_t maxLen = std::min(prefix.size(), strs[i].size());
        while (j < maxLen && prefix[j] == strs[i][j]) {
            ++j;
        }
        prefix = prefix.substr(0, j);
        if (prefix.empty()) break;
    }
    return prefix;
}

__attribute__((noinline))
bool parseIntSafe(const std::string& s, long& outValue) {
    if (s.empty()) return false;
    size_t i = 0;
    bool negative = false;
    if (s[0] == '-' || s[0] == '+') {
        negative = (s[0] == '-');
        i = 1;
    }
    if (i >= s.size()) return false;
    long value = 0;
    for (; i < s.size(); ++i) {
        char c = s[i];
        if (c < '0' || c > '9') return false;
        value = value * 10 + (c - '0');
    }
    outValue = negative ? -value : value;
    return true;
}

__attribute__((noinline))
std::string intToStringManual(long value) {
    char buffer[32];
    int written = std::snprintf(buffer, sizeof(buffer), "%ld", value);
    if (written < 0) return "";
    return std::string(buffer);
}

__attribute__((noinline))
std::string caesarShift(const std::string& s, int shift) {
    std::string result = s;
    int normalizedShift = ((shift % 26) + 26) % 26;
    for (char& c : result) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (uc >= 'a' && uc <= 'z') {
            int offset = static_cast<int>(uc - 'a');
            offset = (offset + normalizedShift) % 26;
            c = static_cast<char>('a' + offset);
        } else if (uc >= 'A' && uc <= 'Z') {
            int offset = static_cast<int>(uc - 'A');
            offset = (offset + normalizedShift) % 26;
            c = static_cast<char>('A' + offset);
        }
    }
    return result;
}

const char* BASE64_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

__attribute__((noinline))
std::string base64Encode(const std::string& input) {
    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);

    size_t i = 0;
    size_t len = input.size();
    while (i + 2 < len) {
        unsigned char b0 = static_cast<unsigned char>(input[i]);
        unsigned char b1 = static_cast<unsigned char>(input[i + 1]);
        unsigned char b2 = static_cast<unsigned char>(input[i + 2]);
        output.push_back(BASE64_ALPHABET[(b0 >> 2) & 0x3F]);
        output.push_back(BASE64_ALPHABET[((b0 << 4) | (b1 >> 4)) & 0x3F]);
        output.push_back(BASE64_ALPHABET[((b1 << 2) | (b2 >> 6)) & 0x3F]);
        output.push_back(BASE64_ALPHABET[b2 & 0x3F]);
        i += 3;
    }

    size_t remaining = len - i;
    if (remaining == 1) {
        unsigned char b0 = static_cast<unsigned char>(input[i]);
        output.push_back(BASE64_ALPHABET[(b0 >> 2) & 0x3F]);
        output.push_back(BASE64_ALPHABET[(b0 << 4) & 0x3F]);
        output.push_back('=');
        output.push_back('=');
    } else if (remaining == 2) {
        unsigned char b0 = static_cast<unsigned char>(input[i]);
        unsigned char b1 = static_cast<unsigned char>(input[i + 1]);
        output.push_back(BASE64_ALPHABET[(b0 >> 2) & 0x3F]);
        output.push_back(BASE64_ALPHABET[((b0 << 4) | (b1 >> 4)) & 0x3F]);
        output.push_back(BASE64_ALPHABET[(b1 << 2) & 0x3F]);
        output.push_back('=');
    }
    return output;
}

__attribute__((noinline))
int base64CharIndex(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

__attribute__((noinline))
std::string base64Decode(const std::string& input) {
    std::string output;
    output.reserve((input.size() / 4) * 3);

    unsigned int buffer = 0;
    int bitsCollected = 0;
    for (char c : input) {
        if (c == '=' || c == '\0') break;
        int idx = base64CharIndex(c);
        if (idx < 0) continue;
        buffer = (buffer << 6) | static_cast<unsigned int>(idx);
        bitsCollected += 6;
        if (bitsCollected >= 8) {
            bitsCollected -= 8;
            char outByte = static_cast<char>((buffer >> bitsCollected) & 0xFF);
            output.push_back(outByte);
        }
    }
    return output;
}

__attribute__((noinline))
bool compareByLengthThenAlpha(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return a.size() < b.size();
    return a < b;
}

__attribute__((noinline))
bool isAnagram(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    std::string sa = toLowerStr(a);
    std::string sb = toLowerStr(b);
    std::sort(sa.begin(), sa.end());
    std::sort(sb.begin(), sb.end());
    return sa == sb;
}

__attribute__((noinline))
bool isPalindromeRange(const std::string& s, size_t left, size_t right) {
    while (left < right) {
        if (s[left] != s[right]) return false;
        ++left;
        --right;
    }
    return true;
}

__attribute__((noinline))
std::string longestPalindromicSubstring(const std::string& s) {
    if (s.empty()) return "";
    size_t bestStart = 0;
    size_t bestLen = 1;
    size_t n = s.size();
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i; j < n; ++j) {
            size_t candidateLen = j - i + 1;
            if (candidateLen > bestLen && isPalindromeRange(s, i, j)) {
                bestLen = candidateLen;
                bestStart = i;
            }
        }
    }
    return s.substr(bestStart, bestLen);
}

__attribute__((noinline))
size_t levenshteinDistance(const std::string& a, const std::string& b) {
    size_t n = a.size();
    size_t m = b.size();
    std::vector<std::vector<size_t>> dp(n + 1, std::vector<size_t>(m + 1, 0));

    for (size_t i = 0; i <= n; ++i) dp[i][0] = i;
    for (size_t j = 0; j <= m; ++j) dp[0][j] = j;

    for (size_t i = 1; i <= n; ++i) {
        for (size_t j = 1; j <= m; ++j) {
            if (a[i - 1] == b[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1];
            } else {
                size_t deletion = dp[i - 1][j] + 1;
                size_t insertion = dp[i][j - 1] + 1;
                size_t substitution = dp[i - 1][j - 1] + 1;
                size_t best = deletion;
                if (insertion < best) best = insertion;
                if (substitution < best) best = substitution;
                dp[i][j] = best;
            }
        }
    }
    return dp[n][m];
}

int main() {
    auto start = std::chrono::high_resolution_clock::now();

    uint64_t checksum = 0;

    std::cout << "=== String / C-string Manipulation Test ===\n\n";

    std::cout << "-- C-string basics --\n";
    char buffer[128];
    safeStrCopy(buffer, sizeof(buffer), "Hello, obfuscator world!");
    std::cout << "buffer: " << buffer << "\n";
    safeStrAppend(buffer, sizeof(buffer), " -- appended text.");
    std::cout << "buffer after append: " << buffer << "\n";
    size_t len = safeStrLen(buffer, sizeof(buffer));
    std::cout << "buffer length: " << len << "\n";
    checksum += static_cast<uint64_t>(len);

    const char* literalA = "compare_me";
    const char* literalB = "compare_me";
    const char* literalC = "compare_you";
    int cmpAB = std::strcmp(literalA, literalB);
    int cmpAC = std::strcmp(literalA, literalC);
    int ncmpAC = std::strncmp(literalA, literalC, 7);
    std::cout << "strcmp(A,B) == 0: " << (cmpAB == 0 ? "true" : "false") << "\n";
    std::cout << "strcmp(A,C) != 0: " << (cmpAC != 0 ? "true" : "false") << "\n";
    std::cout << "strncmp(A,C,7) == 0: " << (ncmpAC == 0 ? "true" : "false") << "\n";
    checksum += static_cast<uint64_t>(cmpAB == 0) + static_cast<uint64_t>(cmpAC != 0);

    char toReverse[] = "reverse-this-buffer";
    size_t revLen = safeStrLen(toReverse, sizeof(toReverse));
    reverseCStrInPlace(toReverse, revLen);
    std::cout << "reversed C-string: " << toReverse << "\n";
    checksum += fnv1aHashCStr(toReverse) % 1000000ULL;

    std::cout << "\n-- std::string basics --\n";
    std::string s1 = "The Quick Brown Fox";
    std::string s2 = " Jumps Over The Lazy Dog";
    std::string concatenated = s1 + s2;
    std::cout << "concatenated: " << concatenated << "\n";

    std::string upper = toUpperStr(concatenated);
    std::string lower = toLowerStr(concatenated);
    std::cout << "upper: " << upper << "\n";
    std::cout << "lower: " << lower << "\n";
    checksum += fnv1aHash(upper) % 1000000ULL;
    checksum += fnv1aHash(lower) % 1000000ULL;

    std::string sub = concatenated.substr(4, 5);
    std::cout << "substr(4,5): " << sub << "\n";
    size_t foundPos = concatenated.find("Lazy");
    std::cout << "find(\"Lazy\"): " << foundPos << "\n";
    checksum += foundPos;

    std::string replaced = concatenated;
    size_t replacePos = replaced.find("Fox");
    if (replacePos != std::string::npos) {
        replaced.replace(replacePos, 3, "Cat");
    }
    std::cout << "replaced: " << replaced << "\n";

    std::string reversedStr = reverseStr(concatenated);
    std::cout << "reversed: " << reversedStr << "\n";
    checksum += fnv1aHash(reversedStr) % 1000000ULL;

    std::cout << "\n-- Palindrome checks --\n";
    std::vector<std::string> palindromeCandidates = {
        "A man a plan a canal Panama",
        "Not a palindrome",
        "racecar",
        "Was it a car or a cat I saw",
        "Obfuscation"
    };
    for (const auto& candidate : palindromeCandidates) {
        bool result = isPalindrome(candidate);
        std::cout << "\"" << candidate << "\" is palindrome: " << (result ? "true" : "false") << "\n";
        checksum += result ? 1 : 0;
    }

    std::cout << "\n-- Split / join --\n";
    std::string csvLine = "alpha,beta,gamma,delta,epsilon";
    std::vector<std::string> splitParts = splitStr(csvLine, ',');
    std::cout << "split count: " << splitParts.size() << "\n";
    for (const auto& part : splitParts) {
        std::cout << "  part: " << part << "\n";
    }
    std::string rejoined = joinStr(splitParts, " | ");
    std::cout << "rejoined: " << rejoined << "\n";
    checksum += fnv1aHash(rejoined) % 1000000ULL;

    std::cout << "\n-- Trim --\n";
    std::string padded = "   \t  padded string with spaces  \n  ";
    std::string trimmed = trimStr(padded);
    std::cout << "trimmed: [" << trimmed << "]\n";
    checksum += trimmed.size();

    std::cout << "\n-- Tokenizing --\n";
    std::string sentence = "one, two;three   four,,five";
    std::vector<std::string> tokens = tokenize(sentence, ", ;");
    std::cout << "token count: " << tokens.size() << "\n";
    for (const auto& t : tokens) {
        std::cout << "  token: " << t << "\n";
    }
    checksum += tokens.size();

    std::cout << "\n-- Word frequency --\n";
    std::vector<std::string> paragraph = {
        "the", "quick", "brown", "fox", "the", "lazy", "dog",
        "The", "Fox", "jumps", "over", "the", "dog"
    };
    std::map<std::string, int> freq = wordFrequency(paragraph);
    for (const auto& entry : freq) {
        std::cout << "  " << entry.first << ": " << entry.second << "\n";
        checksum += static_cast<uint64_t>(entry.second);
    }

    std::cout << "\n-- Longest common prefix --\n";
    std::vector<std::string> prefixCandidates = {"interstellar", "interval", "internet", "internal"};
    std::string commonPrefix = longestCommonPrefix(prefixCandidates);
    std::cout << "common prefix: " << commonPrefix << "\n";
    checksum += commonPrefix.size();

    std::cout << "\n-- Manual number/string conversion --\n";
    std::vector<std::string> numberStrings = {"42", "-17", "1000", "notanumber", "+256"};
    for (const auto& ns : numberStrings) {
        long parsed = 0;
        bool ok = parseIntSafe(ns, parsed);
        std::cout << "  parse(\"" << ns << "\") -> ok=" << (ok ? "true" : "false");
        if (ok) {
            std::cout << " value=" << parsed << " roundtrip=" << intToStringManual(parsed);
            checksum += static_cast<uint64_t>(parsed < 0 ? -parsed : parsed);
        }
        std::cout << "\n";
    }

    std::cout << "\n-- Caesar cipher --\n";
    std::string plain = "Attack at Dawn, Obfuscator!";
    std::string encrypted = caesarShift(plain, 7);
    std::string decrypted = caesarShift(encrypted, -7);
    std::cout << "plain:     " << plain << "\n";
    std::cout << "encrypted: " << encrypted << "\n";
    std::cout << "decrypted: " << decrypted << "\n";
    std::cout << "roundtrip matches: " << (decrypted == plain ? "true" : "false") << "\n";
    checksum += fnv1aHash(encrypted) % 1000000ULL;
    checksum += (decrypted == plain) ? 1 : 0;

    std::cout << "\n-- Base64 --\n";
    std::string toEncode = "Deterministic obfuscation testing, 1234567890!";
    std::string encoded = base64Encode(toEncode);
    std::string decoded = base64Decode(encoded);
    std::cout << "original: " << toEncode << "\n";
    std::cout << "encoded:  " << encoded << "\n";
    std::cout << "decoded:  " << decoded << "\n";
    std::cout << "roundtrip matches: " << (decoded == toEncode ? "true" : "false") << "\n";
    checksum += fnv1aHash(encoded) % 1000000ULL;
    checksum += (decoded == toEncode) ? 1 : 0;

    std::cout << "\n-- Sorted word list --\n";
    std::vector<std::string> wordsToSort = {
        "beta", "a", "gamma", "delta", "xy", "alpha", "hi", "z"
    };
    std::vector<std::string> sortedWords = wordsToSort;
    std::sort(sortedWords.begin(), sortedWords.end(), compareByLengthThenAlpha);
    for (const auto& w : sortedWords) {
        std::cout << "  " << w << "\n";
    }
    checksum += fnv1aHash(joinStr(sortedWords, ",")) % 1000000ULL;

    std::cout << "\n-- Anagram checks --\n";
    std::vector<std::pair<std::string, std::string>> anagramPairs = {
        {"listen", "silent"},
        {"triangle", "integral"},
        {"hello", "world"},
        {"Dormitory", "DirtyRoom"}
    };
    for (const auto& p : anagramPairs) {
        bool result = isAnagram(p.first, p.second);
        std::cout << "  " << p.first << " / " << p.second << " -> " << (result ? "true" : "false") << "\n";
        checksum += result ? 1 : 0;
    }

    std::cout << "\n-- Longest palindromic substring --\n";
    std::string palinSource = "forgeeksskeegfortestcaseabccba";
    std::string longestPalin = longestPalindromicSubstring(palinSource);
    std::cout << "source: " << palinSource << "\n";
    std::cout << "longest palindromic substring: " << longestPalin << "\n";
    checksum += longestPalin.size();

    std::cout << "\n-- Levenshtein edit distance --\n";
    std::vector<std::pair<std::string, std::string>> distPairs = {
        {"kitten", "sitting"},
        {"flaw", "lawn"},
        {"intention", "execution"},
        {"obfuscate", "obfuscated"}
    };
    for (const auto& p : distPairs) {
        size_t dist = levenshteinDistance(p.first, p.second);
        std::cout << "  distance(" << p.first << ", " << p.second << ") = " << dist << "\n";
        checksum += dist;
    }

    std::cout << "\n-- ostringstream formatting --\n";
    std::ostringstream oss;
    oss << "Report: " << std::setw(6) << std::setfill('0') << 42
        << " items, total value $" << std::fixed << std::setprecision(2) << 1234.5;
    std::string report = oss.str();
    std::cout << report << "\n";
    checksum += fnv1aHash(report) % 1000000ULL;

    std::cout << "\n-- Deterministic hashing --\n";
    std::vector<std::string> hashSamples = {"alpha", "beta", "gamma", "obfuscator", "determinism"};
    for (const auto& sample : hashSamples) {
        uint64_t h = fnv1aHash(sample);
        std::cout << "  fnv1a(\"" << sample << "\") = " << h << "\n";
        checksum += h % 1000000ULL;
    }

    std::cout << "\n=== Final checksum ===\n";
    std::cout << "TOTAL_CHECKSUM: " << checksum << "\n";

    auto end = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    std::cout << diff << "\n";

    return 0;
}
