CC = cc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -O2

SRC = util.c value.c lexer.c ast.c parser.c chunk.c table.c compiler.c vm.c
OBJ = $(SRC:.c=.o)

TESTS = value lexer ast parser chunk table compiler vm

# sources each standalone test links against
T_value    = util.c value.c
T_lexer    = util.c lexer.c
T_ast      = util.c value.c ast.c
T_parser   = util.c value.c lexer.c ast.c parser.c
T_chunk    = util.c value.c chunk.c
T_table    = util.c value.c table.c
T_compiler = util.c value.c lexer.c ast.c parser.c chunk.c table.c compiler.c
T_vm       = util.c value.c chunk.c table.c vm.c

all: mplangp

mplangp: main.o $(OBJ)
	$(CC) $(CFLAGS) -o $@ main.o $(OBJ)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

define TESTRULE
tests/test_$(1): tests/test_$(1).c $$(T_$(1))
	$$(CC) $$(CFLAGS) -I. -o $$@ tests/test_$(1).c $$(T_$(1))
endef
$(foreach t,$(TESTS),$(eval $(call TESTRULE,$(t))))

test: mplangp
	@fail=0; \
	for t in $(TESTS); do \
		if $(MAKE) -s tests/test_$$t && ./tests/test_$$t; then \
			echo "PASS: test_$$t"; \
		else \
			echo "FAIL: test_$$t"; fail=1; \
		fi; \
	done; \
	sh tests/run.sh || fail=1; \
	exit $$fail

clean:
	rm -f mplangp main.o $(OBJ) $(TESTS:%=tests/test_%)

.PHONY: all test clean
