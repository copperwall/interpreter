package com.craftinginterpreters.lox;

import java.util.HashMap;
import java.util.Map;

class LoxInstance {

    // Why not final?
    private LoxClass klass;
    private Map<String, Object> fields = new HashMap<>();

    LoxInstance(LoxClass klass) {
        this.klass = klass;
    }

    Object get(Token name) {
        if (fields.containsKey(name.lexeme)) {
            return fields.get(name.lexeme);
        }

        // Accessing a method on an instance binds "this"
        // to the method. That method can be passed around with
        // the same "this" at time of access, not at the time of calling.
        LoxFunction method = klass.findMethod(name.lexeme);
        if (method != null) return method.bind(this);

        throw new RuntimeError(
            name,
            "Name '" + name.lexeme + "' not found in object."
        );
    }

    void set(Token name, Object value) {
        fields.put(name.lexeme, value);
    }

    @Override
    public String toString() {
        return klass.name + " instance";
    }
}
