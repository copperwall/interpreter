package com.craftinginterpreters.lox;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

class LoxClass implements LoxCallable {

    final String name;
    final Map<String, LoxFunction> methods;

    LoxClass(String name, Map<String, LoxFunction> methods) {
        this.name = name;
        this.methods = methods;
    }

    LoxFunction findMethod(String name) {
        return methods.getOrDefault(name, null);
    }

    @Override
    public int arity() {
        LoxFunction init = findMethod("init");

        if (init != null) {
            return init.arity();
        }

        return 0;
    }

    @Override
    public String toString() {
        return name;
    }

    @Override
    public Object call(Interpreter interpreter, List<Object> arguments) {
        LoxInstance instance = new LoxInstance(this);

        LoxFunction init = findMethod("init");

        if (init != null) {
            // Call the method after binding it to this instance.
            init.bind(instance).call(interpreter, arguments);
        }

        return instance;
    }
}
