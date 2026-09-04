#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include "parser.h"

Parser::Parser(std::vector<Token> tokens) : tokens(std::move(tokens)) {
    if(this->tokens.empty()){
        this->tokens.push_back({TokenType::EndOfFile, "" , 0 , 0});
    }
}

const Token& Parser::peek() const {
    if(current >= tokens.size()) return tokens.back();
    return tokens[current];
}

const Token& Parser::peekNext() const {
    if(current + 1 >= tokens.size()) return tokens.back();
    return tokens[current + 1];
}

const Token& Parser::advance() {
    if (current < tokens.size()) return tokens[current++];
    return tokens.back();
}

ParseError Parser::error(const Token& token, const std::string& message) {
    std::string errStr = "[Line " + std::to_string(token.line) + ":" + std::to_string(token.column) + "] Error at '" + token.value + "': " + message;
    return ParseError(errStr);
}

std::vector<StmtPtr> Parser::parse(){
    std::vector<StmtPtr> statements;
    while(peek().type != TokenType::EndOfFile){
        // Each top-level statement gets its own recovery boundary: a
        // ParseError anywhere while parsing it (a missing 'finish', a bad
        // assignment target, etc.) no longer aborts the whole file or
        // leaves the parser silently limping along on bad state — it's
        // reported once, the parser resyncs to the next likely statement
        // start via synchronize(), and parsing continues so later, unrelated
        // errors in the same file are still found in one pass.
        try {
            statements.push_back(statement());
        } catch (const ParseError& e) {
            std::cerr << e.what() << "\n";
            synchronize();
        }
    }
    return statements;
}

bool Parser::check(TokenType type) const{
    return peek().type == type;
}

void Parser::consume(TokenType type, const std::string& message){
    if (peek().type == type){
        advance();
        return;
    }
    throw error(peek(), message);
}

// Advance past tokens until we're positioned at a plausible place to resume
// parsing: right after a 'finish' (the end of whatever construct just went
// wrong), or right before a token that starts a new statement. Without this,
// a single malformed statement leaves `current` wherever consume() first
// gave up, and every following consume()/check() call operates on tokens
// that belong to a construct the parser didn't intend to be looking at.
void Parser::synchronize() {
    while (peek().type != TokenType::EndOfFile) {
        if (peek().type == TokenType::finish) {
            advance();
            return;
        }
        switch (peek().type) {
            case TokenType::loot:
            case TokenType::when:
            case TokenType::grind:
            case TokenType::skill:
            case TokenType::reward:
                return;
            default:
                break;
        }
        advance();
    }
}

StmtPtr Parser::statement() {
    if (check(TokenType::loot))    return letStatement();
    if (check(TokenType::when))    return ifStatement();
    if (check(TokenType::grind))   return whileStatement();
    if (check(TokenType::skill))   return functionDeclaration();
    if (check(TokenType::reward))  return returnStatement();
    if (check(TokenType::breakKw))    return breakStatement();
    if (check(TokenType::continueKw)) return continueStatement();
    return expressionStatement();
}

std::vector<StmtPtr> Parser::block() {
    std::vector<StmtPtr> statements;
    while (!check(TokenType::finish) && !check(TokenType::otherwise) && !check(TokenType::EndOfFile)) {
        statements.push_back(statement());
    }
    return statements;
}

StmtPtr Parser::letStatement(){
    consume(TokenType::loot, "Expected 'loot'");
    Token name = peek();
    consume(TokenType::identifier, "Expected 'identifier' after 'loot'");
    consume(TokenType::equal, "Expected '=' or 'equip'");
    ExpressionPtr initializer = expression();
    return std::make_unique<LetStmt>(name.value, std::move(initializer));
}

StmtPtr Parser::ifStatement() {
    advance(); // 'when'
    consume(TokenType::LeftParen, "Expected '(' after 'when'.");
    ExpressionPtr cond = expression();
    consume(TokenType::RightParen, "Expected ')' after if condition.");
    
    StmtPtr thenBranch = std::make_unique<BlockStmt>(block());
    StmtPtr elseBranch = nullptr;
    
    if (check(TokenType::otherwise)) { 
        advance(); 
        elseBranch = std::make_unique<BlockStmt>(block()); 
    }
    
    consume(TokenType::finish, "Expected 'finish' at end of 'when' block.");
    return std::make_unique<IfStmt>(std::move(cond), std::move(thenBranch), std::move(elseBranch));
}

