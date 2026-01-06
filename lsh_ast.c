// Functions you need to implement are labeled below

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <search.h>
#include <ctype.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "lsh_ast.h"

// Forward declarations.
/*static*/ void context_pid_wait_tree_add(struct context *context, int pid);
/*static*/ void context_empty_pid_wait_tree(struct context *context);

static int _env_tree_compare(const char *a, const char *b) {
	while (1) {
		if (a == b) return 0;
		int adone = a == 0 || *a == 0 || *a == '=';
		int bdone = b == 0 || *b == 0 || *b == '=';
		if (adone && bdone) return 0;
		if (bdone) return -1;
		if (adone) return 1;
		if (*a < *b) return -1;
		if (*a > *b) return 1;
		a++; b++;
	}
}

int env_tree_compare(const void *a, const void *b) {
	int rc = _env_tree_compare(a, b);
	return rc;
}

int pid_wait_tree_compare(const void *a, const void *b) {
	uintptr_t ap = (uintptr_t)a;
	uintptr_t bp = (uintptr_t)b;
	if (ap < bp) return -1;
	else if (ap == bp) return 0;
	else return -1;
}

void tsearch_print_env_tree(const void *nodep, VISIT which, int depth)
{
	const char *datap;
	(void)depth;
	datap = *(const char **) nodep;
	if (which == postorder || which == leaf) {
		printf("%d %p %s\n", which, nodep, datap);
	}
}


void space(FILE *f, int depth) {
	for (int i = 0; i < depth; i++)
		fprintf(f, "  ");
}

void print_words(FILE *f, const struct words *words) {
	int i = 0;
	for (const struct word *w = words->first; w != NULL; w = w->next) {
		fprintf(f, "%s%s", i++ ? " " : "", w->text);
	}
}

void print_program(FILE *f, const struct program *program, int depth) {
	space(f, depth);
	fprintf(f, "program: ");
	print_words(f, program->words);
	fprintf(f, "\n");
}

void print_pipe_stream(FILE *f, const struct pipe_stream *pipe_stream, int depth) {
	if (pipe_stream->first == pipe_stream->last) {
		print_program(f, pipe_stream->first, depth);
		return;
	}
	space(f, depth);
	fprintf(f, "pipe_stream:\n");
	for (const struct program *p = pipe_stream->first; p != NULL; p = p->next) {
		print_program(f, p, depth + 1);
	}
}	

void print_conditional(FILE *f, const struct conditional *conditional, int depth) {
	int i = 0;
	for (const struct conditional_part *cp = conditional->first; cp != NULL; cp = cp->next) {
		space(f, depth);
		fprintf(f, "%sif:\n", i++ ? "el" : "");
		print_pipe_stream(f, cp->predicate, depth + 1);
		space(f, depth);
		fprintf(f, "then:\n");
		print_script(f, cp->if_true_block, depth + 1);
	}
	if (conditional->else_block != NULL) {
		space(f, depth);
		fprintf(f, "else:\n");
		print_script(f, conditional->else_block, depth + 1);
	}	
}

void print_for_loop(FILE *f, const struct for_loop *for_loop, int depth) {
	space(f, depth);
	fprintf(f, "for %s in ", for_loop->var_name->text);
	print_words(f, for_loop->var_values);
	fprintf(f, "; %sdo\n", for_loop->parallel ? "parallel " : "");
	print_script(f, for_loop->script, depth + 1);
}

void print_var_assign(FILE *f, const struct var_assign *var_assign, int depth) {
	space(f, depth);
	fprintf(f, "var_assign: %s = ", var_assign->var_name);
	print_words(f, var_assign->var_value);
	fprintf(f, "\n");
}

