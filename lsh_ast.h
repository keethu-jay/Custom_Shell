#ifndef __LST_AST__H__	
#define __LST_AST__H__	

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <search.h>

struct argv_buf {
	char **argv;
	int argc;
	size_t used;
	size_t capacity;
	char buf[8];	// dynamically grows.
};

struct word {
	const char *text;
	int is_var;
	struct word *next;
};

struct words {
	struct word *first;
	struct word *last;
};

struct program {
	struct words *words;
	struct program *next;
};

struct pipe_stream {
	struct program *first;
	struct program *last;
};

struct statement {
	struct for_loop *for_loop;
	struct conditional *conditional;
	struct pipe_stream *pipe_stream;
	struct var_assign *var_assign;
	int background;
	struct statement *next;
};

struct script {
	struct statement *first;
	struct statement *last;
};

struct conditional_part {
	struct pipe_stream *predicate;
	struct script *if_true_block;
	struct conditional_part *next;
};

struct conditional {
	struct conditional_part *first;
	struct conditional_part *last;
	struct script *else_block;
};

struct for_loop {
	int parallel;
	struct word *var_name;
	struct words *var_values;
	struct script *script;
};

struct var_assign {
	const char *var_name;
	struct words *var_value;	// Kind of a hack to make code simpler, should just be word, not words.
};	

struct context {
	struct script *script;
	void *env_tree;
	void *pid_wait_tree;
};

void context_set_var(struct context *context, const char *key, const char *value);
const char *context_get_var(const struct context *context, const char *key);
int env_tree_compare(const void *_a, const void *_b);
void tsearch_print_env_tree(const void *nodep, VISIT which, int depth);

#define append_ll(a, b)		do { if (a->first == NULL) { a->first = a->last = b; } else { a->last->next = b; a->last = b; b->next = NULL; } } while(0)
#define prepend_ll(a, b)	do { if (a->first == NULL) { a->first = a->last = b; } else { b->next = a->first; a->first = b; } } while(0)

#define CREATE_NEW_FN(x)	static inline struct x *new_##x() { struct x *p = malloc(sizeof(struct x)); memset(p, 0, sizeof(struct x)); return p; }
CREATE_NEW_FN(word)
CREATE_NEW_FN(words)
CREATE_NEW_FN(program)
CREATE_NEW_FN(pipe_stream)
CREATE_NEW_FN(statement)
CREATE_NEW_FN(script)
CREATE_NEW_FN(conditional_part)
CREATE_NEW_FN(conditional)
CREATE_NEW_FN(for_loop)
CREATE_NEW_FN(var_assign)
CREATE_NEW_FN(context)

// Hacks here because the lexer and parser are co-dependent for type definitions.
#define YY_TYPEDEF_YY_SCANNER_T
typedef void * yyscan_t;

void print_words(FILE *f, const struct words *words);
void print_program(FILE *f, const struct program *program, int depth);
void print_pipe_stream(FILE *f, const struct pipe_stream *pipe_stream, int depth);
void print_statement(FILE *f, const struct statement *statement, int depth);
void print_script(FILE *f, const struct script *script, int depth);
void print_var_assign(FILE *f, const struct var_assign *var_assign, int depth);

void free_word(struct word *word);
void free_words(struct words *words);
void free_program(struct program *program);
void free_pipe_stream(struct pipe_stream *pipe_stream);
void free_statement(struct statement *statement);
void free_script(struct script *script);
void free_conditional_part(struct conditional_part *conditional_part);
void free_conditional(struct conditional *conditional);
void free_var_assign(struct var_assign *var_assign);
void free_context(struct context *context);

int run_program(struct context *context, const struct program *program);
int run_pipe_stream(struct context *context, const struct pipe_stream *pipe_stream);
void run_statement(struct context *context, const struct statement *statement);
void run_script(struct context *context, const struct script *script);
void run_conditional(struct context *context, const struct conditional *conditional);

// Turn the actual implementation on.
#define SOLUTION


#endif