StmtPtr Parser::whileStatement() {
    advance(); // 'grind'
    consume(TokenType::LeftParen, "Expected '(' after 'grind'.");
    ExpressionPtr cond = expression();
    consume(TokenType::RightParen, "Expected ')' after while condition.");
    
    StmtPtr body = std::make_unique<BlockStmt>(block());
    consume(TokenType::finish, "Expected 'finish' at end of 'grind' block.");
    return std::make_unique<WhileStmt>(std::move(cond), std::move(body));
}

StmtPtr Parser::functionDeclaration() {
    advance(); // 'skill'
    Token name = peek();
    consume(TokenType::identifier, "Expected skill name.");
    consume(TokenType::LeftParen, "Expected '(' after skill name.");
    
    std::vector<std::string> params;
    if (!check(TokenType::RightParen)) {
        // Parse the first parameter
        Token p = peek();
        consume(TokenType::identifier, "Expected parameter name.");
        params.push_back(p.value);
        
        // Safely parse any subsequent parameters
        while (check(TokenType::comma)) {
            advance(); // Safely consume the comma
            Token nextP = peek();
            consume(TokenType::identifier, "Expected parameter name.");
            params.push_back(nextP.value);
        }
    }
    consume(TokenType::RightParen, "Expected ')' after parameters.");
    
    std::vector<StmtPtr> body = block();
    consume(TokenType::finish, "Expected 'finish' at end of 'skill' block.");
    return std::make_unique<FunctionStmt>(name.value, std::move(params), std::move(body));
}

StmtPtr Parser::returnStatement() {
    Token keyword = advance(); // 'reward'
    ExpressionPtr value = nullptr;
    if (!check(TokenType::finish) && !check(TokenType::otherwise) && !check(TokenType::EndOfFile)) {
        value = expression();
    }
    return std::make_unique<ReturnStmt>(keyword, std::move(value));
}

StmtPtr Parser::breakStatement() {
    Token keyword = advance(); // 'break'
    return std::make_unique<BreakStmt>(keyword);
}

StmtPtr Parser::continueStatement() {
    Token keyword = advance(); // 'continue'
    return std::make_unique<ContinueStmt>(keyword);
}

StmtPtr Parser::expressionStatement() {
    ExpressionPtr expr = expression();
    return std::make_unique<ExpressionStmt>(std::move(expr));
}
ExpressionPtr Parser::expression(){
    return assignment();
}
ExpressionPtr Parser::assignment(){
    ExpressionPtr exp = logicOr();
    if(check(TokenType::equal)){
        Token eq = advance();
        ExpressionPtr value = assignment();
        if(auto* var = dynamic_cast<VariableExpression*>(exp.get())){
            return std::make_unique<AssignmentExpression>(var->name, std::move(value));
        }
        if(auto* idx = dynamic_cast<IndexExpression*>(exp.get())){
            return std::make_unique<IndexAssignmentExpression>(
                std::move(idx->array), idx->bracket, std::move(idx->index), std::move(value));
        }
        throw error(eq,"Invalid Assignment Target");
    }

    // Compound assignment (+=, -=, *=, /=): desugared here at parse time
    // into `name = name OP value`, reusing AssignmentExpression and
    // BinaryExpression as-is rather than adding dedicated AST nodes and
    // codegen visitor methods for four more expression kinds.
    //
    // Only plain variables are supported as a target. `arr[i] += v` would
    // need `arr` and `i` evaluated exactly once and the result reused for
    // both the read and the write; the AST's ExpressionPtr children are
    // single-owner/single-use, so desugaring that case the same way would
    // either duplicate side effects (evaluating `arr`/`i` twice) or need a
    // proper temporary — left as a follow-up rather than done here.
    if (check(TokenType::plusequal) || check(TokenType::minusequal) ||
        check(TokenType::starequal) || check(TokenType::slashequal)) {
        Token opEq = advance();
        ExpressionPtr value = assignment();

        auto* var = dynamic_cast<VariableExpression*>(exp.get());
        if (!var) {
            if (dynamic_cast<IndexExpression*>(exp.get())) {
                throw error(opEq, "Compound assignment on array elements isn't supported yet; write 'arr[i] = arr[i] + ...' instead");
            }
            throw error(opEq, "Invalid Assignment Target");
        }

        TokenType binOpType;
        std::string binOpText;
        switch (opEq.type) {
            case TokenType::plusequal:  binOpType = TokenType::Plus;  binOpText = "+"; break;
            case TokenType::minusequal: binOpType = TokenType::Minus; binOpText = "-"; break;
            case TokenType::starequal:  binOpType = TokenType::Star;  binOpText = "*"; break;
            default:                    binOpType = TokenType::Slash; binOpText = "/"; break; // slashequal
        }
        Token binOpToken{binOpType, binOpText, opEq.line, opEq.column};

        ExpressionPtr currentRead = std::make_unique<VariableExpression>(var->name);
        ExpressionPtr combined = std::make_unique<BinaryExpression>(std::move(currentRead), binOpToken, std::move(value));
        return std::make_unique<AssignmentExpression>(var->name, std::move(combined));
    }

    return exp;
}

