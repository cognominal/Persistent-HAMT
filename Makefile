#
# Project configuration
#
CFGFLAGS := -DWITH_TABLE_CACHE -DWITH_TABLE_CACHE_STATS

#
# Project structure
#
SRCDIRS = src src/hamt src/reprs
INCDIRS := $(shell find $(SRCDIRS) -type d) 3rdparty/MoarVM/src
INCFLAGS := $(addprefix -I,$(INCDIRS))
SRCS := src/hamt/cache.c \
        src/hamt/hamt.c \
        src/hamt/murmur3.c \
        src/hamt/uh.c \
		src/reprs/hamt-node.c
OBJS := $(SRCS:.c=.o)

#
# System info
#
ARCH := $(shell uname)
ifeq ($(ARCH),Darwin)
    LIB := libhamt.dylib
    LDFLAGS = -dynamiclib
else ifeq ($(ARCH),Linux)
    LIB := libhamt.so
    LDFLAGS = -shared
endif

#
# Compiler flags
#
CC     := gcc
CFLAGS := -std=c11 -pedantic -Wall -Werror -Wextra $(INCFLAGS)

#
# Debug build settings
#
DBGDIR = build/debug
DBGTREE = $(addprefix $(DBGDIR)/, $(SRCDIRS))
DBGLIB = $(DBGDIR)/$(LIB)
DBGOBJS = $(addprefix $(DBGDIR)/, $(OBJS))
DBGCFLAGS = -g -O0 -DDEBUG $(CFGFLAGS)
DBGLDFLAGS := $(LDFLAGS)

#
# Release build settings
#
RELDIR = build/release
RELTREE = $(addprefix $(RELDIR)/, $(SRCDIRS))
RELLIB = $(RELDIR)/$(LIB)
RELOBJS = $(addprefix $(RELDIR)/, $(OBJS))
RELCFLAGS = -O3 -DNDEBUG $(CFGFLAGS)
RELLDFLAGS := $(LDFLAGS)

#
# Test build settings
#
TESTDIR = build/debug
TESTSRCS := src/uh.c \
            test/test_hamt.c \
            test/utils.c \
            test/words.c
TESTOBJS := $(TESTSRCS:.c=.o)
TESTOBJS := $(addprefix $(TESTDIR)/, $(TESTOBJS))
TESTEXE := $(addprefix $(TESTDIR)/test/,test_hamt)

.PHONY: all clean debug prep release remake

# Default build to debug
all: prep debug

#
# Debug rules
#
debug: $(DBGLIB)

$(DBGLIB): $(DBGOBJS)
	$(CC) $(DBGLDFLAGS) -o $(DBGLIB) $^

$(DBGDIR)/%.o: %.c
	$(CC) -c $(CFLAGS) $(DBGCFLAGS) -o $@ $<

#
# Release rules
#
release: $(RELLIB)

$(RELLIB): $(RELOBJS)
	$(CC) $(RELLDFLAGS) -o $(RELLIB) $^

$(RELDIR)/%.o: %.c
	$(CC) -c $(CFLAGS) $(RELCFLAGS) -o $@ $<

#
# Test rules
#
runtest: test
	$(TESTEXE)

test: $(TESTEXE)

$(TESTEXE): $(TESTOBJS)
	echo ${TESTOBJS}
	$(CC) -o $(TESTEXE) $^

#
# Other rules
#
prep:
	@mkdir -p $(DBGTREE) $(RELTREE)
	@git submodule init
	@git submodule update

buildMoarVM:
	bin/buildMoarVM

	

remake: clean all

clean:
	rm -f $(RELLIB) $(RELOBJS) $(DBGLIB) $(DBGOBJS) $(TESTEXE) $(TESTOBJS)

distclean:
	rm -r -f build