void print_statement(FILE *f, const struct statement *statement, int depth) {
	if (statement->for_loop != NULL) {
		print_for_loop(f, statement->for_loop, depth);
	}
	if (statement->conditional != NULL) {
		print_conditional(f, statement->conditional, depth);	
	}
	if (statement->pipe_stream != NULL) {
		print_pipe_stream(f, statement->pipe_stream, depth);
	}
	if (statement->var_assign != NULL) {
		print_var_assign(f, statement->var_assign, depth);	
	}
}

void print_script(FILE *f, const struct script *script, int depth) {
	space(f, depth);
	fprintf(f, "script:\n");
	for (const struct statement *s = script->first; s != NULL; s = s->next) {
		print_statement(f, s, depth + 1);
	}
}

#define FREE_LL(t, x)	do { struct t *p = x->first; while (p) { struct t *next = p->next; free_##t(p); p = next; } } while(0)

void free_word(struct word *word) {
	free((void *) word->text);
	free(word);
}

void free_words(struct words *words) {
	FREE_LL(word, words);
	free(words);
}

void free_program(struct program *program) {
	free_words(program->words);
	free(program);
}

void free_pipe_stream(struct pipe_stream *pipe_stream) {
	FREE_LL(program, pipe_stream);
	free(pipe_stream);
}

void free_for_loop(struct for_loop *for_loop) {
	free_word(for_loop->var_name);
	free_words(for_loop->var_values);
	free_script(for_loop->script);
	free(for_loop);
}

void free_var_assign(struct var_assign *var_assign) {
	free((void *)var_assign->var_name);
	free_words(var_assign->var_value);
	free(var_assign);
}

void free_statement(struct statement *statement) {
	if (statement->for_loop)
		free_for_loop(statement->for_loop);
	if (statement->conditional)
		free_conditional(statement->conditional);
	if (statement->pipe_stream)
		free_pipe_stream(statement->pipe_stream);
	if (statement->var_assign)
		free_var_assign(statement->var_assign);
	free(statement);
}

void free_script(struct script *script) {
	FREE_LL(statement, script);
	free(script);
}

void free_conditional_part(struct conditional_part *conditional_part) {
	free_pipe_stream(conditional_part->predicate);
	free_script(conditional_part->if_true_block);
	free(conditional_part);
}

void free_conditional(struct conditional *conditional) {
	FREE_LL(conditional_part, conditional);
	if (conditional->else_block)
		free_script(conditional->else_block);
	free(conditional);
}

static void context_empty_env_tree(struct context *context) {
	while (context->env_tree != NULL) {
		const char *e = *(const char **)context->env_tree;
		tdelete(e, &context->env_tree, env_tree_compare);
		free((void *)e);
	}
}

void free_context(struct context *context) {
	context_empty_env_tree(context);
	context_empty_pid_wait_tree(context);
	free(context);
}

static struct argv_buf *argv_buf_expand(struct argv_buf *buf) {
	buf->capacity <<= 1;
	struct argv_buf *ret = realloc(buf, sizeof(*buf) + buf->capacity);
	if (ret == NULL) {
		fprintf(stderr, "realloc() failed for argv_buf!\n");
		exit(1);
	}
	return ret;
}

// Add a single char to the argv buf, expanding if needed.
static struct argv_buf *argv_buf_putchar(struct argv_buf *buf, char c) {
	if (buf->used >= buf->capacity - 1) {
		buf = argv_buf_expand(buf);
	}
	buf->buf[buf->used++] = c;
	return buf;
}

// Add an entire string to the argv buf plus a null terminator, expanding if needed.
static struct argv_buf *argv_buf_puts(struct argv_buf *buf, const char *s) {
	for (const char *c = s; c && *c; c++)
		buf = argv_buf_putchar(buf, *c);
	buf = argv_buf_putchar(buf, 0);
	return buf;
}

struct argv_buf *make_argv(const struct context *context, const struct words *words) {
	struct argv_buf *buf = malloc(sizeof(*buf));
	buf->argv = 0;
	buf->argc = 0;
	buf->used = 0;
	buf->capacity = sizeof(buf->buf);

