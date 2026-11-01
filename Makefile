CC = cc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -O2

# native modules: modules/<name>.c -> modules/<name>.so
MODSRC = $(wildcard modules/*.c)
MODOUT = $(MODSRC:.c=.so)
ifeq ($(shell uname),Darwin)
SHARED = -dynamiclib
LDLIBS =
else
SHARED = -shared -fPIC
LDLIBS = -ldl -lm
endif

SRC = util.c value.c function.c module.c lexer.c ast.c parser.c chunk.c table.c compiler.c vm.c
OBJ = $(SRC:%.c=build/%.o)

TESTS = value lexer ast parser chunk table compiler vm

# sources each standalone test links against
CORE = src/util.c src/value.c src/function.c src/module.c src/chunk.c
T_value    = $(CORE)
T_lexer    = src/util.c src/lexer.c
T_ast      = $(CORE) src/ast.c
T_parser   = $(CORE) src/lexer.c src/ast.c src/parser.c
T_chunk    = $(CORE)
T_table    = $(CORE) src/table.c
T_compiler = $(CORE) src/lexer.c src/ast.c src/parser.c src/table.c src/compiler.c
T_vm       = $(CORE) src/table.c src/vm.c

all: mplangp modules

mplangp: build/main.o $(OBJ)
	$(CC) $(CFLAGS) -o $@ build/main.o $(OBJ) $(LDLIBS)

modules: $(MODOUT)

modules/%.so: modules/%.c src/mplangpfunc.h src/value.h
	$(CC) $(CFLAGS) $(SHARED) -Isrc -o $@ $< -lm

build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c -o $@ $<

define TESTRULE
tests/test_$(1): tests/test_$(1).c $$(T_$(1))
	$$(CC) $$(CFLAGS) -Isrc -o $$@ tests/test_$(1).c $$(T_$(1)) $$(LDLIBS) -lm
endef
$(foreach t,$(TESTS),$(eval $(call TESTRULE,$(t))))

test: mplangp modules
	@fail=0; \
	for t in $(TESTS); do \
		if $(MAKE) -s tests/test_$$t && ./tests/test_$$t; then \
			echo "PASS: test_$$t"; \
		else \
			echo "FAIL: test_$$t"; fail=1; \
		fi; \
	done; \
	sh tests/run.sh || fail=1; \
	sh tests/errors.sh || fail=1; \
	exit $$fail

clean:
	rm -rf mplangp build $(MODOUT) $(TESTS:%=tests/test_%)

.PHONY: all modules test clean
