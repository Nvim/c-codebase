CC = clang
CPPFLAGS = -D_DEFAULT_SOURCE
CFLAGS = -Wall -Wextra -Werror -std=c99 -Wvla -Wno-gnu-zero-variadic-macro-arguments -Iinclude
LDFLAGS = -shared
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
ARENA_OBJS=$(ARENA_SRC:.c=.o)

arena: $(ARENA_LIB)
$(ARENA_LIB): CFLAGS += -pedantic -fvisibility=hidden -fPIC -Isrc
$(ARENA_LIB): LDFLAGS += -Wl,--no-undefined
$(ARENA_LIB): $(ARENA_OBJS) $(COMMON_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^
#	ar rcs $@ $^

debug: CFLAGS += -glldb -D_DBG -D_LOG -fvisibility=default
debug: CFLAGS := $(filter-out -fvisibility=hidden, $(CFLAGS))
debug: CFLAGS := $(filter-out -fPIC, $(CFLAGS))
debug: LDFLAGS += -glldb -D_DBG -D_LOG
debug: $(ARENA_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

asan: CFLAGS += -fsanitize=address -D_DBG -D_LOG
asan: LDFLAGS += -fsanitize=address -D_DBG -D_LOG
asan: clean $(ARENA_LIB)

clean:
	$(RM) $(ARENA_LIB) $(ARENA_OBJS)

.PHONY: all $(ARENA_LIB) clean

