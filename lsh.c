#include <stdio.h>
#include <string.h>
#include <search.h>

#include <readline/readline.h>

#include "lsh_ast.h"
#include "lsh.yacc.generated_h"
#include "lsh.lex.generated_h"

#define PROMPT	"$ "

// man 7 environ
extern char **environ;

int handle_script(struct context *context) {
	if (context->script) {
		//print_script(stdout, context->script, 0);

		run_script(context, context->script);	

		free_script(context->script);
		context->script = NULL;
	}

	return 0;
}

int main(int argc, char **argv)
{
	struct context *context = new_context();
	int rc;
	FILE *finput = NULL;
	yyscan_t scanner;

	// Load environment into a data structure. These will work as variables for
	// variable expansion, for example 'echo $HOME'.
	for (char **p = environ; p && *p; p++) {
		const char *buf = strdup(*p);
		void *t = tsearch(buf, &context->env_tree, env_tree_compare);
		if (buf != *(const char **)t) free((void*)buf);
	}
	//twalk(context->env_tree, tsearch_print_env_tree);

	// Uncomment to get far more parser generator debug output.
	// This would ordinarily be attached to a flag, but we're not
	// introducing getopt & friends yet for simplicity.
	//yydebug = 1;

	yylex_init(&scanner);

	if (argc == 1 && isatty(0)) {
		// If stdin is a terminal, and no arguments are specified, assume an interactive terminal is desired.
		// Use readline() to provide a pleasant-ish experience.
		char *input;
		yylex_init(&scanner);
		while ((input = readline(PROMPT)) != NULL) {
			yy_switch_to_buffer(yy_scan_string(input, scanner), scanner);
			if ((rc = yyparse(context, scanner)) == 0) {
				rc = handle_script(context);
			}
			free(input);
		}
	} else {
		// Read from a script. By default this is stdin.
		if (argc > 1) {
			// If a file is specified as a command line argument, read from that instead of stdin.
			const char *source = argv[1];
			finput = fopen(source, "rb");
			if (finput == NULL) {
				fprintf(stderr, "Could not open '%s' for reading, errno %d (%s)\n", source, errno, strerror(errno));
				return 1;
			}
			yyset_in(finput, scanner);
		}
		// Parse the input file and run the parsed script if parsing was successful.
		if ((rc = yyparse(context, scanner)) == 0) {
			rc = handle_script(context);
		}
	}
	// Cleanup.
	yylex_destroy(scanner);
	if (finput) fclose(finput);
	free_context(context);
	return rc;
}

