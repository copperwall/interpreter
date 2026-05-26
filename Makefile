SRC_DIR := java
BUILD_DIR := build
MAIN_CLASS := com.craftinginterpreters.lox.Lox
GENERATE_AST_CLASS := com.craftinginterpreters.tool.GenerateAst
SOURCES := $(shell find $(SRC_DIR) -name "*.java")

.PHONY: all clean generate

all: jlox

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/classes: $(SOURCES) | $(BUILD_DIR)
	javac -d $(BUILD_DIR)/classes $(SOURCES)
	@touch $(BUILD_DIR)/classes

jlox: $(BUILD_DIR)/classes
	@printf '#!/bin/sh\nexec java -cp "$(CURDIR)/$(BUILD_DIR)/classes" $(MAIN_CLASS) "$$@"\n' > jlox
	@chmod +x jlox

generate: $(BUILD_DIR)/classes
	java -cp $(BUILD_DIR)/classes $(GENERATE_AST_CLASS) $(SRC_DIR)/com/craftinginterpreters/lox
	$(MAKE) $(BUILD_DIR)/classes

clean:
	rm -rf $(BUILD_DIR) jlox
