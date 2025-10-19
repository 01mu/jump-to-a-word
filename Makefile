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

.PHONY: confirm
confirm:
	@echo -n 'Are you sure? [y/N] ' && read ans && [ $${ans:-N} = y ]

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

## run: run target
.PHONY: run
run: $(TARGET)
	@./$(TARGET)

## clean: remove build files
.PHONY: clean
clean:
	@rm -rf $(BUILD_DIR)

## valgrind: run valgrind on target
.PHONY: valgrind
valgrind:
	@valgrind ./$(TARGET)

## bear: create compile_comands.json for the language server protocol
.PHONY: bear
bear: clean
	@mkdir -p build && bear --output build/compile_commands.json -- make build

## tidy: clang-tidy lint a file
.PHONY: tidy
tidy:
	@clang-tidy src/${FILE} -p build/ --fix

## lint: cppcheck lint a file
.PHONY: check
check:
	@cppcheck --language=c --enable=warning,style --check-level=exhaustive --template=gcc "src/${FILE}"

## format: format files
.PHONY: format
format:
	@cd src && find . -name "*.c" -o -name "*.h" | xargs clang-format -i

## push: push to origin
.PHONY: push
push:
	@git push origin master