	for (const struct word *word = words->first; word != NULL; word = word->next) {
		if (word->is_var) {
			const char *var = context_get_var(context, word->text);
			if (var) {
				buf = argv_buf_puts(buf, var);	// FIXME
			}
		} else {
			buf = argv_buf_puts(buf, word->text);
		}
	}

	// Argv in a separate allocation, for pointer stability of the underlying string
	// data if a vector expansion occurs due to argv.
	int max_argc = 2;
	buf->argv = malloc(sizeof(char *) * (max_argc + 1));

	int in_word = 0;
	for (size_t i = 0; i < buf->used; i++) {
		char *c = &buf->buf[i];
		if (buf->argc == max_argc - 1) {
			max_argc <<= 1;
			buf->argv = realloc(buf->argv, sizeof(char *) * (max_argc + 1));
		}
		if (!in_word && !isspace(*c) && *c != 0) {
			in_word = 1;
			buf->argv[buf->argc++] = c;
		} else if (in_word && (isspace(*c) || *c == 0)) {
			in_word = 0;
			*c = 0;	// convert spaces to null termination for argv.
		} else if (!in_word) {
			*c = 0;
		}
	}
	buf->argv[buf->argc] = NULL;
	return buf;
}

void free_argv(struct argv_buf *buf) {
	if (buf->argv) free(buf->argv);
	free(buf);
}

void run_conditional(struct context *context, const struct conditional *conditional) {
	for (const struct conditional_part *cp = conditional->first; cp != NULL; cp = cp->next) {
		int rc = run_pipe_stream(context, cp->predicate);
		if (rc == 0) {
			// take this block and return.
			run_script(context, cp->if_true_block);
			return;
		}
	}
	if (conditional->else_block != NULL) {
		run_script(context, conditional->else_block);
	}
}

void run_for_loop(struct context *context, const struct for_loop *for_loop) {
	struct argv_buf *buf = make_argv(context, for_loop->var_values);

	for (int i = 0; i < buf->argc; i++) {
		context_set_var(context, for_loop->var_name->text, buf->argv[i]);
		run_script(context, for_loop->script);
	}

	free_argv(buf);
}

void run_var_assign(struct context *context, const struct var_assign *var_assign) {
	struct argv_buf *buf = make_argv(context, var_assign->var_value);

	context_set_var(context, var_assign->var_name, buf->argv[0]);

	free_argv(buf);
}

void run_fg_statement(struct context *context, const struct statement *statement) {
	if (statement->conditional)
		run_conditional(context, statement->conditional);
	if (statement->pipe_stream)
		run_pipe_stream(context, statement->pipe_stream);
	if (statement->for_loop)
		run_for_loop(context, statement->for_loop);
	if (statement->var_assign)
		run_var_assign(context, statement->var_assign);
}

/******************************************************************************************************
 *                                                                                                    *
 * Start of functions for you to implement. You will likely want to use other functions in this file. *
 *                                                                                                    *
 ******************************************************************************************************/


/*static*/ void context_pid_wait_tree_add(struct context *context, int pid) {
	tsearch((void*)(uintptr_t)pid, &context->pid_wait_tree, pid_wait_tree_compare);
}

struct pid_list {
   pid_t pid; // The PID being stored in this node of the list
   struct pid_list *next; // Pointer to the next node in the linked list
};

 void collect_pids(const void *nodep, const VISIT which) {
    static struct pid_list *head = NULL;  // keeps track of the head of the linked list
    static struct pid_list *current = NULL;  // keeps track of the current node in the linked list

    // Dereference nodep to get the PID value
    pid_t pid = *((pid_t *) nodep);  // Assuming nodep is a pointer to pid_t

    // Check the visit type to find out what the action is
    // VISIT enum values indicate when to perform actions in tree traversal
    if (which == postorder || which == leaf) {
        // Allocate memory for a new node in the linked list
        struct pid_list *new_node = malloc(sizeof(struct pid_list));
	// Initialize the new node with the PID and set its next pointer to NUL
        new_node->pid = pid;
        new_node->next = NULL;

	// If the list is empty, initialize the head and current pointers to the new node
        if (head == NULL) {
            head = new_node;
            current = new_node;
        } else {
	    // append the new node to the end of the list
            current->next = new_node;
            current = new_node;
        }
    }
}

