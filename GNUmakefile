CFLAGS += -O3
CFLAGS += -s
CFLAGS += -std=gnu2x

CFLAGS += -Wall
CFLAGS += -Wextra
CFLAGS += -Warray-bounds
CFLAGS += -Wno-unused-value
CFLAGS += -Wno-type-limits
CFLAGS += -Wconversion
CFLAGS += -Wmissing-braces
CFLAGS += -Wno-parentheses
CFLAGS += -Wpedantic
CFLAGS += -Wstrict-prototypes
CFLAGS += -Wwrite-strings
CFLAGS += -Winline

# Quieten warning about GCC seeing a one-liner function as un-inlinable.
CFLAGS += -Wno-attributes

CFLAGS += -fsanitize=float-cast-overflow
CFLAGS += -fsanitize=address
CFLAGS += -fsanitize=undefined
CFLAGS += -fsanitize=leak

CLANG_CFLAGS += -fsanitize=function
CLANG_CFLAGS += -fsanitize=implicit-unsigned-integer-truncation
CLANG_CFLAGS += -fsanitize=implicit-signed-integer-truncation
CLANG_CFLAGS += -fsanitize=implicit-integer-sign-change
CLANG_CFLAGS += -Wreserved-identifier

GCC_CFLAGS += -Wformat-signedness
GCC_CFLAGS += -Wsuggest-attribute=pure
GCC_CFLAGS += -Wsuggest-attribute=const
GCC_CFLAGS += -Wsuggest-attribute=noreturn
GCC_CFLAGS += -Wsuggest-attribute=cold
GCC_CFLAGS += -Wsuggest-attribute=malloc
GCC_CFLAGS += -Wsuggest-attribute=format

GCC_CFLAGS += -fanalyzer

# Detect if CC is clang or gcc, in this order.
COMPILER_VERSION := $(shell $(CC) --version)

ifneq '' '$(findstring clang,$(COMPILER_VERSION))'
  CFLAGS += $(CLANG_CFLAGS)
else ifneq '' '$(findstring gcc,$(COMPILER_VERSION))'
  CFLAGS += $(GCC_CFLAGS)
endif

SRCS   := unix-copy-file.c test-unix-copy-file.c
TARGET := tests

test: $(TARGET)
	./$(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	$(RM) $(TARGET)

.PHONY: test clean 
.DELETE_ON_ERROR:
