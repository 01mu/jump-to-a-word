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

all: $(TARGET)

$(TARGET): $(OBJ)
	@echo "Linking $@"
	@$(CC) $(OBJ) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: src/%.c
	@$(MKDIR_P) $(OBJ_DIR)
	@echo "Compiling $<"
	@$(CC) $(CFLAGS) -c $< -o $@

-include $(DEP)

.PHONY: build
build: all

.PHONY: all

.PHONY: help
help:
	@echo 'Usage:'
	@sed -n 's/^##//p' ${MAKEFILE_LIST} | column -t -s ':' | sed -e 's/^/ /'

.PHONY: confirm
confirm:
	@echo -n 'Are you sure? [y/N] ' && read ans && [ $${ans:-N} = y ]

.PHONY: build
build: all

.PHONY: run
run: $(TARGET)
	@./$(TARGET)

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: valgrind
valgrind:
	valgrind ./$(TARGET)

.PHONY: bear
bear: clean
	mkdir -p build && bear --output build/compile_commands.json -- make build

.PHONY: push
push:
	git push origin master
