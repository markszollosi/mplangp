---
name: add-src
description: Scaffold a new interpreter source file (src/NAME.[ch], tests/test_NAME.c, Makefile SRC/TESTS/T_NAME, filemap line).
---

Run `tools/new-src.sh NAME`, fix `T_NAME` in the `Makefile` if the test links more sources, fill in the `.claude/filemap.txt` line, then implement and `make test`.
