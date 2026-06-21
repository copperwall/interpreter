package com.craftinginterpreters.lox;

import java.util.List;

class LoxFunction implements LoxCallable {

    private final Stmt.Function declaration;
    private final Environment closure;

    LoxFunction(Stmt.Function declaration, Environment closure) {
        this.declaration = declaration;
        this.closure = closure;
    }

    @Override
    public Object call(Interpreter interpreter, List<Object> arguments) {
        // Create new environment
        Environment environment = new Environment(closure);

        // Create new bindings for each parameter and argument match
        for (int i = 0; i < this.declaration.params.size(); i++) {
            environment.define(
                this.declaration.params.get(i).lexeme,
                arguments.get(i)
            );
        }

        // Execute body with new bindings
        try {
            interpreter.executeBlock(this.declaration.body, environment);
        } catch (Return e) {
            return e.value;
        }

        return null;
    }

    @Override
    public int arity() {
        return declaration.params.size();
    }

    @Override
    public String toString() {
        return "<fn " + this.declaration.name.lexeme + ">";
    }
}
