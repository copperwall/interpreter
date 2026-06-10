package com.craftinginterpreters.lox;

import java.util.List;

class Interpreter implements Expr.Visitor<Object>, Stmt.Visitor<Void> {

    private Environment environment = new Environment();

    void interpret(List<Stmt> stmts) {
        try {
            for (Stmt stmt : stmts) {
                execute(stmt);
            }
        } catch (RuntimeError e) {
            Lox.runtimeError(e);
        }
    }

    // Expression visitors

    @Override
    public Object visitLiteralExpr(Expr.Literal expr) {
        return expr.value;
    }

    @Override
    public Object visitGroupingExpr(Expr.Grouping expr) {
        return evaluate(expr.expression);
    }

    @Override
    public Object visitCommaExpr(Expr.Comma expr) {
        // Evaluate each expression, return the value of the last one.

        Object result = null;
        for (Expr subExpr : expr.expressions) {
            result = evaluate(subExpr);
        }

        return result;
    }

    @Override
    public Object visitUnaryExpr(Expr.Unary expr) {
        Object operand = evaluate(expr.right);

        switch (expr.operator.type) {
            case MINUS:
                checkNumberOperand(expr.operator, operand);
                return -(double) operand;
            case BANG:
                return !isTruthy(operand);
        }

        return null;
    }

    @Override
    public Object visitBinaryExpr(Expr.Binary expr) {
        Object left = evaluate(expr.left);
        Object right = evaluate(expr.right);

        switch (expr.operator.type) {
            case GREATER:
                checkNumberOperands(expr.operator, left, right);
                return (double) left > (double) right;
            case GREATER_EQUAL:
                checkNumberOperands(expr.operator, left, right);
                return (double) left >= (double) right;
            case LESS:
                checkNumberOperands(expr.operator, left, right);
                return (double) left < (double) right;
            case LESS_EQUAL:
                checkNumberOperands(expr.operator, left, right);
                return (double) left <= (double) right;
            case BANG_EQUAL:
                checkNumberOperands(expr.operator, left, right);
                return !isEqual(left, right);
            case EQUAL_EQUAL:
                checkNumberOperands(expr.operator, left, right);
                return !isEqual(left, right);
            case MINUS:
                checkNumberOperands(expr.operator, left, right);
                return (double) left - (double) right;
            case STAR:
                checkNumberOperands(expr.operator, left, right);
                return (double) left * (double) right;
            case SLASH:
                checkNumberOperands(expr.operator, left, right);
                if (right.equals(0.0)) {
                    throw new RuntimeError(
                        expr.operator,
                        "Cannot divide by zero"
                    );
                }
                return (double) left / (double) right;
            case PLUS:
                // Support string concat as well
                if (left instanceof Double && right instanceof Double) {
                    return (double) left + (double) right;
                }

                if (left instanceof String && right instanceof String) {
                    return (String) left + (String) right;
                }

                throw new RuntimeError(
                    expr.operator,
                    "Operands must be two numbers or two strings"
                );
        }

        return null;
    }

    @Override
    public Object visitVariableExpr(Expr.Variable expr) {
        return environment.get(expr.name);
    }

    @Override
    public Object visitLogicalExpr(Expr.Logical expr) {
        Object left = evaluate(expr.left);

        if (expr.operator.type == TokenType.OR) {
            if (isTruthy(left)) {
                return left;
            }
        } else {
            if (!isTruthy(left)) {
                return left;
            }
        }

        return evaluate(expr.right);
    }

    @Override
    public Object visitAssignExpr(Expr.Assign expr) {
        Object value = evaluate(expr.value);
        environment.assign(expr.name, value);

        return value;
    }

    // Statement visitors

    @Override
    public Void visitExpressionStmt(Stmt.Expression stmt) {
        // MAYBE_TODO: print statement value when in REPL mode.
        evaluate(stmt.expression);
        return null;
    }

    @Override
    public Void visitIfStmt(Stmt.If stmt) {
        Object condition = evaluate(stmt.condition);

        if (isTruthy(condition)) {
            execute(stmt.thenBranch);
        } else {
            execute(stmt.elseBranch);
        }

        return null;
    }

    @Override
    public Void visitPrintStmt(Stmt.Print stmt) {
        // Evaluate value and then print it?
        Object val = evaluate(stmt.expression);
        System.out.println(stringify(val));
        return null;
    }

    @Override
    public Void visitVarStmt(Stmt.Var stmt) {
        if (stmt.initializer != null) {
            Object val = stmt.initializer.accept(this);
            environment.define(stmt.name.lexeme, val);
        } else {
            environment.define(stmt.name.lexeme);
        }

        return null;
    }

    @Override
    public Void visitWhileStmt(Stmt.While stmt) {
        while (isTruthy(evaluate(stmt.condition))) {
            execute(stmt.body);
        }

        return null;
    }

    @Override
    public Void visitBlockStmt(Stmt.Block stmt) {
        // Add new environment, evalute statements, set environment back to enclosing
        executeBlock(stmt, new Environment(environment));

        return null;
    }

    private void executeBlock(Stmt.Block stmt, Environment env) {
        Environment previous = this.environment;

        try {
            this.environment = env;

            for (Stmt statement : stmt.statments) {
                execute(statement);
            }
        } finally {
            this.environment = previous;
        }
    }

    private Object evaluate(Expr expression) {
        return expression.accept(this);
    }

    private void execute(Stmt stmt) {
        stmt.accept(this);
    }

    private boolean isEqual(Object left, Object right) {
        if (left == null && right == null) {
            return true;
        }

        if (left == null) {
            return false;
        }

        return left.equals(right);
    }

    private String stringify(Object obj) {
        if (obj == null) return "nil";

        if (obj instanceof Double) {
            String objStr = obj.toString();

            // Cut off trailing zero for whole numbers.
            if (objStr.endsWith(".0")) {
                return objStr.substring(0, objStr.length() - 2);
            }

            return objStr;
        }

        return obj.toString();
    }

    private boolean isTruthy(Object obj) {
        if (obj == null) return false;

        if (obj instanceof Boolean) {
            return (boolean) obj;
        }

        return true;
    }

    private void checkNumberOperand(Token operator, Object operand) {
        if (operand instanceof Double) return;

        throw new RuntimeError(operator, "Operand must be a number");
    }

    private void checkNumberOperands(
        Token operator,
        Object left,
        Object right
    ) {
        if (left instanceof Double && right instanceof Double) return;

        throw new RuntimeError(operator, "Both operands must be numbers");
    }
}
