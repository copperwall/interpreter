package com.craftinginterpreters.lox;

import static com.craftinginterpreters.lox.TokenType.AND;
import static com.craftinginterpreters.lox.TokenType.BANG;
import static com.craftinginterpreters.lox.TokenType.BANG_EQUAL;
import static com.craftinginterpreters.lox.TokenType.COMMA;
import static com.craftinginterpreters.lox.TokenType.ELSE;
import static com.craftinginterpreters.lox.TokenType.EOF;
import static com.craftinginterpreters.lox.TokenType.EQUAL;
import static com.craftinginterpreters.lox.TokenType.EQUAL_EQUAL;
import static com.craftinginterpreters.lox.TokenType.FALSE;
import static com.craftinginterpreters.lox.TokenType.FOR;
import static com.craftinginterpreters.lox.TokenType.GREATER;
import static com.craftinginterpreters.lox.TokenType.GREATER_EQUAL;
import static com.craftinginterpreters.lox.TokenType.IDENTIFIER;
import static com.craftinginterpreters.lox.TokenType.IF;
import static com.craftinginterpreters.lox.TokenType.LEFT_BRACE;
import static com.craftinginterpreters.lox.TokenType.LEFT_PAREN;
import static com.craftinginterpreters.lox.TokenType.LESS;
import static com.craftinginterpreters.lox.TokenType.LESS_EQUAL;
import static com.craftinginterpreters.lox.TokenType.MINUS;
import static com.craftinginterpreters.lox.TokenType.NIL;
import static com.craftinginterpreters.lox.TokenType.NUMBER;
import static com.craftinginterpreters.lox.TokenType.OR;
import static com.craftinginterpreters.lox.TokenType.PLUS;
import static com.craftinginterpreters.lox.TokenType.PRINT;
import static com.craftinginterpreters.lox.TokenType.RIGHT_BRACE;
import static com.craftinginterpreters.lox.TokenType.RIGHT_PAREN;
import static com.craftinginterpreters.lox.TokenType.SEMICOLON;
import static com.craftinginterpreters.lox.TokenType.SLASH;
import static com.craftinginterpreters.lox.TokenType.STAR;
import static com.craftinginterpreters.lox.TokenType.STRING;
import static com.craftinginterpreters.lox.TokenType.TRUE;
import static com.craftinginterpreters.lox.TokenType.VAR;
import static com.craftinginterpreters.lox.TokenType.WHILE;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

class Parser {

    private static class ParseError extends RuntimeException {}

    private final List<Token> tokens;
    private int current = 0;

    Parser(List<Token> tokens) {
        this.tokens = tokens;
    }

    List<Stmt> parse() {
        ArrayList<Stmt> stmts = new ArrayList<>();

        while (!isAtEnd()) {
            stmts.add(declaration());
        }

        return stmts;
    }

    private Stmt declaration() {
        try {
            if (match(VAR)) return varDeclaration();
            return statement();
        } catch (ParseError error) {
            synchronize();
            return null;
        }
    }

    private Stmt varDeclaration() {
        // Already matched var
        // Read in identifier

        Token name = consume(IDENTIFIER, "Expected variable name");
        Expr initializer = null;
        if (match(EQUAL)) {
            // NOTE: Not saying comma here because that seems confusing to have
            // as an initializer expression.
            initializer = expression();
        }

        consume(SEMICOLON, "Expected semicolon");

        return new Stmt.Var(name, initializer);
    }

    private Stmt statement() {
        if (match(PRINT)) {
            return printStatement();
        }

        if (match(IF)) {
            return ifStatement();
        }

        // for "(" varDecl | exprStmt | ";" expression? ";" expression? ")" statement ;
        if (match(FOR)) {
            return forStatement();
        }

        if (match(WHILE)) {
            return whileStatement();
        }

        if (match(LEFT_BRACE)) {
            return new Stmt.Block(block());
        }

        return expressionStatement();
    }