/*static*/ void context_empty_pid_wait_tree(struct context *context) {
	// This loop will iterate through all pids inserted into pid_wait_tree.
	while (context->pid_wait_tree != NULL) {
	        // Extract the PID from the tree node
	        // Dereference the void pointer to get the integer value of the PID
		int pid = *(int *)context->pid_wait_tree;
		// Remove the PID from the pid_wait_tree
		// Convert the PID integer to a void pointer for the tree function
		tdelete((void *)(uintptr_t)pid, &context->pid_wait_tree, pid_wait_tree_compare);
		// Wait on the child process with process id 'pid'.
	        // Wait for the child process with process id 'pid'.
		int status;
		// Call waitpid to wait for the child process with the given PID
		if (waitpid(pid, &status, 0) == -1) {
		  perror("waitpid");
		  // Handle the error continuing with other processes
		} else {
		  // Check the status of the child process to determine how it terminated
		  // returns true if the child process terminated normally
		  if (WIFEXITED(status)) {
		    // Print the PID and exit status of the child process
		    printf("Child %d exited with status %d\n", pid, WEXITSTATUS(status));
		    // Returns true if the child process was terminated by a signal
		  } else if (WIFSIGNALED(status)) {
		    // Print the PID and the signal number that caused the termination
		    printf("Child %d terminated by signal %d\n", pid, WTERMSIG(status));
		  }
		}
	}
}

// Determines if the given command (string) is an intrinsic command (see sections 3 and 4)
// Return 1 if the command is an intrinsic and 0 otherwise
int is_builtin(const char *argv0) {
	if (strcmp(argv0, "exit") == 0) return 1;
	// Your code goes here (Sections 4 & 5)
	// Check if the command is 'cd' (change directory)
	if (strcmp(argv0, "cd") == 0) return 1;
	
	// Check if the command is 'jobs' (list background jobs)
	if (strcmp(argv0, "jobs") == 0) return 1;
	
	if (strcmp(argv0, "help") == 0) return 1;
	// return 0 if the command is not a built-in
	return 0;
}

void print_pids(const void *nodep, const VISIT which, int depth) {
    (void)depth;  // Suppress unused parameter warning

    // Check the type of visit to determine if the function should process the current node.
    // 'leaf' means the node is a leaf node in the tree.
    // 'postorder' means the function should process the node after processing it's children
    if (which == leaf || which == postorder) {
        // Cast nodep to a pointer to struct pid_list *
        // nodep is a pointer to a struct pid_list *
        const struct pid_list *node = *(const struct pid_list **)nodep;
	// Print the PID of the background job
        printf("Background job: PID %d\n", node->pid);
    }
}


