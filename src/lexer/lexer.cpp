#include "lexer.h"
#include <cctype>
#include <iostream>
#include <unordered_map>

// Keyword table, generated from TOKEN_TYPE_LIST: every entry with a
// non-null keywordText gets an entry here automatically. "equip" is kept as
// a manual addition since it's a second spelling for the same TokenType::equal
// token (produced primarily by the '=' operator, not by an identifier) —
// there's no single canonical keyword string for that token to hang it off.
static const std::unordered_map<std::string, TokenType> keywords = [] {
    std::unordered_map<std::string, TokenType> m;
#define X(name, keyword, str) if (keyword) m.emplace(keyword, TokenType::name);
    TOKEN_TYPE_LIST
#undef X
    m.emplace("equip", TokenType::equal);
    return m;
}();

// '+', '-', '*' used to live here too, but each now needs a one-character
// lookahead (for +=, -=, *=) that a flat char->TokenType map can't express,
// so they moved into scanOperator's switch alongside '/' (which already
// needed the same kind of lookahead for // and /*).
static const std::unordered_map<char, TokenType> singleCharOps = {
    {'(', TokenType::LeftParen},
    {')', TokenType::RightParen}, {',', TokenType::comma},
    {'[', TokenType::LeftBracket}, {']', TokenType::RightBracket}
};

std::string tokenTypeToString(TokenType type) {
    switch (type) {
#define X(name, keyword, str) case TokenType::name: return str;
        TOKEN_TYPE_LIST
#undef X
    }
    return "Unknown";
}
Lexer::Lexer(const std::string& source)
    : source(source) {}

bool Lexer::hadError() const {
    return errorFlag;
}

void Lexer::addToken(
    std::vector<Token>& tokens,
    TokenType type,
    const std::string& value,
    size_t line,
    size_t column
){
    tokens.push_back({
        type,
        value,
        line,
        column
    });
}

void Lexer::scanNumber(
    std::vector<Token>& tokens,
    size_t line,
    size_t column
){
    std::string number;

    while (std::isdigit(static_cast<unsigned char>(peek()))) {
        number = number + advance();
    }

    if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peekNext()))) {
        number = number + advance(); 
        while (std::isdigit(static_cast<unsigned char>(peek()))) {
            number = number + advance();
        }
    }

    addToken(
        tokens,
        TokenType::Number,
        number,
        line,
        column
    );
}

void Lexer::scanOperator(
    std::vector<Token>& tokens,
    size_t startLine,
    size_t startColumn
){
    switch (peek()) {

        case '/': {

            if (peekNext() == '/') {

                advance();
                advance();

                addToken(
                    tokens,
                    TokenType::Floor,
                    "//",
                    startLine,
                    startColumn
                );
            }

            else if (peekNext() == '*') {

                skipBlockComment(tokens);
            }

            else if (peekNext() == '=') {

                advance();
                advance();

                addToken(
                    tokens,
                    TokenType::slashequal,
                    "/=",
                    startLine,
                    startColumn
                );
            }

            else {

                advance();

                addToken(
                    tokens,
                    TokenType::Slash,
                    "/",
                    startLine,
                    startColumn
                );
            }

            break;
        }

        case '+':

            if (match('=')) {

                addToken(
                    tokens,
                    TokenType::plusequal,
                    "+=",
                    startLine,
                    startColumn
                );

            } else {

                advance();

                addToken(
                    tokens,
                    TokenType::Plus,
                    "+",
                    startLine,
                    startColumn
                );
            }

            break;

        case '-':

            if (match('=')) {

                addToken(
                    tokens,
                    TokenType::minusequal,
                    "-=",
                    startLine,
                    startColumn
                );

            } else {

                advance();

                addToken(
                    tokens,
                    TokenType::Minus,
                    "-",
                    startLine,
                    startColumn
                );
            }

            break;

        case '*':

            if (match('=')) {

                addToken(
                    tokens,
                    TokenType::starequal,
                    "*=",
                    startLine,
                    startColumn
                );

            } else {

                advance();

                addToken(
                    tokens,
                    TokenType::Star,
                    "*",
                    startLine,
                    startColumn
                );
            }

            break;

        case '%':

            advance();

            addToken(
                tokens,
                TokenType::percent,
                "%",
                startLine,
                startColumn
            );

            break;

        case '=':

            if (match('=')) {

                addToken(
                    tokens,
                    TokenType::equalequal,
                    "==",
                    startLine,
                    startColumn
                );

            } else {

                advance();

                addToken(
                    tokens,
                    TokenType::equal,
                    "=",
                    startLine,
                    startColumn
                );
            }

            break;

        case '<':

            if (match('=')) {

                addToken(
                    tokens,
                    TokenType::lessequal,
                    "<=",
                    startLine,
                    startColumn
                );

            } else {

                advance();

                addToken(
                    tokens,
                    TokenType::less,
                    "<",
                    startLine,
                    startColumn
                );
            }

            break;

        case '>':

            if (match('=')) {

                addToken(
                    tokens,
                    TokenType::greatequal,
                    ">=",
                    startLine,
                    startColumn
                );

            } else {

                advance();

                addToken(
                    tokens,
                    TokenType::great,
                    ">",
                    startLine,
                    startColumn
                );
            }

            break;

        case '!':

            if (match('=')) {

                addToken(
                    tokens,
                    TokenType::bangequal,
                    "!=",
                    startLine,
                    startColumn
                );

            } else {

                advance();

                addToken(
                    tokens,
                    TokenType::bang,
                    "!",
                    startLine,
                    startColumn
                );
            }

            break;

        case '#':

            advance();
            skipLineComment();
            break;

        default: {

            auto it = singleCharOps.find(peek());

            if (it != singleCharOps.end()) {

                std::string val(1, peek());
                advance();

                addToken(
                    tokens,
                    it->second,
                    val,
                    startLine,
                    startColumn
                );

            } else {

                std::cerr
                    << "Error: "
                    << startLine << ":" << startColumn
                    << ": Unexpected Character "
                    << peek() << '\n';

                addToken(
                    tokens,
                    TokenType::Error,
                    std::string(1, peek()),
                    startLine,
                    startColumn
                );
                errorFlag = true;

                advance();
            }

            break;
        }
    }
}

