#ifndef AST_H
#define AST_H

#include "token.h"
#include "value.h"

typedef enum { EXPR_LITERAL, EXPR_NAME, EXPR_UNARY, EXPR_BINARY, EXPR_LOGICAL, EXPR_CALL, EXPR_ATTR, EXPR_LIST, EXPR_MAP, EXPR_INDEX } ExprType;

typedef struct Expr Expr;
struct Expr {
	ExprType type;
	int line;
	union {
		Value literal;
		struct { const char *name; int length; } name;
		struct { TokenType op; Expr *right; } unary;
		struct { TokenType op; Expr *left; Expr *right; } binary;
		struct { TokenType op; Expr *left; Expr *right; } logical; /* op = TOK_AND/TOK_OR */
		struct { Expr *callee; Expr **args; int nargs; } call;
		struct { Expr *object; const char *name; int length; } attr;
		struct { Expr **items; int n; } list;
		struct { Expr **keys; Expr **vals; int n; } map;
		struct { Expr *object; Expr *index; } index;
	} as;
};

typedef enum { STMT_EXPR, STMT_ASSIGN, STMT_IF, STMT_WHILE, STMT_PRINT, STMT_DEF, STMT_RETURN, STMT_IMPORT, STMT_BREAK, STMT_CONTINUE, STMT_FOR, STMT_SETINDEX } StmtType;

typedef struct Stmt Stmt;
struct Stmt {
	StmtType type;
	int line;
	Stmt *next; /* next statement in the enclosing block (linked list) */
	union {
		Expr *expr;                                   /* STMT_EXPR, STMT_PRINT, STMT_RETURN (may be NULL) */
		struct { const char *name; int length; Expr *value; } assign;
		struct { Expr *cond; Stmt *then_branch; Stmt *else_branch; } if_stmt; /* else_branch may be a single If-stmt for elif, or a block, or NULL */
		struct { Expr *cond; Stmt *body; } while_stmt;
		struct { const char *name; int length; Expr **params; int nparams; Stmt *body; } def;
		struct { const char *name; int length; } import;
		struct { const char *name; int length; Expr *iter; Stmt *body; } for_stmt;
		struct { Expr *object; Expr *index; Expr *value; } setindex;
	} as;
};

typedef struct { Stmt *body; } Program;

Expr *ast_new_literal(Value v, int line);
Expr *ast_new_name(const char *name, int length, int line);
Expr *ast_new_unary(TokenType op, Expr *right, int line);
Expr *ast_new_binary(TokenType op, Expr *left, Expr *right, int line);
Expr *ast_new_logical(TokenType op, Expr *left, Expr *right, int line);
Expr *ast_new_call(Expr *callee, Expr **args, int nargs, int line);
Expr *ast_new_attr(Expr *object, const char *name, int length, int line);
Expr *ast_new_list(Expr **items, int n, int line);
Expr *ast_new_map(Expr **keys, Expr **vals, int n, int line);
Expr *ast_new_index(Expr *object, Expr *index, int line);
Stmt *ast_new_for_stmt(const char *name, int length, Expr *iter, Stmt *body, int line);
Stmt *ast_new_setindex_stmt(Expr *object, Expr *index, Expr *value, int line);
Stmt *ast_new_def_stmt(const char *name, int length, Expr **params, int nparams, Stmt *body, int line);
Stmt *ast_new_return_stmt(Expr *e, int line);
Stmt *ast_new_break_stmt(int line);
Stmt *ast_new_continue_stmt(int line);
Stmt *ast_new_import_stmt(const char *name, int length, int line);
Stmt *ast_new_expr_stmt(Expr *e, int line);
Stmt *ast_new_print_stmt(Expr *e, int line);
Stmt *ast_new_assign_stmt(const char *name, int length, Expr *value, int line);
Stmt *ast_new_if_stmt(Expr *cond, Stmt *then_branch, Stmt *else_branch, int line);
Stmt *ast_new_while_stmt(Expr *cond, Stmt *body, int line);
void expr_free(Expr *e);
void program_free(Program *p); /* frees the whole AST */

#endif
