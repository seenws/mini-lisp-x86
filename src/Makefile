# Compiler
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -O0 -I.

# Output
TARGET = lispc
BUILD_DIR = build

OBJECTS = \
	$(BUILD_DIR)/main.o \
	$(BUILD_DIR)/compiler/lexer.o

all: $(TARGET)

# Link
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@

# Compile main
$(BUILD_DIR)/main.o: main.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile lexer
$(BUILD_DIR)/compiler/lexer.o: compiler/lexer.c
	@mkdir -p $(BUILD_DIR)/compiler
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

run: all
	./$(TARGET) ../tests/stage_1/test.lisp

.PHONY: all clean run

