package com.craftinginterpreters.tool;

import java.io.IOException;
import java.io.PrintWriter;
import java.util.Arrays;
import java.util.List;

public class GenerateAst {

    public static void main(String[] args) throws IOException {
        if (args.length != 1) {
            System.err.println("Usage: generate_ast <output directory>");
            System.exit(64);
        }

        String outputDir = args[0];

        // TODO: Add anonymous function expression syntax.
        defineAst(
            outputDir,
            "Expr",
            Arrays.asList(
                "Assign   : Token name, Expr value",
                "Binary   : Expr left, Token operator, Expr right",
                "Grouping : Expr expression",
                "Comma    : List<Expr> expressions",
                "Literal  : Object value",
                "Logical  : Expr left, Token operator, Expr right",
                "Unary    : Token operator, Expr right",
                "Call     : Expr callee, Token paren, List<Expr> arguments",
                "Variable : Token name",
                "Get      : Expr object, Token name",
                "Set      : Expr object, Token name, Expr value"
            )
        );

        defineAst(
            outputDir,
            "Stmt",
            Arrays.asList(
                "Block      : List<Stmt> statments",
                "Expression : Expr expression",
                "If         : Expr condition, Stmt thenBranch, Stmt elseBranch",
                "Function   : Token name, List<Token> params, List<Stmt> body",
                "Return     : Token keyword, Expr value",
                "Print      : Expr expression",
                // Initializer is optional, null if DNE
                "Var        : Token name, Expr initializer",
                "While      : Expr condition, Stmt body",
                "Class      : Token name, List<Stmt.Function> methods"
            )
        );
    }

    private static void defineAst(
        String outputDir,
        String baseName,
        List<String> types
    ) throws IOException {
        String path = outputDir + "/" + baseName + ".java";
        PrintWriter writer = new PrintWriter(path, "UTF-8");

        // Package, imports, abstract class
        writer.println("package com.craftinginterpreters.lox;");
        writer.println();
        writer.println("import java.util.List;");
        writer.println();
        writer.println("abstract class " + baseName + "{");

        defineVisitor(writer, baseName, types);

        // Generate nested classes from the types
        // type string -> "Class : <type> <name>, ..."
        // ...

        for (String typeSpec : types) {
            String[] classToTypes = typeSpec.split(":");

            String subclass = classToTypes[0].trim();
            String classTypes = classToTypes[1].trim();

            StringBuilder fieldDefinitions = new StringBuilder();
            writer.println(
                "static class " + subclass + " extends " + baseName + " {"
            );
            // Constructor start
            writer.println("    " + subclass + "(");
            String[] typePairs = classTypes.split(",");

            writer.print("      ");
            for (int i = 0; i < typePairs.length; i++) {
                String typePair = typePairs[i];

                writer.print(typePair);
                fieldDefinitions.append("   final " + typePair + ";\n");

                if (i != typePairs.length - 1) {
                    writer.print(", ");
                }
            }

            writer.println(") {");

            // constructor body
            for (int i = 0; i < typePairs.length; i++) {
                String typePair = typePairs[i].trim();
                String[] typePairParts = typePair.split(" ");

                System.out.println(typePair);
                String nameStr = typePairParts[1].trim();

                writer.println(
                    "        this." + nameStr + " = " + nameStr + ";"
                );
            }

            // constructor end
            writer.println("    }");

            // Visitor pattern
            writer.println();
            writer.println("    @Override");
            writer.println("    <R> R accept(Visitor<R> visitor) {");
            writer.println(
                "        return visitor.visit" + subclass + baseName + "(this);"
            );
            writer.println("    }");

            // static field definitions
            writer.println(fieldDefinitions.toString());

            writer.println("}");
        }

        // Base accept() method
        writer.println();
        writer.println("    abstract <R> R accept(Visitor<R> visitor);");

        // Need a
        writer.println("}");
        writer.close();
    }

    private static void defineVisitor(
        PrintWriter writer,
        String baseName,
        List<String> types
    ) {
        writer.println("    interface Visitor<R> {");

        for (String type : types) {
            String typeName = type.split(":")[0].trim();
            writer.println(
                "       R visit" +
                    typeName +
                    baseName +
                    "(" +
                    typeName +
                    " " +
                    baseName.toLowerCase() +
                    ");"
            );
        }

        writer.println("    }");
    }
}
