package com.craftinginterpreters.lox;

import java.util.List;

class LoxFunction implements LoxCallable {

    private final Stmt.Function declaration;
    private final Environment closure;
    private final boolean isInitializer;

    LoxFunction(
        Stmt.Function declaration,
        Environment closure,
        boolean isInitializer
    ) {
        this.declaration = declaration;
        this.closure = closure;
        this.isInitializer = isInitializer;
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
            // Early returns from an init method should return this, not null.
            if (isInitializer) {
                return closure.getAt(0, "this");
            }

            return e.value;
        }

        if (isInitializer) return closure.getAt(0, "this");

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

    LoxFunction bind(LoxInstance instance) {
        // Create new environment (don't mutate existing one)
        // Add instance's this to function's environment
        // Return new LoxFunction with that new closure
        Environment env = new Environment(closure);
        env.define("this", instance);
        return new LoxFunction(declaration, env, this.isInitializer);
    }
}