// Handle an intrinsic command. See run_one_program for how this function will be used.
// Takes in context, instrinsic command + arguments, and the length of argv
// Hint: which system call can change the current working directory of a process?
// Hint: the home directory is in the environment variable 'HOME'
int handle_builtin(struct context *context, char **argv, int argc) {
	if (strcmp(argv[0], "exit") == 0) {
		exit(0);
	}

	// Your code goes here (Sections 4 & 5)
	if (strcmp(argv[0], "cd") == 0) {
	  // If no argument provided, go to the home directory
	  if (argc < 2 || argv[1] == NULL) {
	    const char *home = getenv("HOME");
	    if (home == NULL) {
               fprintf(stderr, "cd: HOME not set\n");
               return EINVAL;  // Error if HOME is not set
	    }
            if (chdir(home) != 0) {
               perror("cd");  // Print error if chdir fails
               return errno;  // Return error code
            }
       } else {
           // Change to the specified directory
           if (chdir(argv[1]) != 0) {
               perror("cd");  // Print error if chdir fails
               return errno;  // Return error code
           }
       }
       return 0;  // Return 0 on success
   }
   // Handle 'jobs'
   if (strcmp(argv[0], "jobs") == 0) {
        // List all background jobs stored in pid_wait_tree
        if (context->pid_wait_tree == NULL) {
            printf("No background jobs.\n");
            return 0;
        }

	// traverse and print background jobs
        twalk(context->pid_wait_tree, print_pids);
        return 0;
    }

    // Handle 'help'
    if (strcmp(argv[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  exit: Exit the shell\n");
        printf("  cd [dir]: Change the current directory to [dir]\n");
        printf("  jobs: List background jobs\n");
        printf("  help: Display this help message\n");
        return 0;
    }


	return EINVAL;
}


// Run one command. If the command is an intrinsic (like 'cd'), it will be handled by handle_builtin.
// Otherwise, you need to write code to handle it here in a child subprocess.
// Hint: The shell (parent) needs to wait for the child to finish executing.
int run_one_program(struct context *context, const struct program *program) {
        // Declare a variable to hold the return code
	int rc;
	// Create an argument vector (argv)
	// Converts the program's words into an array of arguments
	struct argv_buf *argv = make_argv(context, program->words);

	// If this is a builtin, run it, otherwise, fork and exec.
	if (is_builtin(argv->argv[0])) {
		rc = handle_builtin(context, argv->argv, argv->argc);
		goto out;
	}

	// Your code goes here (Section 3)
	// Fork a child process to run the command
	pid_t pid = fork();
	if (pid == -1) {
        perror("fork");
        rc = -1;
        goto out;
	}
	if (pid == 0) { // Child process
        // Execute the command using execvp
        execvp(argv->argv[0], argv->argv);
        // If execvp returns, it must have failed
        perror("execvp");
        exit(EXIT_FAILURE);
    }  else { // Parent process
        // Wait for the child process to complete
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            rc = -1;
        } else {
            if (WIFEXITED(status)){
	        // If the child process exited normally, get the exit status code
                rc = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
	      // If the child process was terminated by a signal, print a message with the signal number
                printf("Child %d terminated by signal %d\n", pid, WTERMSIG(status));
                rc = -1;
            } else {
	        // If the child process ended abnormally
                rc = -1;
            }
        }
    }


out:
	// Clean up the argument vector
	free_argv(argv);
	return rc;
}

// Run a command in the background (spawn as a child process but do not wait)
// Note: need to keep track of all background child PIDs in case the user wants to call wait
void run_bg_statement(struct context *context, const struct statement *statement) {
    // Your code goes here (Section 5)
    // Fork a child process to run the command
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return;
    }
    if (pid == 0) { // Child process
        // Detach from terminal and avoid blocking the parent shell
        if (setsid() == -1) { // Create a new session
            perror("setsid");
            exit(EXIT_FAILURE);
        }
        // Execute the command
        run_fg_statement(context, statement);
        exit(EXIT_SUCCESS);
    } else { // Parent process
       // Add the PID to the context's pid_wait_tree to track background processes
        context_pid_wait_tree_add(context, pid);
        printf("Started background job: PID %d\n", pid);
    }

}