ExpressionPtr Parser::logicOr(){
    ExpressionPtr exp = logicAnd();
    while(check(TokenType::orr)){
        Token op = advance();
        ExpressionPtr right = logicAnd();
        exp = std::make_unique<LogicalExpression>(std::move(exp),op,std::move(right));
    }
    return exp;
}
ExpressionPtr Parser::logicAnd() {
    ExpressionPtr exp = equality();
    while (check(TokenType::andd)) {
        Token op = advance();
        ExpressionPtr right = equality();
        exp = std::make_unique<LogicalExpression>(std::move(exp), op, std::move(right));
    }
    return exp;
}
ExpressionPtr Parser::equality(){
    ExpressionPtr exp = comparison();
    while(check(TokenType::equalequal) || check(TokenType::bangequal)){
        Token op = advance();
        ExpressionPtr right = comparison();
        exp =   std::make_unique<BinaryExpression>(std::move(exp), op , std::move(right));
    }
    return exp;
}
ExpressionPtr Parser::comparison(){
    ExpressionPtr exp = term();
    while(check(TokenType::less) || check(TokenType::lessequal)
    || check(TokenType::great)|| check(TokenType::greatequal)){
        Token op = advance();
        ExpressionPtr right = term();
        exp = std::make_unique<BinaryExpression>(std::move(exp), op , std::move(right));
    }
    return exp;
}
ExpressionPtr Parser::term(){
    ExpressionPtr exp = factor();
    while(check(TokenType::Plus) || check(TokenType::Minus)){
        Token op = advance();
        ExpressionPtr right = factor();
        exp = std::make_unique<BinaryExpression>(std::move(exp), op , std::move(right));
    }
    return exp;
}
ExpressionPtr Parser::factor(){
    ExpressionPtr exp = unary();
    while(check(TokenType::Star) || check(TokenType::Slash) || check(TokenType::percent)){
        Token op = advance();
        ExpressionPtr right = unary();
        exp = std::make_unique<BinaryExpression>(std::move(exp), op , std::move(right));
    }
    return exp;
}
ExpressionPtr Parser::unary(){

    while(check(TokenType::Minus) || check(TokenType::bang)){
        Token op = advance();
        ExpressionPtr right = unary();
        return std::make_unique<UnaryExpression>( op , std::move(right));
    }
    return call();
}
ExpressionPtr Parser::call() {
    ExpressionPtr exp = primary();
    while (true) {
        if (check(TokenType::LeftParen)) {
            Token paren = advance();
            std::vector<ExpressionPtr> args;

            if (!check(TokenType::RightParen)) {
                // Parse the first argument
                args.push_back(expression());

                while (check(TokenType::comma)) {
                    advance(); // Safely consume the comma
                    args.push_back(expression());
                }
            }
            consume(TokenType::RightParen, "Expected ')' after arguments");
            exp = std::make_unique<CallExpression>(std::move(exp), paren, std::move(args));
        } else if (check(TokenType::LeftBracket)) {
            Token bracket = advance();
            ExpressionPtr indexExpr = expression();
            consume(TokenType::RightBracket, "Expected ']' after array index");
            exp = std::make_unique<IndexExpression>(std::move(exp), bracket, std::move(indexExpr));
        } else {
            break;
        }
    }
    return exp;
}
ExpressionPtr Parser::primary(){
    if(check(TokenType::Number) || check(TokenType::String)){
        Token value = advance();
        return std::make_unique<LiteralExpression>(value);
    }
    if(check(TokenType::identifier)){
        Token value = advance();
        return std::make_unique<VariableExpression>(value.value);
    }
    if(check(TokenType::LeftParen)){
        advance();
        ExpressionPtr inner = expression();
        consume(TokenType::RightParen, "Expected ')' after expression");
        return std::make_unique<GroupingExpression>(std::move(inner));
    }
    if(check(TokenType::LeftBracket)){
        advance();
        std::vector<ExpressionPtr> elements;
        if (!check(TokenType::RightBracket)) {
            elements.push_back(expression());
            while (check(TokenType::comma)) {
                advance();
                elements.push_back(expression());
            }
        }
        consume(TokenType::RightBracket, "Expected ']' after array elements");
        return std::make_unique<ArrayLiteralExpression>(std::move(elements));
    }
    std::cerr << "Syntax Error : expected expression got '" << peek().value
    << "' at" << peek().line << ":" << peek().column << "\n";
    advance();
    return nullptr;
}