void Lexer::scanIdentifier(
    std::vector<Token>& tokens,
    size_t startLine,
    size_t startColumn
) {
    std::string identifier;

    while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_') {
        identifier += advance();
    }

    auto it = keywords.find(identifier);
    TokenType type = (it != keywords.end()) ? it->second : TokenType::identifier;

    addToken(
        tokens,
        type,
        identifier,
        startLine,
        startColumn
    );
}

void Lexer::scanString(
    std::vector<Token>& tokens,
    size_t startLine,
    size_t startColumn
) {
    advance(); 

    std::string value;

    while (peek() != '"' && peek() != '\0') {
        value += advance();
    }

    if (peek() == '\0') {
        std::cerr
            << "Lexer Error: "
            << startLine << ":" << startColumn
            << ": Unterminated string\n";
        addToken(tokens, TokenType::Error, value, startLine, startColumn);
        errorFlag = true;
        return;
    }

    advance(); 

    addToken(tokens, TokenType::String, value, startLine, startColumn);
}

char Lexer::peek() const {
    if (current >= source.length()) {
        return '\0';
    }

    return source[current];
}

char Lexer::peekNext() const {
    if (current + 1 >= source.length()) {
        return '\0';
    }

    return source[current + 1];
}

char Lexer::advance() {
    if (current >= source.length()) {
        return '\0';
    }

    char c = source[current++];
    if (c == '\n'){
        line++;
        column=1;
    }
    else{
        column++;
    }
    return c;
}

bool Lexer::match(char expected) {
    if (peekNext() != expected) {
        return false;
    }
    advance();
    advance(); 
    return true;
}

void Lexer::skipWhitespace() {
    while (std::isspace(static_cast<unsigned char>(peek()))) {
        advance();
    }
}

void Lexer::skipLineComment() {
    while (peek() != '\n' && peek() != '\0') {
        advance();
    }
}

void Lexer::skipBlockComment(std::vector<Token>& tokens){
    if(peek() == '/' && peekNext() == '*'){
        size_t startLine = line;
        size_t startColumn = column;

        advance();
        advance();
        while(true){
            if(peek() == '\0'){
                std::cerr << "Lexer Error " << line << ":" << column << " Unterminated Comment\n";
                addToken(tokens, TokenType::Error, "/*", startLine, startColumn);
                errorFlag = true;
                break;
            }
            if(peek() == '*' && peekNext() == '/'){
                advance();
                advance();
                break;
            }
            advance();
        }
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (current < source.length()) {
        skipWhitespace();
        size_t startLine = line;
        size_t startColumn = column;

        if (current >= source.length()) {
            break;
        }

        char c = peek();

        if (std::isdigit(static_cast<unsigned char>(c))) {
            scanNumber(tokens, startLine, startColumn);
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            scanIdentifier(tokens, startLine, startColumn);
            continue;
        }
        if (c == '"') {
            scanString(tokens, startLine, startColumn);
            continue;
        }
        scanOperator(
            tokens,
            startLine,
            startColumn
        );
    }

    tokens.push_back({TokenType::EndOfFile, "", line, column});

    return tokens;
}