// Execute the pipe stream of commands, which is two commands chained together with a pipe (|)
// i.e. cat /usr/share/dict/words | grep ^z.*o$
// See the pipe_steam struct in lsh_ast.h. It contains the command before the pipe and the command after the pipe.
// run_pipe_stream returns the status code of the last member of the pipe. 0 = success, anything else is failure.
// Hint: see 'man pipe' to create a pipe between processes
// Hint: see 'man dup2' for making one file descriptor (i.e. stdin or stdout) point to another.
int run_pipe_stream(struct context *context, const struct pipe_stream *pipe_stream) {
   int rc = 0;  // Initialize return code
    int pipe_fds[2];
    int prev_fd = -1;  // Previous pipe's read end
    pid_t pid;
    const struct program *current_program = pipe_stream->first;

    while (current_program != NULL) {
        // Create a pipe
        if (pipe(pipe_fds) == -1) {
            perror("pipe");
            return -1;
        }

        // Fork a child process
        pid = fork();
        if (pid == -1) {
            perror("fork");
            return -1;
        }

        if (pid == 0) { // Child process
            if (prev_fd != -1) {
                // Redirect stdin to the previous pipe's read end
                if (dup2(prev_fd, STDIN_FILENO) == -1) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                close(prev_fd);
            }

            if (current_program->next != NULL) {
                // Redirect stdout to the current pipe's write end
                if (dup2(pipe_fds[1], STDOUT_FILENO) == -1) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }

            // Close unused file descriptors
            close(pipe_fds[0]); // Close read end
            close(pipe_fds[1]); // Close write end

            // Execute the current command
            struct argv_buf *argv = make_argv((const struct context *)context, current_program->words);
            execvp(argv->argv[0], argv->argv);
            perror("execvp");
            exit(EXIT_FAILURE);
        } else { // Parent process
            // Close the write end of the current pipe
            close(pipe_fds[1]);

            if (prev_fd != -1) {
                // Close the previous pipe's read end
                close(prev_fd);
            }

            // Set the current pipe's read end as the previous pipe's read end for the next iteration
            prev_fd = pipe_fds[0];

            // Move to the next command in the pipe stream
            current_program = current_program->next;
        }
    }

    // After all commands are forked, close the last pipe's read end in the parent
    if (prev_fd != -1) {
        close(prev_fd);
    }

    // Wait for all child processes to complete
    // This simplistic implementation waits only for the last child
    // For a more robust solution, you should track all PIDs and wait for each
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        return -1;
    }

    if (WIFEXITED(status)) {
        rc = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        printf("Child %d terminated by signal %d\n", pid, WTERMSIG(status));
        rc = -1;
    } else {
        rc = -1;
    }

    return rc;



}


/******************************************
 *                                        *
 * End of functions for you to implement. *
 *                                        *
 ******************************************/

void run_statement(struct context *context, const struct statement *statement) {
	if (statement->background) {
		run_bg_statement(context, statement);
	} else {
		run_fg_statement(context, statement);
	}
}

void run_script(struct context *context, const struct script *script) {
	for (const struct statement *s = script->first; s; s = s->next) {
		run_statement(context, s);
	}
}

static const char *context_get_var_raw(const struct context *context, const char *key) {
	void *t = tfind(key, &context->env_tree, env_tree_compare);
	if (t == NULL) {
		return NULL;
	}
	const char *s = *(const char **)t;
	return s;
}

const char *context_get_var(const struct context *context, const char *key) {
	const char *s = context_get_var_raw(context, key);
	return &s[strlen(key) + 1];
}

void context_set_var(struct context *context, const char *key, const char *value) {
	value = value ? value : "";
	char *buf = malloc(strlen(key) + strlen(value) + 2);
	const char *p = key;
	char *b = buf;
	while (*p && *p != '=') *b++ = *p++;
	*b++ = '=';
	p = value;
	while (*p) *b++ = *p++;
	*b++ = 0;

	//fprintf(stderr, "%s: buf: '%s'\n", __FUNCTION__, buf);

	// Delete any existing value.
	const char *s;
	while ((s = context_get_var_raw(context, key)) != NULL) {
		tdelete(buf, &context->env_tree, env_tree_compare);
		free((void *)s);
	}
	tsearch(buf, &context->env_tree, env_tree_compare);
}

