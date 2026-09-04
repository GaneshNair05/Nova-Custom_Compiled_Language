#pragma once
#include <string>
#include <vector>

// --- Token Vocabulary (X-Macro) ---
// This list is the single source of truth for every token type. Adding a
// new token means adding ONE line here: the TokenType enum, the keyword
// lookup table (lexer.cpp), and tokenTypeToString() are all generated from
// it, so the three can never drift out of sync the way they used to
// (quest/relic/character/spawn/nothing/curse/andd/orr used to fall through
// to "Unknown" in tokenTypeToString because they'd only been added to the
// enum and the switch was never updated to match).
//
//   X(EnumName, keywordText, displayString)
//     EnumName      - the TokenType enumerator.
//     keywordText   - source spelling that the identifier scanner maps to
//                     this token (e.g. "loot"), or nullptr if this token is
//                     never produced from a bare keyword (operators,
//                     literals, EOF, etc.).
//     displayString - text returned by tokenTypeToString().
#define TOKEN_TYPE_LIST \
    X(Number,      nullptr,      "Number") \
    X(String,      nullptr,      "String") \
    X(identifier,  nullptr,      "identifier") \
    X(quest,       "quest",      "quest") \
    X(loot,        "loot",       "loot") \
    X(relic,       "relic",      "relic") \
    X(skill,       "skill",      "skill") \
    X(reward,      "reward",     "reward") \
    X(when,        "when",       "when") \
    X(otherwise,   "otherwise",  "otherwise") \
    X(grind,       "grind",      "grind") \
    X(character,   "character",  "character") \
    X(spawn,       "spawn",      "spawn") \
    X(nothing,     "nothing",    "nothing") \
    X(curse,       "curse",      "curse") \
    X(finish,      "finish",     "finish") \
    X(breakKw,     "break",      "Break") \
    X(continueKw,  "continue",   "Continue") \
    X(andd,        "and",        "And") \
    X(orr,         "or",         "Or") \
    X(comma,       nullptr,      "Comma") \
    X(equal,       nullptr,      "Equal") \
    X(equalequal,  nullptr,      "Equal_Equal") \
    X(plusequal,   nullptr,      "Plus_Equal") \
    X(minusequal,  nullptr,      "Minus_Equal") \
    X(starequal,   nullptr,      "Star_Equal") \
    X(slashequal,  nullptr,      "Slash_Equal") \
    X(percent,     nullptr,      "Percent") \
    X(bang,        nullptr,      "Bang") \
    X(bangequal,   nullptr,      "Bang_Equal") \
    X(less,        nullptr,      "Less") \
    X(lessequal,   nullptr,      "Less_Equal") \
    X(great,       nullptr,      "Great") \
    X(greatequal,  nullptr,      "Great_Equal") \
    X(Plus,        nullptr,      "Plus") \
    X(Minus,       nullptr,      "Minus") \
    X(Star,        nullptr,      "Star") \
    X(Slash,       nullptr,      "Slash") \
    X(Floor,       nullptr,      "Floor") \
    X(LeftParen,   nullptr,      "LeftParen") \
    X(RightParen,  nullptr,      "RightParen") \
    X(LeftBracket, nullptr,      "LeftBracket") \
    X(RightBracket, nullptr,     "RightBracket") \
    X(Error,       nullptr,      "Error") \
    X(EndOfFile,   nullptr,      "EndOfFile")

enum class TokenType {
#define X(name, keyword, str) name,
    TOKEN_TYPE_LIST
#undef X
};

std::string tokenTypeToString(TokenType type);

struct Token {
    TokenType type;
    std::string value;
    size_t line;
    size_t column;
};

class Lexer {
public:
    explicit Lexer(const std::string& source);
    std::vector<Token> tokenize();
    bool hadError() const;

private:
    void addToken(std::vector<Token>& tokens, TokenType type, const std::string& value, size_t line, size_t column);
    void scanNumber(std::vector<Token>& tokens, size_t line, size_t column);
    void scanIdentifier(std::vector<Token>& tokens, size_t startLine, size_t startColumn);
    void scanString(std::vector<Token>& tokens, size_t startLine, size_t startColumn);
    void scanOperator(std::vector<Token>& tokens, size_t startLine, size_t startColumn);

    std::string source;
    size_t current = 0;
    size_t line = 1;
    size_t column = 1;
    bool errorFlag = false;

    char peek() const;
    char peekNext() const;
    char advance();
    bool match(char expected);
    void skipWhitespace();
    void skipLineComment();
    void skipBlockComment(std::vector<Token>& tokens);
};