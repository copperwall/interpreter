package com.craftinginterpreters.lox;

import java.util.ArrayList;
import java.util.List;

class AstPrinter implements Expr.Visitor<String>, Stmt.Visitor<String> {

    String print(Expr expr) {
        return expr.accept(this);
    }

    String print(Stmt stmt) {
        return stmt.accept(this);
    }

    @Override
    public String visitBinaryExpr(Expr.Binary expr) {
        return parenthesize(expr.operator.lexeme, expr.left, expr.right);
    }

    @Override
    public String visitGroupingExpr(Expr.Grouping expr) {
        return parenthesize("group", expr.expression);
    }

    @Override
    public String visitCommaExpr(Expr.Comma expr) {
        return parenthesize("comma", expr.expressions.toArray(new Expr[0]));
    }

    @Override
    public String visitLiteralExpr(Expr.Literal expr) {
        if (expr.value == null) {
            return "nil";
        }

        return expr.value.toString();
    }

    @Override
    public String visitLogicalExpr(Expr.Logical expr) {
        return parenthesize("and", expr.left, expr.right);
    }

    @Override
    public String visitUnaryExpr(Expr.Unary expr) {
        return parenthesize(expr.operator.lexeme, expr.right);
    }

    @Override
    public String visitVariableExpr(Expr.Variable expr) {
        return expr.name.toString();
    }

    @Override
    public String visitAssignExpr(Expr.Assign expr) {
        StringBuilder builder = new StringBuilder();
        builder
            .append("(")
            .append(expr.name.lexeme)
            .append(" = ")
            .append(expr.value.accept(this))
            .append(")");

        return builder.toString();
    }

    @Override
    public String visitPrintStmt(Stmt.Print stmt) {
        return parenthesize("print", stmt.expression);
    }

    @Override
    public String visitExpressionStmt(Stmt.Expression stmt) {
        // TODO: This is pretty printing the token, not the lexeme.
        return parenthesize("statement", stmt.expression);
    }

    @Override
    public String visitIfStmt(Stmt.If stmt) {
        StringBuilder builder = new StringBuilder();
        builder
            .append("(")
            .append("if (")
            .append(stmt.condition.accept(this))
            .append(")")
            .append(stmt.thenBranch.accept(this));

        if (stmt.elseBranch != null) {
            builder.append(stmt.elseBranch.accept(this));
        }

        builder.append(")");

        return builder.toString();
    }

    @Override
    public String visitWhileStmt(Stmt.While stmt) {
        StringBuilder builder = new StringBuilder();
        builder
            .append("(")
            .append("while (")
            .append(stmt.condition.accept(this))
            .append(")")
            .append(stmt.body.accept(this));

        builder.append(")");

        return builder.toString();
    }

    @Override
    public String visitVarStmt(Stmt.Var stmt) {
        StringBuilder builder = new StringBuilder();
        builder.append("(").append("var ").append(stmt.name.lexeme);

        if (stmt.initializer != null) {
            builder.append(" = ").append(stmt.initializer.accept(this));
        }
        builder.append(")");

        return builder.toString();
    }

    @Override
    public String visitBlockStmt(Stmt.Block stmt) {
        return parenthesize("block", stmt.statments.toArray(new Stmt[0]));
    }

    private String parenthesize(String name, Expr... exprs) {
        StringBuilder builder = new StringBuilder();

        builder.append("(").append(name);
        for (Expr expr : exprs) {
            builder.append(" ");
            builder.append(expr.accept(this));
        }
        builder.append(")");

        return builder.toString();
    }

    private String parenthesize(String name, Stmt... stmts) {
        StringBuilder builder = new StringBuilder();

        builder.append("(").append(name);
        for (Stmt stmt : stmts) {
            builder.append(" ");
            builder.append(stmt.accept(this));
        }
        builder.append(")");

        return builder.toString();
    }

    public static void main(String[] args) {
        Expr expression = new Expr.Binary(
            new Expr.Unary(
                new Token(TokenType.MINUS, "-", null, 1),
                new Expr.Literal(123)
            ),
            new Token(TokenType.STAR, "*", null, 1),
            new Expr.Grouping(new Expr.Literal(45.67))
        );

        System.out.println(new AstPrinter().print(expression));
    }
}
