#
# Copyright (C) 2025  notweerdmonk
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
# Makefile for gdblint

.DEFAULT_GOAL = all

CC := gcc
STRIP := strip

C_VERSION_FLAGS := -std=c11

INCLUDE_DIRS := $(shell pwd)

ifneq ($(INCLUDE_DIR),)
INCLUDE_DIRS += $(INCLUDE_DIR)
endif

ifeq ($(SRC_DIR),)
SRC_DIR := $(shell pwd)/src
endif

ifeq ($(DEPS_DIR),)
DEPS_DIR := $(shell pwd)/deps
endif

ifeq ($(BIN_DIR),)
BIN_DIR := $(shell pwd)/bin
endif

ifeq ($(TESTS_DIR),)
TESTS_DIR := $(shell pwd)/tests
endif

ifeq ($(UNIT_TESTS_DIR),)
UNIT_TESTS_DIR := $(shell pwd)/tests/unit
endif

ifeq ($(UNIT_TESTS_BIN_DIR),)
UNIT_TESTS_BIN_DIR := $(shell pwd)/tests/unit/bin
endif

DEPS := $(wildcard $(DEPS_DIR)/*)

WARNINGS := -Wall -Wextra -Wpedantic -Werror
ERRORS :=

WARNING_IGNORES := -Wno-implicit-fallthrough
ERROR_IGNORES :=

CFLAGS := $(C_VERSION_FLAGS) $(WARNINGS) $(WARNING_IGNORES) $(ERRORS) $(ERROR_IGNORES)

STRIP_OPTS := -s -R .comment

STRIP_CMD = $(STRIP) $(STRIP_OPTS)

# Build configuration
ifneq ($(cfg-hashmap-chk-duplicates),)
CFLAGS += -DCFG_HASHMAP_CHK_DUPLICATES
endif

ifneq ($(cfg_hashmap_chk_duplicates),)
CFLAGS += -DCFG_HASHMAP_CHK_DUPLICATES
endif

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

exe ?= $(TARGET)

testexe ?= $(TESTS_DIR)/testrunner

TARGET_NAME := gdblint

TARGET := $(BIN_DIR)/$(TARGET_NAME)

.PHONY = all clean strip unit-tests run-unit-tests test format valgrind help

$(DEPS):
include $(DEPS)

$(TARGET):
	$(MAKE) -C $(SRC_DIR)

all: $(TARGET) $(RELEASE_TARGET) unit-tests

clean:
	$(MAKE) -C $(SRC_DIR) clean
	$(MAKE) -C $(UNIT_TESTS_DIR) clean

strip: $(TARGET)
	$(MAKE) -C $(SRC_DIR) strip

unit-tests:
	$(MAKE) -C $(UNIT_TESTS_DIR)

run-unit-tests:
	@if [ ! -d "$(UNIT_TESTS_BIN_DIR)" ]; then echo "run make unit-tests"; exit 1; fi
	@$(MAKE) -C $(UNIT_TESTS_DIR) test

test: $(TARGET) $(exe) $(testexe)
	$(testexe) $(TESTS_DIR) $(exe)

format:
	$(MAKE) -C $(SRC_DIR) format

valgrind: $(VALGRIND) $(GDBLINT)
	$(MAKE) -C $(SRC_DIR) valgrind

BIN_TARGET := $(notdir $(TARGET))
help:
	@echo "$(BIN_TARGET) build options"
	@echo
	@echo "GOALS"
	@echo "\t$(BIN_TARGET)"
	@echo "\t\tBuild $(BIN_TARGET) executable with intermediates"
	@echo "\tall"
	@echo "\t\tBuild all executables with intermediates"
	@echo "\tclean"
	@echo "\t\tRemove all executables and intermediates"
	@echo
	@echo "\tstrip exe=[PATH]"
	@echo "\t\tRemove all symbols and unecessary sections"
	@echo
	@echo "\t\tVARIABLES"
	@echo "\t\texe\t\tExecutable to strip, $(BIN_TARGET) is set by default"
	@echo
	@echo "\ttest exe=[PATH] testexe=[PATH]"
	@echo "\t\tRun the testsuite on exe with testexe"
	@echo
	@echo "\t\tVARIABLES"
	@echo "\t\texe\t\tExecutable to be tested by testexe, $(BIN_TARGET) is "
	@echo "\t\t\t\tset by default"
	@echo "\t\ttestexe\t\tExecutable used to run tests, testrunner is "
	@echo "\t\t\t\tset by default"
	@echo
	@echo "\tvalgrind exe=[PATH] gdbfile=<PATH>"
	@echo "\t\tRun valgrind on an executable with a GDB script"
	@echo
	@echo "\t\tVARIABLES"
	@echo "\t\texe\t\tExecutable to run valgrind on, $(BIN_TARGET) is set by"
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
	@echo "CONFIGURATION"
	@echo "\tcfg_hashmap_chk_duplicates, cfg-hashmap-chk-duplicates"
	@echo "\t\tEnable checking of duplicate entries in hash map"
	@echo

export
