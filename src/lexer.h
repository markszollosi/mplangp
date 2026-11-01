#ifndef LEXER_H
#define LEXER_H

#include "token.h"

typedef struct Lexer Lexer;

Lexer *lexer_new(const char *source);
void lexer_free(Lexer *lx);
Token lexer_next(Lexer *lx);

#endif
