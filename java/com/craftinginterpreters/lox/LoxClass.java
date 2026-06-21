package com.craftinginterpreters.lox;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

class LoxClass implements LoxCallable {

    final String name;
    final Map<String, LoxFunction> methods;
    final LoxClass superclass;

    LoxClass(
        String name,
        LoxClass superclass,
        Map<String, LoxFunction> methods
    ) {
        this.name = name;
        this.methods = methods;
        this.superclass = superclass;
    }

    LoxFunction findMethod(String name) {
        // If no method, check superclasses until you find one.
        if (methods.containsKey(name)) {
            return methods.get(name);
        }

        if (superclass != null) {
            return superclass.findMethod(name);
        }

        return null;
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
