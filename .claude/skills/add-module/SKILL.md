---
name: add-module
description: Scaffold a new native module (modules/NAME.c, docs page, sample, error check) with no hand-written boilerplate.
---

Run `tools/new-module.sh NAME`, then replace the `ping` stub in `modules/NAME.c`, `docs/modules/NAME.md`, `tests/samples/NAME.{my,expected}` and `tests/errors.sh` with the real functions (rules: `modules/README.md`). Finish with `make test`.