    private Stmt forStatement() {
        consume(LEFT_PAREN, "Expected '(' after 'for'.");

        Stmt initializer;
        // First is either a varDevl or exprStmt or ";"
        if (match(SEMICOLON)) {
            initializer = null;
        } else if (match(VAR)) {
            initializer = varDeclaration();
        } else {
            initializer = expressionStatement();
        }

        // Optional condition, non-optional semicolon
        Expr condition = new Expr.Literal(true);
        if (!check(SEMICOLON)) {
            condition = comma();
        }

        consume(SEMICOLON, "';' expected after optional condition.");

        Expr increment = null;
        if (!check(RIGHT_PAREN)) {
            increment = comma();
        }

        consume(RIGHT_PAREN, "')' expected after optional increment.");
        Stmt forBody = statement();

        // whileBody needs to be a block, forBody may be a single statement.
        // This makes a new block and places the forBody with a followup increment.
        List<Stmt> whileBodyStmts = new ArrayList<>();
        whileBodyStmts.add(forBody);
        if (increment != null) {
            whileBodyStmts.add(new Stmt.Expression(increment));
        }

        Stmt whileBody = new Stmt.Block(whileBodyStmts);

        Stmt whileStmt = new Stmt.While(condition, whileBody);

        List<Stmt> loopStmts = new ArrayList<>();

        if (initializer != null) {
            loopStmts.add(initializer);
        }
        loopStmts.add(whileStmt);
        // to desugar, need to return an AST node that represents
        return new Stmt.Block(loopStmts);
    }

    // while ( expression ) statement ;
    private Stmt whileStatement() {
        consume(LEFT_PAREN, "Expected '(' after 'while'.");
        Expr condition = comma();
        consume(RIGHT_PAREN, "Expected ')' after while condition.");
        Stmt body = statement();

        return new Stmt.While(condition, body);
    }

    private Stmt ifStatement() {
        // Matched if already
        consume(LEFT_PAREN, "Expected '(' after 'if'.");
        Expr condition = comma();
        consume(RIGHT_PAREN, "Expected ')' after if condition.");

        Stmt thenBranch = statement();
        Stmt elseBranch = null;

        if (match(ELSE)) {
            elseBranch = statement();
        }

        return new Stmt.If(condition, thenBranch, elseBranch);
    }

    private Stmt printStatement() {
        Expr expr = comma();
        consume(SEMICOLON, "Expected semicolon after print statement");
        return new Stmt.Print(expr);
    }

    private List<Stmt> block() {
        // Consumed opening brace already
        List<Stmt> stmts = new ArrayList<>();

        // Consume declarations until we hit a RIGHT_BRACE
        while (!isAtEnd() && !check(RIGHT_BRACE)) {
            stmts.add(declaration());
        }

        consume(RIGHT_BRACE, "Expected '}' after block");
        return stmts;
    }

    private Stmt expressionStatement() {
        // Parse expression (already consumed print token)
        // consume semicolon
        // return print statement

        // MAYBE_TODO: If in the repl, don't require a semicolon
        Expr expr = comma();
        consume(SEMICOLON, "Expected semicolon after statement");
        return new Stmt.Expression(expr);
    }

    // comma

    private Expr comma() {
        List<Expr> exprs = new ArrayList<>();
        exprs.add(expression());

        while (match(COMMA)) {
            exprs.add(expression());
        }

        if (exprs.size() == 1) {
            return exprs.get(0);
        }
        return new Expr.Comma(exprs);
    }

    private Expr expression() {
        // Could place the comma operator here
        return assignment();
    }

    // Parse equality
    private Expr assignment() {
        Expr expr = or();

        // If the next token is equal, make sure name is l-value.
        if (match(EQUAL)) {
            Token equals = previous();
            Expr value = assignment();

            if (expr instanceof Expr.Variable) {
                Token name = ((Expr.Variable) expr).name;

                return new Expr.Assign(name, value);
            }

            error(equals, "Invalid assignment type");
        }

        return expr;
    }

