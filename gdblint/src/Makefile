# Makefile for gdblint

.DEFAULT_GOAL = all
.ONESHELL:

ifneq ($(INCLUDE_DIR),)
INCLUDE_DIRS += $(INCLUDE_DIR)
endif

ifeq ($(SRC_DIR),)
SRC_DIR := .
endif

ifeq ($(BIN_DIR),)
BIN_DIR := ../bin
endif

HEADERS := $(foreach dir, $(INCLUDE_DIRS), $(wildcard $(dir)/*.h))
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(SRCS:.c=.o)
ELFS := $(foreach obj, $(OBJS:.o=), $(BIN_DIR)/$(notdir $(obj)))
INCLUDES := $(foreach dir, $(INCLUDE_DIRS), -I$(dir))

WARNINGS := -Wall -Wextra -Wpedantic -Werror
ERRORS :=

WARNING_IGNORES := -Wno-implicit-fallthrough
ERROR_IGNORES :=

CFLAGS := $(C_VERSION_FLAGS) $(WARNINGS) $(WARNING_IGNORES) $(ERRORS) $(ERROR_IGNORES)

STRIP_OPTS := -s -R .comment

STRIP_CMD = $(STRIP) $(STRIP_OPTS)

TARGET_NAME := gdblint

TARGET = $(BIN_DIR)/$(TARGET_NAME)

# Command line variable
exe ?= $(TARGET)

.PHONY = all clean strip format valgrind

./%.o: ./%.c $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $< $(LDFLAGS)

$(TARGET): $(OBJS) $(SRCS) $(HEADERS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(OBJS) $(LDFLAGS)

all: $(RELEASE_TARGET) $(TARGET) $(ELFS) $(OBJS) $(SRCS) $(HEADERS)

clean:
	rm -f $(ELFS) $(OBJS)
	rm -rf $(BIN_DIR)

strip: $(TARGET) $(ELFS)
	$(STRIP_CMD) $(exe)

ASTYLE := $(shell command -v astyle 2>/dev/null)

format: $(ASTYLE)
	@if [ -z "$(ASTYLE)" ]; then echo "astyle not found"; exit 1; fi
	$(ASTYLE) --style=google \
		--indent=spaces=2 --indent-switches \
		--max-code-length=80 \
		--add-braces \
		--break-return-type \
		--break-return-type-decl \
		--break-after-logical \
		--convert-tabs \
		$(SRCS)

VALGRIND := $(shell command -v valgrind 2>/dev/null)

valgrind: $(VALGRIND) $(GDBLINT)
	@if [ -z "$(VALGRIND)" ]; then echo "valgrind not found"; exit 1; fi
	@if [ -z "$(gdbfile)" ]; then echo "GDB script file not provided. Use \"$(MAKE) $@ exe=<path> gdbfile=<path>\""; exit 1; fi
	@echo $(shell $(VALGRIND) -s \
		--leak-check=full \
		--show-leak-kinds=all \
		--track-origins=yes \
		--track-fds=yes \
		--max-stackframe=5000000 \
		$(exe) \
		$(gdbfile) 1>&2)
	@$(eval retcode=$(.SHELLSTATUS))
	@echo make valgrind: $(notdir $(exe)) exited with code ${retcode}
