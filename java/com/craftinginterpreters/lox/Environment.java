package com.craftinginterpreters.lox;

import java.util.HashMap;
import java.util.Map;

class Environment {

    private static final Object UNINITIALIZED = new Object();
    final Environment enclosing;
    private final Map<String, Object> values = new HashMap<>();

    Environment() {
        enclosing = null;
    }

    Environment(Environment enclosing) {
        this.enclosing = enclosing;
    }

    void define(String name) {
        // Defines a new variable, but marks as uninitialized so that gets will fail.
        // if not initialized.
        values.put(name, UNINITIALIZED);
    }

    // Redefining variable is allowed currently.
    void define(String name, Object value) {
        values.put(name, value);
    }

    // Argument is Token instead of string to more easily create
    // a useful error on an undefined variable.
    Object get(Token name) {
        if (values.containsKey(name.lexeme)) {
            Object val = values.get(name.lexeme);

            if (val == UNINITIALIZED) {
                throw new RuntimeError(
                    name,
                    "Variable '" + name.lexeme + "' is not yet initialized."
                );
            }
            return values.get(name.lexeme);
        }

        if (enclosing != null) return enclosing.get(name);

        throw new RuntimeError(
            name,
            "Undefined variable '" + name.lexeme + "'."
        );
    }

    Object getAt(Integer distance, String name) {
        return ancestor(distance).values.get(name);
    }

    Environment ancestor(Integer distance) {
        Environment env = this;
        for (int i = 0; i < distance; i++) {
            env = env.enclosing;
        }

        return env;
    }

    void assign(Token name, Object value) {
        if (values.containsKey(name.lexeme)) {
            values.put(name.lexeme, value);
            return;
        }

        if (enclosing != null) {
            enclosing.assign(name, value);
            return;
        }

        throw new RuntimeError(
            name,
            "Undefined variable '" + name.lexeme + "'."
        );
    }

    void assignAt(Integer distance, Token name, Object value) {
        ancestor(distance).values.put(name.lexeme, value);
    }

    void debug() {
        System.out.println(values);
        if (enclosing != null) {
            System.out.println("Parent");
            enclosing.debug();
        }
    }
}
