package com.craftinginterpreters.lox;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

class Interpreter implements Expr.Visitor<Object>, Stmt.Visitor<Void> {

    final Environment globals = new Environment();
    private Environment environment = globals;
    private final Map<Expr, Integer> locals = new HashMap<>();

    Interpreter() {
        globals.define(
            "clock",
            new LoxCallable() {
                @Override
                public int arity() {
                    return 0;
                }

                @Override
                public Object call(
                    Interpreter interpreter,
                    List<Object> arguments
                ) {
                    return (double) System.currentTimeMillis() / 1000.0;
                }

                @Override
                public String toString() {
                    return "<native fn>";
                }
            }
        );
    }

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
    public Object visitCallExpr(Expr.Call expr) {
        // Evaluate callee
        // Throw if not a function?
        // evaluate args in order
        // apply args to function
        Object callee = evaluate(expr.callee);

        List<Object> args = new ArrayList<>();

        for (Expr argument : expr.arguments) {
            args.add(evaluate(argument));
        }

        if (!(callee instanceof LoxCallable)) {
            throw new RuntimeError(
                expr.paren,
                "Can only call functions and classes."
            );
        }

        LoxCallable function = (LoxCallable) callee;

        if (args.size() != function.arity()) {
            throw new RuntimeError(
                expr.paren,
                "Arity mismatch: Expected " +
                    function.arity() +
                    " but got " +
                    args.size() +
                    "."
            );
        }
        return function.call(this, args);
    }

    @Override
    public Object visitGetExpr(Expr.Get expr) {
        // Evaluate object
        Object obj = evaluate(expr.object);

        // If obj isnt' LoxInstance throw an error.
        if (!(obj instanceof LoxInstance)) {
            throw new RuntimeError(
                expr.name,
                "Cannot read property from non-LoxInstance"
            );
        }

        return ((LoxInstance) obj).get(expr.name);
    }

    @Override
    public Object visitSetExpr(Expr.Set expr) {
        Object obj = evaluate(expr.object);

        if (!(obj instanceof LoxInstance)) {
            throw new RuntimeError(
                expr.name,
                "Cannot set value '" + expr.name.lexeme + "' on a non-instance"
            );
        }

        Object val = evaluate(expr.value);

        ((LoxInstance) obj).set(expr.name, val);
        return val;
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
        return lookUpVariable(expr.name, expr);
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

        Integer distance = locals.get(expr);

        if (distance == null) {
            globals.assign(expr.name, value);
        } else {
            environment.assignAt(distance, expr.name, value);
        }

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
        } else if (stmt.elseBranch != null) {
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

    // Do a binding,
    @Override
    public Void visitFunctionStmt(Stmt.Function stmt) {
        // Evaluate to LoxFunction
        LoxFunction fn = new LoxFunction(stmt, environment);

        // Place LoxFunction in environment
        environment.define(stmt.name.lexeme, fn);
        return null;
    }

    @Override
    public Void visitReturnStmt(Stmt.Return stmt) {
        Object value = null;

        if (stmt.value != null) {
            value = evaluate(stmt.value);
        }

        throw new Return(value);
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
        executeBlock(stmt.statments, new Environment(environment));

        return null;
    }

    @Override
    public Void visitClassStmt(Stmt.Class stmt) {
        environment.define(stmt.name.lexeme, null);

        Map<String, LoxFunction> methods = new HashMap<>();
        for (Stmt.Function method : stmt.methods) {
            LoxFunction function = new LoxFunction(method, environment);
            methods.put(method.name.lexeme, function);
        }

        LoxClass klass = new LoxClass(stmt.name.lexeme, methods);
        environment.assign(stmt.name, klass);
        return null;
    }

    void executeBlock(List<Stmt> stmts, Environment env) {
        Environment previous = this.environment;

        try {
            this.environment = env;

            for (Stmt statement : stmts) {
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

    void resolve(Expr expr, int depth) {
        locals.put(expr, depth);
    }

    private Object lookUpVariable(Token name, Expr expr) {
        Integer distance = locals.get(expr);

        if (distance == null) {
            return globals.get(name);
        } else {
            return environment.getAt(distance, name.lexeme);
        }
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
