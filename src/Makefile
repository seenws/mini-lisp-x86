CC = gcc
CFLAGS = -Wall -Wextra -std=iso9899:1999 -g -O0 -I. -MMD -MP

BUILD_DIR = build
TARGET = lispc
RUNTIME_OBJ = $(BUILD_DIR)/runtime/runtime.o

OBJECTS = \
	$(BUILD_DIR)/main.o \
	$(BUILD_DIR)/compiler/lexer.o \
	$(BUILD_DIR)/compiler/parser.o \
	$(BUILD_DIR)/compiler/token_stream.o \
	$(BUILD_DIR)/compiler/token_array.o \
	$(BUILD_DIR)/compiler/ast.o \
	$(BUILD_DIR)/compiler/hashmap.o \
	$(BUILD_DIR)/compiler/builtin_hash.o \
	$(BUILD_DIR)/compiler/semantic.o \
	$(BUILD_DIR)/compiler/ir.o \
	$(BUILD_DIR)/compiler/codegen.o \
	$(BUILD_DIR)/util/mlispc_strdup.o \
	$(BUILD_DIR)/util/error.o \
	$(BUILD_DIR)/util/mlispc_strndup.o

all: $(TARGET) $(RUNTIME_OBJ)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@

$(BUILD_DIR)/main.o: main.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/compiler/%.o: compiler/%.c
	@mkdir -p $(BUILD_DIR)/compiler
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/util/%.o: ./util/%.c
	@mkdir -p build/util
	$(CC) $(CFLAGS) -c $< -o $@

$(RUNTIME_OBJ): runtime/runtime.c
	@mkdir -p $(BUILD_DIR)/runtime
	$(CC) $(CFLAGS) -c $< -o $@

# Compile and link a lisp program against the runtime:
#   make tests/stage_1/04_arithmetic.bin && ./tests/stage_1/04_arithmetic.bin
%.bin: %.lisp $(TARGET) $(RUNTIME_OBJ)
	./$(TARGET) $<
	$(CC) $*.s $(RUNTIME_OBJ) -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
	find tests -name '*.s' -delete -o -name '*.bin' -delete

# auto-generated header dependencies (-MMD)
-include $(OBJECTS:.o=.d) $(RUNTIME_OBJ:.o=.d)

.PHONY: all clean

