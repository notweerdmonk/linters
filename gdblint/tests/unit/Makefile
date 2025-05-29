# Makefile for unit tests

.DEFAULT_GOAL = all
.ONESHELL:

ifneq ($(INCLUDE_DIR),)
INCLUDE_DIRS += $(INCLUDE_DIR)
endif

ifeq ($(UNIT_SRC_DIR),)
UNIT_SRC_DIR := $(shell pwd)
endif

ifeq ($(UNIT_TEST_BIN_DIR),)
UNIT_TEST_BIN_DIR := $(shell pwd)/bin
endif

HEADERS := $(foreach dir, $(INCLUDE_DIRS), $(wildcard $(dir)/*.h))
SRCS := $(wildcard $(UNIT_SRC_DIR)/*.c)
OBJS := $(SRCS:.c=.o)
ELFS := $(foreach obj, $(OBJS:.o=), $(UNIT_TEST_BIN_DIR)/$(notdir $(obj)))
INCLUDES := $(foreach dir, $(INCLUDE_DIRS), -I$(dir))

WARNINGS :=
ERRORS :=

WARNING_IGNORES :=
ERROR_IGNORES :=

CFLAGS += -DCFG_UNIT_TESTS -Wno-unused-function

STRIP_OPTS := -s -R .comment

STRIP_CMD = $(STRIP) $(STRIP_OPTS)

.PHONY = all clean strip test format valgrind

./%.o: ./%.c $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -I $(SRC_DIR) -include gdblint.c  -c -o $@ $< $(LDFLAGS)

$(ELFS): $(OBJS) $(SRCS) $(HEADERS)
	mkdir -p $(UNIT_TEST_BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(OBJS) $(LDFLAGS)

all: $(ELFS) $(OBJS) $(SRCS) $(HEADERS)

clean:
	rm -f $(ELFS) $(OBJS)
	rm -rf $(UNIT_TEST_BIN_DIR)

strip: $(ELFS)
	$(STRIP_CMD) $(ELFS)

test: $(ELFS)
	@echo
	@ret=0; \
	for file in $(ELFS); \
	do \
	  echo "Running $$file\n"; \
		$$file && ret=$$(($$ret && $$?)); \
	done
	@echo

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

valgrind: $(VALGRIND)
	@if [ -z "$(VALGRIND)" ]; then echo "valgrind not found"; exit 1; fi
	@if [ -z "$(exe)" ]; then echo "GDB script file not provided. Use \"$(MAKE) $@ exe=<path> gdbfile=<path>\""; exit 1; fi
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
