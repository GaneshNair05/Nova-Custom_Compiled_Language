#pragma once
#include <iostream>
#include <vector> 
#include "../lexer/lexer.h"
#include "ast.h"
class ParseError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
class Parser{
    public:
        Parser(std::vector<Token> tokens);
        std::vector<StmtPtr> parse();
        
    private:
        ParseError error(const Token& token, const std::string& message);
        ExpressionPtr expression();
        ExpressionPtr assignment();
        ExpressionPtr logicOr();
        ExpressionPtr logicAnd();
        ExpressionPtr equality();
        ExpressionPtr comparison();
        ExpressionPtr term();
        ExpressionPtr factor();
        ExpressionPtr unary();
        ExpressionPtr call();
        ExpressionPtr primary();

        std::vector<Token> tokens;
        size_t current = 0 ;
        const Token& peek() const;
        const Token& advance();
        const Token& peekNext() const;
        
        bool check(TokenType type) const;
        void consume(TokenType type, const std::string& message);
        void synchronize();
        
        StmtPtr statement();
        StmtPtr letStatement();
        StmtPtr ifStatement();
        StmtPtr whileStatement();
        StmtPtr functionDeclaration();
        StmtPtr returnStatement();
        StmtPtr breakStatement();
        StmtPtr continueStatement();
        StmtPtr expressionStatement();
        std::vector<StmtPtr> block();
        
};