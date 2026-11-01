#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "lexer.h"

Program *parse(Lexer *lx);

#endif