    private Expr or() {
        Expr expr = and();

        while (match(OR)) {
            Token operator = previous();
            Expr right = and();
            expr = new Expr.Logical(expr, operator, right);
        }

        return expr;
    }

    private Expr and() {
        Expr expr = equality();

        while (match(AND)) {
            Token operator = previous();
            Expr right = equality();

            expr = new Expr.Logical(expr, operator, right);
        }

        return expr;
    }

    private Expr equality() {
        Expr expr = comparison();

        // While the next token is either of these,
        // get the previous token as the operator (since match advances)
        // recurse to parse the right hand operand, create a new binary expression
        // Replace expr with new expression, placing original expr as the left hand value.
        // This makes equality a left-associative expression.
        while (match(BANG_EQUAL, EQUAL_EQUAL)) {
            Token operator = previous();
            Expr right = comparison();
            expr = new Expr.Binary(expr, operator, right);
        }

        return expr;
    }

    private Expr comparison() {
        Expr expr = term();

        while (match(GREATER, GREATER_EQUAL, LESS, LESS_EQUAL)) {
            Token operator = previous();
            Expr right = term();
            expr = new Expr.Binary(expr, operator, right);
        }

        return expr;
    }

    private Expr term() {
        Expr expr = factor();

        while (match(MINUS, PLUS)) {
            Token operator = previous();
            Expr right = factor();
            expr = new Expr.Binary(expr, operator, right);
        }

        return expr;
    }

    private Expr factor() {
        Expr expr = unary();

        while (match(SLASH, STAR)) {
            Token operator = previous();
            Expr right = factor();
            expr = new Expr.Binary(expr, operator, right);
        }

        return expr;
    }

    // unary -> (! | -) unary;
    // unary -> primary;
    private Expr unary() {
        if (match(BANG, MINUS)) {
            Token operator = previous();
            Expr right = unary();

            return new Expr.Unary(operator, right);
        }

        return primary();
    }

    private Expr primary() {
        if (match(FALSE)) return new Expr.Literal(false);
        if (match(TRUE)) return new Expr.Literal(true);
        if (match(NIL)) return new Expr.Literal(null);

        if (match(IDENTIFIER)) {
            Token prev = previous();
            return new Expr.Variable(prev);
        }

        if (match(NUMBER, STRING)) {
            Token prev = previous();
            return new Expr.Literal(prev.literal);
        }

        if (match(LEFT_PAREN)) {
            Expr expr = expression();
            // consume right paren
            consume(RIGHT_PAREN, "Expect ')' after expression.");
            return new Expr.Grouping(expr);
        }

        throw error(peek(), "Expect expression");
    }

    // If the current token matches any tokentype, advance and return true.
    private boolean match(TokenType... types) {
        for (TokenType type : types) {
            if (check(type)) {
                advance();
                return true;
            }
        }

        return false;
    }

    private boolean check(TokenType type) {
        if (isAtEnd()) return false;

        return peek().type == type;
    }

    private boolean isAtEnd() {
        return peek().type == EOF;
    }

    private Token peek() {
        return this.tokens.get(this.current);
    }

    private Token previous() {
        return this.tokens.get(this.current - 1);
    }

    private Token advance() {
        if (!isAtEnd()) this.current++;
        return previous();
    }

    private ParseError error(Token token, String message) {
        Lox.error(token.line, message);
        return new ParseError();
    }

    private void synchronize() {
        advance();

        // Advance the parser until we're at a new starting point to continue parsing.
        while (!isAtEnd()) {
            if (previous().type == SEMICOLON) return;

            switch (peek().type) {
                case CLASS:
                case FUN:
                case VAR:
                case FOR:
                case IF:
                case WHILE:
                case PRINT:
                case RETURN:
                    return;
            }

            advance();
        }
    }

    private Token consume(TokenType expected, String errorMsg) {
        if (check(expected)) return advance();
        throw error(peek(), errorMsg);
    }
}
