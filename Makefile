CC = clang
CPPFLAGS = -D_DEFAULT_SOURCE
CFLAGS = -Wall -Wextra -Werror -std=c99 -Wvla -Wno-gnu-zero-variadic-macro-arguments -Iinclude
LDFLAGS = -shared
DEBUG_FLAGS = -glldb -Og -fno-omit-frame-pointer
VPATH = src
RM=rm -f
OUTDIR=out
COMMON_SRC=$(wildcard src/common/*.c) $(wildcard src/common/*/*.c)
COMMON_OBJS=$(COMMON_SRC:.c=.o)

all: common

$(OUTDIR):
	mkdir -p $(OUTDIR)

ARENA_SRC=$(wildcard src/arena/*.c) $(wildcard src/arena/*/*.c)
ARENA_LIB = $(OUTDIR)/libarena.so
ARENA_DEBUG_LIB = $(OUTDIR)/libarena_debug.so
ARENA_OBJS=$(ARENA_SRC:.c=.o)

arena: $(ARENA_LIB)
$(ARENA_LIB): CFLAGS += -pedantic -fvisibility=hidden -fPIC -Isrc
$(ARENA_LIB): LDFLAGS += -Wl,--no-undefined
$(ARENA_LIB): $(ARENA_OBJS) $(COMMON_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

arena_debug: $(ARENA_DEBUG_LIB)
$(ARENA_DEBUG_LIB): CFLAGS += $(DEBUG_FLAGS) -D_DEBUG -fvisibility=default -Isrc
$(ARENA_DEBUG_LIB): CFLAGS := $(filter-out -fvisibility=hidden, $(CFLAGS))
$(ARENA_DEBUG_LIB): $(ARENA_OBJS) $(COMMON_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

clean:
	$(RM) $(ARENA_LIB) $(ARENA_OBJS)

.PHONY: all $(ARENA_LIB) clean

###################################################################

TEST_SRC=$(wildcard tests/*.c) $(wildcard tests/*/*.c)
TEST_OBJS=$(TEST_SRC:.c=.o)
test: CFLAGS += $(DEBUG_FLAGS) -D_DEBUG
test: clean $(ARENA_LIB) $(TEST_OBJS)
	$(CC) -o $(OUTDIR)/tests $(TEST_OBJS) -Lout -larena
	LD_LIBRARY_PATH=$(OUTDIR) $(OUTDIR)/tests
