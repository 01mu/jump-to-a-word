default: help

CC = gcc
MKDIR_P = mkdir -p

SRC = $(wildcard src/*.c)
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
TARGET = $(BUILD_DIR)/jump-to-a-word.so

OBJ = $(patsubst src/%.c, $(OBJ_DIR)/%.o, $(SRC))
DEP = $(OBJ:.o=.d)

CFLAGS = -g -Wall -fPIC -MMD -MP `pkg-config --cflags geany`
LDFLAGS = -shared `pkg-config --libs geany`

## help: print this help message
.PHONY: help
help:
	@echo 'Usage:'
	@sed -n 's/^##//p' ${MAKEFILE_LIST} | column -t -s ':' | sed -e 's/^/ /'

## build: build target
.PHONY: build
build: $(TARGET)

$(TARGET): $(OBJ)
	@echo "Linking $@"
	@$(CC) $(OBJ) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: src/%.c
	@$(MKDIR_P) $(OBJ_DIR)
	@echo "Compiling $<"
	@$(CC) $(CFLAGS) -c $< -o $@

-include $(DEP)

## bear: create compile_comands.json for the language server protocol
.PHONY: bear
bear: clean
	@mkdir -p build && bear --output build/compile_commands.json -- make build

## format: format files
.PHONY: format
format:
	@cd src && find . -name "*.c" -o -name "*.h" | xargs clang-format -i
