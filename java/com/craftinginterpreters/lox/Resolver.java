package com.craftinginterpreters.lox;

import com.craftinginterpreters.lox.Expr.Binary;
import com.craftinginterpreters.lox.Expr.Call;
import com.craftinginterpreters.lox.Expr.Comma;
import com.craftinginterpreters.lox.Expr.Grouping;
import com.craftinginterpreters.lox.Expr.Literal;
import com.craftinginterpreters.lox.Expr.Logical;
import com.craftinginterpreters.lox.Expr.Unary;
import com.craftinginterpreters.lox.Stmt.Class;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Stack;

class Resolver implements Expr.Visitor<Void>, Stmt.Visitor<Void> {

    private class ResolveEntry {

        Boolean beenRead;
        Boolean isSet;
        final Token token;

        ResolveEntry(Boolean isSet, Token token) {
            this.isSet = isSet;
            this.token = token;
            beenRead = false;
        }
    }

    private final Interpreter interpreter;
    private final Stack<Map<String, ResolveEntry>> scopes = new Stack<>();
    private FunctionType currentFunction = FunctionType.NONE;

    Resolver(Interpreter interpreter) {
        this.interpreter = interpreter;
    }

    @Override
    public Void visitBlockStmt(Stmt.Block stmt) {
        beginScope();
        resolve(stmt.statments);
        checkForUnusedLocals();
        endScope();
        return null;
    }

    @Override
    public Void visitVarStmt(Stmt.Var stmt) {
        /*
            Edge case:
            var a = "global";
            {
                // Here we define it as an error to reference
                // the variable in its definition
                // we don't use the outer scoped variable in the expression.
                var a = a + " something";

            }
        */

        declare(stmt.name);
        if (stmt.initializer != null) {
            resolve(stmt.initializer);
        }

        define(stmt.name);
        return null;
    }

    @Override
    public Void visitGetExpr(Expr.Get expr) {
        resolve(expr.object);
        return null;
    }

    @Override
    public Void visitSetExpr(Expr.Set expr) {
        resolve(expr.object);
        resolve(expr.value);
        return null;
    }

    @Override
    public Void visitVariableExpr(Expr.Variable expr) {
        if (
            !scopes.empty() &&
            scopes.peek().containsKey(expr.name.lexeme) &&
            scopes.peek().get(expr.name.lexeme).isSet == Boolean.FALSE
        ) {
            Lox.error(
                expr.name.line,
                "Variable cannot be referenced in its definition."
            );
        }

        resolveLocal(expr, expr.name);
        return null;
    }

    @Override
    public Void visitAssignExpr(Expr.Assign expr) {
        resolve(expr.value);
        resolveLocal(expr, expr.name);
        return null;
    }

    @Override
    public Void visitFunctionStmt(Stmt.Function stmt) {
        declare(stmt.name);
        define(stmt.name);

        resolveFunction(stmt, FunctionType.FUNCTION);
        return null;
    }

    @Override
    public Void visitExpressionStmt(Stmt.Expression stmt) {
        resolve(stmt.expression);
        return null;
    }

    @Override
    public Void visitIfStmt(Stmt.If stmt) {
        resolve(stmt.condition);
        resolve(stmt.thenBranch);

        if (stmt.elseBranch != null) {
            resolve(stmt.elseBranch);
        }

        return null;
    }

    @Override
    public Void visitPrintStmt(Stmt.Print stmt) {
        resolve(stmt.expression);
        return null;
    }

    @Override
    public Void visitReturnStmt(Stmt.Return stmt) {
        if (currentFunction == FunctionType.NONE) {
            Lox.error(stmt.keyword.line, "Cannot return outside of function.");
        }
        if (stmt.value != null) {
            resolve(stmt.value);
        }
        return null;
    }

    @Override
    public Void visitWhileStmt(Stmt.While stmt) {
        resolve(stmt.condition);
        resolve(stmt.body);
        return null;
    }

    @Override
    public Void visitBinaryExpr(Expr.Binary expr) {
        resolve(expr.left);
        resolve(expr.right);
        return null;
    }

    @Override
    public Void visitGroupingExpr(Expr.Grouping expr) {
        resolve(expr.expression);
        return null;
    }

    @Override
    public Void visitCommaExpr(Expr.Comma expr) {
        for (Expr subExpr : expr.expressions) {
            resolve(subExpr);
        }
        return null;
    }

    @Override
    public Void visitLiteralExpr(Expr.Literal expr) {
        return null;
    }

    @Override
    public Void visitLogicalExpr(Expr.Logical expr) {
        resolve(expr.left);
        resolve(expr.right);
        return null;
    }

    @Override
    public Void visitUnaryExpr(Expr.Unary expr) {
        resolve(expr.right);
        return null;
    }

    @Override
    public Void visitCallExpr(Expr.Call expr) {
        resolve(expr.callee);
        for (Expr arg : expr.arguments) {
            resolve(arg);
        }

        return null;
    }

    @Override
    public Void visitClassStmt(Class stmt) {
        declare(stmt.name);
        define(stmt.name);

        for (Stmt.Function method : stmt.methods) {
            FunctionType declaration = FunctionType.METHOD;
            resolveFunction(method, declaration);
        }

        return null;
    }

    private void resolveFunction(
        Stmt.Function func,
        FunctionType functionType
    ) {
        FunctionType enclosingFunction = currentFunction;
        currentFunction = functionType;
        beginScope();
        for (Token param : func.params) {
            declare(param);
            define(param);
        }

        resolve(func.body);
        checkForUnusedLocals();
        endScope();
        currentFunction = enclosingFunction;
    }

    private void resolveLocal(Expr expr, Token name) {
        // Look for variable in each scope
        for (int i = scopes.size() - 1; i >= 0; i--) {
            Map<String, ResolveEntry> scope = scopes.get(i);
            if (scope.containsKey(name.lexeme)) {
                // Mark as read
                scope.get(name.lexeme).beenRead = true;
                // Resolve in interpreter
                interpreter.resolve(expr, scopes.size() - i - 1);
                return;
            }
        }
    }

    private void beginScope() {
        scopes.push(new HashMap<String, ResolveEntry>());
    }

    private void endScope() {
        scopes.pop();
    }

    /**
     * Declaring a variable and never reading it
     */
    private void checkForUnusedLocals() {
        Map<String, ResolveEntry> scope = scopes.peek();

        for (Map.Entry<String, ResolveEntry> entry : scope.entrySet()) {
            ResolveEntry resolveEntry = entry.getValue();
            if (!resolveEntry.beenRead) {
                Lox.error(
                    resolveEntry.token.line,
                    "Unused variable '" + resolveEntry.token.lexeme + "'."
                );
            }
        }
    }

    private void declare(Token name) {
        if (scopes.empty()) {
            return;
        }

        Map<String, ResolveEntry> scope = scopes.peek();
        if (scope.containsKey(name.lexeme)) {
            Lox.error(
                name.line,
                "Cannot redeclare variable '" + name.lexeme + "'."
            );
        }
        scope.put(name.lexeme, new ResolveEntry(false, name));
    }

    private void define(Token name) {
        if (scopes.empty()) {
            return;
        }

        scopes.peek().put(name.lexeme, new ResolveEntry(true, name));
    }

    void resolve(List<Stmt> statements) {
        for (Stmt statement : statements) {
            resolve(statement);
        }
    }

    private void resolve(Stmt stmt) {
        stmt.accept(this);
    }

    private void resolve(Expr expr) {
        expr.accept(this);
    }

    private enum FunctionType {
        NONE,
        FUNCTION,
        METHOD,
    }
}
