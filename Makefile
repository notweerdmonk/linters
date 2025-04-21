# Makefile for gdblint

.DEFAULT_GOAL = all
.ONESHELL:

CC ?= gcc
STRIP := strip

ifeq ($(INCLUDE_DIR),)
INCLUDE_DIR := .
endif

ifeq ($(SRC_DIR),)
SRC_DIR := src
endif

ifeq ($(BIN_DIR),)
BIN_DIR := bin
endif

ifeq ($(TESTS_DIR),)
TESTS_DIR := ./tests
endif

HEADERS := $(wildcard $(INCLUDE_DIR)/*.h)
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(SRCS:.c=.o)
ELFS := $(foreach obj, $(OBJS:.o=), $(BIN_DIR)/$(notdir $(obj)))
INCLUDES := $(foreach header, $(HEADERS), -I$(header))

CFLAGS := -Wall -Wextra -Wpedantic -Wno-implicit-fallthrough

STRIP_OPTS := -s -R .comment

STRIP_CMD = $(STRIP) $(STRIP_OPTS)

ifneq (,$(filter-out , $(release) $(RELEASE)))
	CFLAGS += -O3 -D__RELEASE
	LDFLAGS += -Wl,--gc-section -Wl,-s
	RELEASE_TARGET := strip
endif

ifneq (,$(filter-out , $(debug) $(DEBUG)))
	CFLAGS += -ggdb3 -D__DEBUG
endif

ifneq ($(LOG),)
	CFLAGS += -D__LOG=$(LOG)
else ifneq ($(log),)
	CFLAGS += -D__LOG=$(log)
endif

exe ?= $(shell pwd)/$(TARGET)

testexe ?= $(abspath $(TESTS_DIR))/testrunner

./%.o: ./%.c $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $< $(LDFLAGS)

.PHONY = all clean strip test format valgrind help

TARGET_NAME := gdblint

TARGET = $(BIN_DIR)/$(TARGET_NAME)

$(TARGET): $(OBJS) $(SRCS) $(HEADERS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(OBJS) $(LDFLAGS)

all: $(RELEASE_TARGET) $(TARGET) $(ELFS) $(OBJS) $(SRCS) $(HEADERS)

clean:
	rm -f $(ELFS) $(OBJS)
	rm -rf $(BIN_DIR)

strip: $(TARGET) $(ELFS)
	$(STRIP_CMD) $(exe)

test: $(TARGET) $(exe) $(testexe)
	@$(testexe) $(TESTS_DIR) $(exe)

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

help:
	@echo "$(TARGET) build options"
	@echo
	@echo "GOALS"
	@echo "\t$(TARGET)"
	@echo "\t\tBuild $(TARGET) executable with intermediates"
	@echo "\tall"
	@echo "\t\tBuild all executables with intermediates"
	@echo "\tclean"
	@echo "\t\tRemove all executables and intermediates"
	@echo
	@echo "\tstrip exe=[PATH]"
	@echo "\t\tRemove all symbols and unecessary sections"
	@echo
	@echo "\t\tVARIABLES"
	@echo "\t\texe\t\tExecutable to strip, $(TARGET) is set by default"
	@echo
	@echo "\ttest exe=[PATH] testexe=[PATH]"
	@echo "\t\tRun the testsuite on exe with testexe"
	@echo
	@echo "\t\tVARIABLES"
	@echo "\t\texe\t\tExecutable to be tested by testexe, $(TARGET) is "
	@echo "\t\t\t\tset by default"
	@echo "\t\ttestexe\t\tExecutable used to run tests, testrunner is "
	@echo "\t\t\t\tset by default"
	@echo
	@echo "\tvalgrind exe=[PATH] gdbfile=<PATH>"
	@echo "\t\tRun valgrind on an executable with a GDB script"
	@echo
	@echo "\t\tVARIABLES"
	@echo "\t\texe\t\tExecutable to run valgrind on, $(TARGET) is set by"
	@echo "\t\t\t\tdefault"
	@echo "\t\tgdbfile\t\tGDB script to pass to executable"
	@echo
	@echo "\tformat"
	@echo "\t\tFormat all C source files"
	@echo "\thelp"
	@echo "\t\tDisplay this message"
	@echo
	@echo "VARIABLES"
	@echo "\tdebug, DEBUG"
	@echo "\t\tSet to any value to enable debug build configuration"
	@echo "\trelease, RELEASE"
	@echo "\t\tSet to any value to enable release build configuration"
	@echo "\tlog, LOG"
	@echo "\t\tAccepts non-negative integer values"
	@echo
	@echo "LOG LEVELS"
	@echo "\t0\tNo logs"
	@echo "\t1\tErrors"
	@echo "\t2\tWarnings"
	@echo "\t3\tNormal logs"
	@echo "\t>= 4\tDebug logs"
	@echo
